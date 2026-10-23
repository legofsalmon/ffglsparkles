#!/usr/bin/env bash
#
# Everything the macOS release ships, from built bundles to verified downloads.
# Both CI workflows call this, so a pull request's artifacts are built exactly
# the way a release's are.
#
#   package.sh <plugins-dir> <version> <out-dir>
#
# Produces in <out-dir>:
#   sparkles-macos-universal.zip   the bundle, INSTALL.txt and THIRD-PARTY.txt
#   sparkles-macos.pkg             the installer (signed and notarised only when
#                                  the signing secrets are present)
#
set -euo pipefail

PLUGINS="${1:?plugins directory required}"
VERSION="${2:?version required}"
OUT="${3:?output directory required}"

HERE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT="$( cd "$HERE/../.." && pwd )"
WORK="$( mktemp -d )"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

test -d "$PLUGINS/Sparkles.bundle" || { echo "error: no Sparkles.bundle in $PLUGINS" >&2; exit 1; }

# --- third-party notices --------------------------------------------------
# The FFGL SDK is BSD-style and requires its copyright notice to accompany a
# binary. GLEW is not listed: macOS links the system OpenGL and never pulls it in.
say "Writing third-party notices"
{
	echo "Sparkles links the following third-party component."
	echo
	printf '=%.0s' {1..72}; echo
	echo "FreeFrame / FFGL SDK"
	printf '=%.0s' {1..72}; echo
	echo
	cat "$ROOT/external/ffgl/LICENSE.md"
} > "$WORK/THIRD-PARTY.txt"

# --- the zip --------------------------------------------------------------
say "Building the zip"
mkdir -p "$WORK/zip"
cp -R "$PLUGINS/Sparkles.bundle" "$WORK/zip/"
cp "$ROOT/packaging/INSTALL-macos.txt" "$WORK/zip/INSTALL.txt"
cp "$WORK/THIRD-PARTY.txt" "$WORK/zip/"
# ditto rather than zip: it preserves the bundle structure. No --keepParent, or
# the staging directory's own name ends up as a folder inside the download.
rm -f "$OUT/sparkles-macos-universal.zip"
( cd "$WORK/zip" && ditto -c -k --sequesterRsrc . "$OUT/sparkles-macos-universal.zip" )

# Checking what actually ships, rather than what went into it: datamosh's first
# release had every file nested under the staging directory.
contents="$( unzip -Z1 "$OUT/sparkles-macos-universal.zip" )"
for expected in Sparkles.bundle/ INSTALL.txt THIRD-PARTY.txt; do
	echo "$contents" | grep -qE "^${expected}" || { echo "error: $expected is not at the root of the zip" >&2; exit 1; }
done
if echo "$contents" | grep -qE "^[^/]+/.*\.bundle/"; then
	echo "error: zip contents are nested inside a folder" >&2
	exit 1
fi
echo "zip layout ok"

# --- the installer --------------------------------------------------------
# The installer lays down only the bundle, so the notices go inside it, before
# signing seals its resources.
say "Building the installer"
mkdir -p "$WORK/payload"
cp -R "$PLUGINS/Sparkles.bundle" "$WORK/payload/"
mkdir -p "$WORK/payload/Sparkles.bundle/Contents/Resources"
cp "$WORK/THIRD-PARTY.txt" "$WORK/payload/Sparkles.bundle/Contents/Resources/THIRD-PARTY.txt"
rm -f "$OUT/sparkles-macos.pkg"
bash "$HERE/build-pkg.sh" "$WORK/payload" "$VERSION" "$OUT/sparkles-macos.pkg"

# --- verify the installer -------------------------------------------------
# Expanding the built package is the only way to see what a user will receive.
say "Verifying the installer payload"
# --expand-full unpacks each component's payload into <component>/Payload/.
pkgutil --expand-full "$OUT/sparkles-macos.pkg" "$WORK/expanded"
components=0
for component in "$WORK/expanded"/*.pkg; do
	[ -d "$component" ] || continue
	components=$(( components + 1 ))
	bundle="$component/Payload/Sparkles.bundle"
	test -f "$bundle/Contents/MacOS/Sparkles" || {
		echo "error: $(basename "$component") does not contain the plugin binary" >&2
		find "$component" -maxdepth 3 >&2
		exit 1; }
	test -f "$bundle/Contents/Resources/THIRD-PARTY.txt" || {
		echo "error: $(basename "$component") has no THIRD-PARTY.txt in the bundle" >&2; exit 1; }
	archs="$( lipo -archs "$bundle/Contents/MacOS/Sparkles" )"
	case "$archs" in
		*arm64*x86_64*|*x86_64*arm64*) ;;
		*) echo "error: $(basename "$component") is not universal: $archs" >&2; exit 1 ;;
	esac
	# And it goes where Resolume looks, not wherever Installer finds a copy.
	grep -q 'Extra Effects' "$component/PackageInfo" || {
		echo "error: $(basename "$component") does not install into Extra Effects" >&2; exit 1; }
	if grep -q 'relocate' "$component/PackageInfo"; then
		echo "error: $(basename "$component") is relocatable" >&2
		cat "$component/PackageInfo" >&2
		exit 1
	fi
	echo "$(basename "$component"): Sparkles.bundle ($archs), notices present, fixed location"
done
# One component per product, Arena and Avenue.
[ "$components" = "2" ] || { echo "error: expected 2 components, found $components" >&2; exit 1; }

# The whole point. Anything quarantined in the payload is restored on install,
# and the plugin silently never appears.
if xattr -lr "$WORK/expanded" | grep -q 'com.apple.quarantine'; then
	echo "error: installer payload carries com.apple.quarantine" >&2
	exit 1
fi
echo "installer ok, no quarantine"

ls -la "$OUT"
