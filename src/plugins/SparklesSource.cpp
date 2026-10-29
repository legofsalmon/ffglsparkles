#include "SparklesPlugin.h"
#include "Thumbnail.h"

#include <ffgl/FFGLPluginInfo.h>
#include <ffgl/FFGLThumbnailInfo.h>

// The class lives in the header so the tests can construct and drive it. This
// file holds only what must exist exactly once per binary.

static CFFGLThumbnailInfo ThumbnailInfo(
	sparkles::thumbnail::WIDTH,
	sparkles::thumbnail::HEIGHT,
	sparkles::thumbnail::Generate()
);

// PluginInfoStruct::PluginName is char[16] and is not null-terminated when
// full, so a name of 16 characters or more runs into the next field.
static constexpr char PLUGIN_NAME[] = "Sparkles";
static_assert( sizeof( PLUGIN_NAME ) <= 16,
               "FFGL PluginName is char[16] and is not null-terminated when full" );

static CFFGLPluginInfo PluginInfo(
	sparkles::GuardedFactory< sparkles::SparklesPlugin >,
	"SPKL",                  // unique ID, four characters
	PLUGIN_NAME,
	2,                       // FFGL API major
	1,                       // FFGL API minor
	SPARKLES_VERSION_MAJOR,  // plugin major
	SPARKLES_VERSION_MINOR,  // plugin minor
	FF_SOURCE,
	// The description has no length limit, so it carries the full version.
	"Fireworks, sparkles, confetti and glitter over a transparent background"
	" (v" SPARKLES_VERSION ")",
	"ffgl-sparkles " SPARKLES_VERSION
);
