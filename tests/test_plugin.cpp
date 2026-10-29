// The plugin driven the way Resolume drives it: by index, through the FFGL
// entry points, including the calls a host makes that our own code never would.

#include "harness/Readback.h"
#include "harness/TestRunner.h"

#include <SparklesPlugin.h>

#include <cstdio>
#include <memory>
#include <string>

using namespace sparkles;
using namespace sparkles::test;

namespace {

constexpr GLsizei W = 256;
constexpr GLsizei H = 144;

/// Exposes the protected bits a host reaches through other routes.
class TestPlugin : public SparklesPlugin
{
public:
	using SparklesPlugin::audioParam;
	using SparklesPlugin::OptionValue;
	using SparklesPlugin::ParamValue;
	using ffglqs::Plugin::UpdateAudioAndTime;
};

/// An initialised instance plus a host framebuffer, torn down in order.
struct Host
{
	std::unique_ptr< TestPlugin > plugin = std::make_unique< TestPlugin >();
	HostFramebuffer               fbo;
	double                        time = 0.0;

	explicit Host( GLsizei width = W, GLsizei height = H )
	{
		FFGLViewportStruct viewport = { 0, 0, static_cast< FFUInt32 >( width ), static_cast< FFUInt32 >( height ) };
		CHECK( plugin->InitGL( &viewport ) == FF_SUCCESS );
		CHECK( fbo.Allocate( width, height ) );
	}
	~Host()
	{
		plugin->DeInitGL();
		fbo.target.Release();
	}

	unsigned int Index( const char* name ) const { return plugin->ParamIndex( name ); }

	void Set( const char* name, float value )
	{
		const unsigned int index = Index( name );
		CHECK( index != SparklesPlugin::NO_PARAM );
		CHECK( plugin->SetFloatParameter( index, value ) == FF_SUCCESS );
	}

	/// One host frame: a new time, then a draw.
	FFResult Frame()
	{
		time += 1.0 / 60.0;
		plugin->SetTime( time );
		ProcessOpenGLStruct gl = {};
		gl.numInputTextures    = 0;
		gl.inputTextures       = nullptr;
		gl.HostFBO             = fbo.FBO();
		return plugin->ProcessOpenGL( &gl );
	}
};

}  // namespace

TEST( TheHostsDefaultWalkSucceedsForEveryParameter )
{
	// Resolume reads every parameter's default and writes it straight back
	// when it creates an instance. One FF_FAIL and the instance is destroyed
	// with nothing logged anywhere.
	TestPlugin plugin;
	const unsigned int count = plugin.GetNumParams();
	CHECK( count > 30 );
	bool allOk = true;
	for( unsigned int i = 0; i < count; ++i )
	{
		const float value = plugin.GetFloatParameter( i );
		allOk = allOk && plugin.SetFloatParameter( i, value ) == FF_SUCCESS;
		allOk = allOk && plugin.GetParameterDisplay( i ) != nullptr;
	}
	CHECK( allOk );
	CHECK( plugin.CaughtErrors() == 0 );
}

TEST( ParameterNamesAreUnique )
{
	TestPlugin plugin;
	bool unique = true;
	for( unsigned int i = 0; i < plugin.GetNumParams(); ++i )
		unique = unique && plugin.ParamIndex( plugin.GetParamName( i ) ) == i;
	CHECK( unique );
}

TEST( AFreshInstanceDrawsSomethingOnItsOwn )
{
	// A source that outputs nothing looks exactly like one that failed to load.
	Host host;
	for( int i = 0; i < 150; ++i )
		CHECK( host.Frame() == FF_SUCCESS );
	CHECK( Analyse( host.fbo.Read() ).lit > 0 );
	CHECK( host.plugin->CaughtErrors() == 0 );
}

TEST( APressAndReleaseBetweenFramesStillBursts )
{
	Host host;
	host.Set( "Effect", 1.0f );  // Bursts
	host.Set( "Auto", 0.0f );
	host.Frame();
	CHECK( host.plugin->System().BurstsFired() == 0 );

	host.Set( "Burst", 1.0f );
	host.Set( "Burst", 0.0f );
	host.Frame();
	CHECK( host.plugin->System().BurstsFired() == 1 );
	CHECK( host.plugin->System().Count() > 0 );

	// Held down across frames is still one press.
	host.Set( "Burst", 1.0f );
	host.Frame();
	host.Set( "Burst", 1.0f );
	host.Frame();
	CHECK( host.plugin->System().BurstsFired() == 2 );
}

