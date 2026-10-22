# Notes for Claude

An FFGL 2.x **source** plugin for Resolume (Windows and macOS) that renders
celebration particles over a transparent background. One binary:

| Binary | Type, ID | Where it shows up |
| --- | --- | --- |
| `Sparkles` | `FF_SOURCE`, `SPKL` | Sources tab, with the generators |

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
ctest --test-dir build --output-on-failure        # headless EGL + llvmpipe
./build/tests/sparkles_tests --profile             # timing at 1080p
./build/tests/sparkles_tests --snapshots <dir>     # raw RGBA frames of every preset, to look at
```

The build system, test harness and host-boundary guards are adapted from
`legofsalmon/ffgl-datamosh`, and so are most of the traps below. Read that
repo's CLAUDE.md if something here is unclear.

## Layout

- `src/core/ParticleSystem.*`: CPU simulation. Deterministic for a seed.
- `src/core/ParticleRenderer.*`: instanced quads into a half-float target,
  trail merge, premultiplied composite. Shaders in `src/core/shaders/` are
  embedded into the binary at build time.
- `src/plugins/SparklesPlugin.h`: parameters, presets, triggers, guards. In a
  header so the tests can drive it; `SparklesSource.cpp` only registers it.

## Traps

**Parameters are addressed by index.** The host and saved compositions store
parameters by position, and dropdowns store the option's position. Append new
parameters at the end of the constructor and new options at the end of their
list. Never insert, reorder or rename.

**The FFGL SDK must be built as a CMake `OBJECT` library.** `plugMain` lives in a
translation unit nothing else references, so out of a static archive the linker
drops it and the plugin loads nowhere, silently. CI checks the export.

**`PluginInfoStruct::PluginName` is `char[16]`**, not null-terminated when full.
Fifteen characters is the limit; there is a `static_assert`.

**OpenGL 4.1 core is the ceiling** (Apple's). No compute, no SSBOs, no base
instance: `PointInstanceAttributes` moves the attribute offsets instead.
`half` is a reserved word in GLSL 4.10; the tests caught it once.

**Resolume expects premultiplied colour in range.** `Composite.glsl` raises
alpha to cover the brightest channel and clamps. `FireworksReachTheHostAsPremultipliedOverlay` pins it.

**Trails are merged with `max`, not added** (`Trail.glsl`). Additive feedback
burns anything slow-moving out to a white blob. The `- 1/512` there is what
lets a half-float trail reach exactly zero; without it a transparent overlay
keeps a permanent haze. `TheTrailFadesAllTheWayToZero` pins it.

**Leave GL state as the host gave it**: blending off and `ONE, ZERO`, the
host's FBO, viewport, program and VAO. `HostStateIsLeftAsFound` checks.

**`ffglqs::Audio` starts at zero gain.** Without `SetGain( 0.0f )` (decibels,
so unity) every band reads zero and Audio Trigger is silently inert.
`TheAudioTriggerHearsHostFFTData` pins it.

**`ParamOption::SetValue` reads past the end on an out-of-range value.** The
plugin clamps before the SDK sees it.

**Buttons are latched in `SetFloatParameter`**, on the press, because a press
and release can both land between two frames. Triggers are consumed only on a
frame that actually advanced the simulation.

**Advance once per host time.** Resolume may render a source twice for the same
moment; the simulation steps only when `SetTime` moves, using the host's delta
when it is sane and wall-clock time otherwise.

**No exception may cross into the host.** Every override is wrapped in
`SPARKLES_CATCH_ALL` and instances come from `GuardedFactory`. A new override
needs the same.

**`ffglex::FFGLFBO` is not used**: its `Release()` leaks the colour texture.
`RenderTarget` replaces it, and `ScopedViewport` restores the viewport that
`ScopedFBOBinding` does not.

## Testing

When you fix a silent bug, reintroduce it and confirm the new test fails. The
premultiply, trail-to-zero and audio-gain tests were each checked that way.
Nothing headless can say whether a firework *looks* good: render
`--snapshots` and look at them.

## Conventions

Work on a branch, open a PR, let CI go green on all three jobs, then merge.