TEST( ClearEmptiesTheScreen )
{
	Host host;
	host.Set( "Effect", 2.0f );  // Sparkles
	for( int i = 0; i < 30; ++i )
		host.Frame();
	CHECK( host.plugin->System().Count() > 0 );
	host.Set( "Auto", 0.0f );
	host.Set( "Clear", 1.0f );
	host.Frame();
	CHECK( host.plugin->System().Count() == 0 );
	CHECK( Analyse( host.fbo.Read() ).lit == 0 );
}

TEST( TheSameHostTimeDoesNotAdvanceTwice )
{
	Host host;
	host.Set( "Effect", 5.0f );  // Fountain, so something moves every frame
	for( int i = 0; i < 20; ++i )
		host.Frame();
	const size_t count = host.plugin->System().Count();
	const float  x     = host.plugin->System().Particles().front().x;

	// Drawn again for the same moment (the clip shown on a second layer).
	ProcessOpenGLStruct gl = {};
	gl.HostFBO             = host.fbo.FBO();
	CHECK( host.plugin->ProcessOpenGL( &gl ) == FF_SUCCESS );
	CHECK( host.plugin->System().Count() == count );
	CHECK( host.plugin->System().Particles().front().x == x );
}

TEST( AnOutOfRangeOptionLandsOnTheFirstEntry )
{
	// A composition saved by a later build with a longer list.
	TestPlugin plugin;
	const unsigned int effect = plugin.ParamIndex( "Effect" );
	CHECK( plugin.SetFloatParameter( effect, 99.0f ) == FF_SUCCESS );
	CHECK( plugin.OptionValue( "Effect" ) == 0 );
	CHECK( plugin.SetFloatParameter( effect, -3.0f ) == FF_SUCCESS );
	CHECK( plugin.OptionValue( "Effect" ) == 0 );
}

TEST( APresetWritesItsLookThroughTheHostPath )
{
	TestPlugin plugin;
	plugin.SetFloatParameter( plugin.ParamIndex( "Wind" ), 0.8f );
	plugin.SetFloatParameter( plugin.ParamIndex( "Preset" ), 5.0f );  // Victory Cannons

	const SparkleParams p = plugin.ReadParams();
	CHECK( p.effect == Effect::ConfettiCannon );
	CHECK( p.palette == Palette::TwoTone );
	CHECK_NEAR( p.rate, 0.4f, 1e-6 );
	// Not part of the preset, so back to its default rather than inherited.
	CHECK_NEAR( p.wind, 0.0f, 1e-6 );

	// A later manual change is kept, and the preset is not re-applied over it.
	plugin.SetFloatParameter( plugin.ParamIndex( "Rate" ), 2.0f );
	plugin.SetFloatParameter( plugin.ParamIndex( "Preset" ), 5.0f );
	CHECK_NEAR( plugin.ReadParams().rate, 2.0f, 1e-6 );
}

TEST( EveryPresetIsDistinct )
{
	TestPlugin plugin;
	const unsigned int preset = plugin.ParamIndex( "Preset" );
	std::vector< SparkleParams > looks;
	for( int i = 1; i <= 8; ++i )
	{
		plugin.SetFloatParameter( preset, static_cast< float >( i ) );
		looks.push_back( plugin.ReadParams() );
	}
	bool distinct = true;
	for( size_t a = 0; a < looks.size(); ++a )
		for( size_t b = a + 1; b < looks.size(); ++b )
			distinct = distinct && !( looks[ a ].effect == looks[ b ].effect && looks[ a ].shape == looks[ b ].shape &&
			                          looks[ a ].palette == looks[ b ].palette );
	CHECK( distinct );
}

TEST( ParametersMapToTheRightFields )
{
	// Each slider set to a distinctive value, read back through ReadParams:
	// catches a name typo that would silently pin a field to zero.
	TestPlugin plugin;
	struct Case { const char* name; float value; float SparkleParams::*field; };
	const Case cases[] = {
		{ "Rate", 3.5f, &SparkleParams::rate },             { "Density", 0.77f, &SparkleParams::density },
		{ "Position X", 0.21f, &SparkleParams::positionX }, { "Position Y", 0.31f, &SparkleParams::positionY },
		{ "Spread", 0.41f, &SparkleParams::spread },        { "Height", 0.51f, &SparkleParams::height },
		{ "Direction", 61.0f, &SparkleParams::direction },  { "Colour Variety", 0.71f, &SparkleParams::colourVariety },
		{ "Speed", 1.7f, &SparkleParams::speed },           { "Gravity", 2.1f, &SparkleParams::gravity },
		{ "Drag", 0.3f, &SparkleParams::drag },             { "Wind", -0.4f, &SparkleParams::wind },
		{ "Turbulence", 0.6f, &SparkleParams::turbulence }, { "Lifetime", 2.5f, &SparkleParams::lifetime },
		{ "Time Scale", 0.5f, &SparkleParams::timeScale },  { "Size", 2.2f, &SparkleParams::size },
		{ "Size Variation", 0.9f, &SparkleParams::sizeVariation }, { "Glow", 0.12f, &SparkleParams::glow },
		{ "Streak", 0.13f, &SparkleParams::streak },        { "Trail", 0.14f, &SparkleParams::trail },
		{ "Twinkle", 0.15f, &SparkleParams::twinkle },      { "Brightness", 1.9f, &SparkleParams::brightness },
	};
	for( const Case& c : cases )
	{
		CHECK( plugin.SetFloatParameter( plugin.ParamIndex( c.name ), c.value ) == FF_SUCCESS );
		const SparkleParams p = plugin.ReadParams();
		if( std::fabs( p.*( c.field ) - c.value ) > 1e-5f )
			Registry::Instance().Fail( std::string( "  " ) + c.name + " did not reach its field" );
	}

	plugin.SetFloatParameter( plugin.ParamIndex( "Max Particles" ), 4.0f );
	CHECK( plugin.ReadParams().maxParticles == 80000 );
	plugin.SetFloatParameter( plugin.ParamIndex( "Background" ), 1.0f );
	CHECK( plugin.ReadParams().blackBackground );
	plugin.SetFloatParameter( plugin.ParamIndex( "Sync" ), 3.0f );
	CHECK( plugin.SyncBeats() == 4 );

	// Colour A: hue 1/3, full saturation and brightness is pure green.
	const unsigned int colourA = plugin.ParamIndex( "Colour A" );
	plugin.SetFloatParameter( colourA, 1.0f / 3.0f );
	plugin.SetFloatParameter( colourA + 1, 1.0f );
	plugin.SetFloatParameter( colourA + 2, 1.0f );
	const RGB green = plugin.ReadParams().colourA;
	CHECK( green.g > 0.99f && green.r < 0.01f && green.b < 0.01f );
}

TEST( TheAudioTriggerHearsHostFFTData )
{
	// ffglqs::Audio starts at zero gain; without the fix every level reads
	// zero and the trigger can never fire, with no other symptom.
	TestPlugin plugin;
	CHECK( plugin.audioParam != nullptr );
	const size_t bins = plugin.audioParam->fftData.size();
	for( size_t bin = 0; bin < bins; ++bin )
		plugin.SetParamElementValue( plugin.audioParam->index, static_cast< unsigned int >( bin ),
		                             bin < bins / 4 ? 1.0f : 0.0f );
	for( int frame = 0; frame < 32; ++frame )
		plugin.UpdateAudioAndTime();
	CHECK( plugin.AudioLevel() > 0.0f );
}

TEST( BeatSyncFiresOnTheHostsBeat )
{
	Host host;
	host.Set( "Effect", 1.0f );  // Bursts
	host.Set( "Sync", 1.0f );    // Every Beat
	// A slow Rate would fire nothing in two seconds if Sync were ignored...
	host.Set( "Rate", 0.05f );
	for( int i = 0; i < 120; ++i )
	{
		// ...and 120 bpm is two beats a second: a bar every two seconds.
		host.plugin->SetBeatInfo( 120.0f, static_cast< float >( ( i % 120 ) / 120.0 ) );
		host.Frame();
	}
	// Four beats in the bar; the first is the reference, so three edges.
	CHECK( host.plugin->System().BurstsFired() >= 3 );
	CHECK( host.plugin->System().BurstsFired() <= 4 );
}

namespace sparkles::test {

/// `sparkles_tests --snapshots <dir>`: renders each preset through the plugin
/// and writes the host framebuffer as raw RGBA (bottom row first), for a human
/// to look at. Not a test: nothing here can tell a good-looking firework from
/// a bad one.
void RunSnapshots( const char* directory )
{
	static const char* const names[] = { "custom", "new-year", "gold-willows", "wedding-glitter", "birthday-confetti",
	                                     "victory-cannons", "starry-night", "sparkler-fountain", "love-hearts" };
	for( int preset = 0; preset <= 8; ++preset )
	{
		Host host( 640, 360 );
		host.Set( "Preset", static_cast< float >( preset ) );
		host.Set( "Seed", 7.0f );
		for( int i = 0; i < 150; ++i )
			host.Frame();
		const std::vector< std::uint8_t > pixels = host.fbo.Read();
		const std::string path = std::string( directory ) + "/" + names[ preset ] + ".rgba";
		if( FILE* f = std::fopen( path.c_str(), "wb" ) )
		{
			std::fwrite( pixels.data(), 1, pixels.size(), f );
			std::fclose( f );
			std::printf( "%s (640x360)\n", path.c_str() );
		}
	}
}

}  // namespace sparkles::test
