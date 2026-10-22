#pragma once

#include <ParticleRenderer.h>
#include <ParticleSystem.h>
#include <SparkleParams.h>

#include <ffgl/FFGLLog.h>
#include <ffglquickstart/FFGLParamBool.h>
#include <ffglquickstart/FFGLParamFFT.h>
#include <ffglquickstart/FFGLParamOption.h>
#include <ffglquickstart/FFGLParamRange.h>
#include <ffglquickstart/FFGLParamTrigger.h>
#include <ffglquickstart/FFGLSource.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sparkles {

/// The catch-all every host entry point ends with. A macro only because a
/// function cannot supply a try block's handlers; NoteCaught does the work.
#define SPARKLES_CATCH_ALL( WHERE )                                \
	catch( const std::exception& caughtError )                     \
	{                                                              \
		this->NoteCaught( WHERE, caughtError.what() );             \
	}                                                              \
	catch( ... )                                                   \
	{                                                              \
		this->NoteCaught( WHERE, "an exception of unknown type" ); \
	}

/// The instance factory handed to CFFGLPluginInfo, in place of the SDK's
/// PluginFactory<T>, which is a bare `new T()` called through a C ABI. A
/// std::bad_alloc unwinding into Resolume takes the host down with the show in
/// it; here it is one log line and an FF_FAIL.
template< typename PluginType >
FFResult __stdcall GuardedFactory( CFFGLPlugin** ppOutInstance )
{
	if( ppOutInstance == nullptr )
		return FF_FAIL;
	*ppOutInstance = nullptr;
	try
	{
		*ppOutInstance = new PluginType();
		return FF_SUCCESS;
	}
	catch( ... )
	{
		FFGLLog::LogToHost( "sparkles: could not create an instance (an exception was caught; out of memory?)" );
		return FF_FAIL;
	}
}

/// Option lists. The stored value is the position, and a saved composition
/// stores the position, so every list is appended to and never reordered.
namespace options {

inline std::vector< ffglqs::ParamOption::Option > Effects()
{
	return { { "Fireworks", 0 }, { "Bursts", 1 }, { "Sparkles", 2 }, { "Confetti Rain", 3 },
	         { "Confetti Cannon", 4 }, { "Fountain", 5 }, { "Glitter Rain", 6 } };
}

inline std::vector< ffglqs::ParamOption::Option > Shapes()
{
	return { { "Peony", 0 }, { "Ring", 1 }, { "Willow", 2 }, { "Palm", 3 },
	         { "Heart", 4 }, { "Star", 5 }, { "Crackle", 6 }, { "Random", 7 } };
}

inline std::vector< ffglqs::ParamOption::Option > Palettes()
{
	return { { "Rainbow", 0 }, { "Gold", 1 }, { "Silver", 2 }, { "Fire", 3 }, { "Ice", 4 },
	         { "Pastel", 5 }, { "Neon", 6 }, { "Colour A + B", 7 }, { "Colour A", 8 } };
}

/// Beats per emission. 0 is "free running on Rate".
inline std::vector< ffglqs::ParamOption::Option > Syncs()
{
	return { { "Free (Rate)", 0 }, { "Every Beat", 1 }, { "Every 2 Beats", 2 },
	         { "Every Bar", 4 }, { "Every 2 Bars", 8 }, { "Every 4 Bars", 16 } };
}

inline std::vector< ffglqs::ParamOption::Option > MaxParticles()
{
	return { { "5,000", 5000 }, { "10,000", 10000 }, { "20,000", 20000 }, { "40,000", 40000 }, { "80,000", 80000 } };
}

inline std::vector< ffglqs::ParamOption::Option > Backgrounds()
{
	return { { "Transparent", 0 }, { "Black", 1 } };
}

inline std::vector< ffglqs::ParamOption::Option > Presets()
{
	return { { "Custom", 0 }, { "New Year", 1 }, { "Gold Willows", 2 }, { "Wedding Glitter", 3 },
	         { "Birthday Confetti", 4 }, { "Victory Cannons", 5 }, { "Starry Night", 6 },
	         { "Sparkler Fountain", 7 }, { "Love Hearts", 8 } };
}

}  // namespace options

/// The plugin: a Resolume source that renders celebration particles over a
/// transparent background.
///
/// Lives in a header so the tests can construct it and drive it through the
/// same entry points the host uses. SparklesSource.cpp holds only the
/// once-per-binary registration.
class SparklesPlugin : public ffglqs::Source
{
public:
	static constexpr unsigned int NO_PARAM = 0xFFFFFFFFu;

	SparklesPlugin()
	{
		// --- one click to a look ---
		// First, so a saved composition restores it before everything it
		// writes: the preset lays the baseline, then the saved values land on top.
		AddGrouped( "Preset", ffglqs::ParamOption::Create( "Preset", options::Presets(), 0 ) );

		// --- what and when ---
		AddGrouped( "Emitter", ffglqs::ParamOption::Create( "Effect", options::Effects(), 0 ) );
		AddGrouped( "Emitter", ffglqs::ParamOption::Create( "Shape", options::Shapes(), 7 ) );
		AddGrouped( "Emitter", ffglqs::ParamBool::Create( "Auto", true ) );
		AddGrouped( "Emitter", ffglqs::ParamRange::Create( "Rate", 1.0f, ffglqs::ParamRange::Range( 0.05f, 10.0f ) ) );
		AddGrouped( "Emitter", ffglqs::ParamRange::Create( "Density", 0.5f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Emitter", ffglqs::ParamTrigger::Create( "Burst" ) );
		AddGrouped( "Emitter", ffglqs::ParamOption::Create( "Sync", options::Syncs(), 0 ) );

		// --- where ---
		AddGrouped( "Position", ffglqs::ParamRange::Create( "Position X", 0.5f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Position", ffglqs::ParamRange::Create( "Position Y", 0.5f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Position", ffglqs::ParamRange::Create( "Spread", 0.6f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Position", ffglqs::ParamRange::Create( "Height", 0.7f, ffglqs::ParamRange::Range( 0.1f, 1.0f ) ) );
		AddGrouped( "Position", ffglqs::ParamRange::Create( "Direction", 90.0f, ffglqs::ParamRange::Range( 0.0f, 180.0f ) ) );

		// --- colour ---
		AddGrouped( "Colour", ffglqs::ParamOption::Create( "Palette", options::Palettes(), 0 ) );
		AddHueColour( "Colour A", 0.11f, 0.8f );  // warm gold
		AddHueColour( "Colour B", 0.58f, 0.7f );  // sky blue
		AddGrouped( "Colour", ffglqs::ParamRange::Create( "Colour Variety", 0.3f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );

		// --- how it moves ---
		AddGrouped( "Motion", ffglqs::ParamRange::Create( "Speed", 1.0f, ffglqs::ParamRange::Range( 0.1f, 3.0f ) ) );
		AddGrouped( "Motion", ffglqs::ParamRange::Create( "Gravity", 1.0f, ffglqs::ParamRange::Range( -1.0f, 4.0f ) ) );
		AddGrouped( "Motion", ffglqs::ParamRange::Create( "Drag", 1.0f, ffglqs::ParamRange::Range( 0.0f, 4.0f ) ) );
		AddGrouped( "Motion", ffglqs::ParamRange::Create( "Wind", 0.0f, ffglqs::ParamRange::Range( -1.0f, 1.0f ) ) );
		AddGrouped( "Motion", ffglqs::ParamRange::Create( "Turbulence", 0.0f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Motion", ffglqs::ParamRange::Create( "Lifetime", 1.0f, ffglqs::ParamRange::Range( 0.2f, 4.0f ) ) );
		AddGrouped( "Motion", ffglqs::ParamRange::Create( "Time Scale", 1.0f, ffglqs::ParamRange::Range( 0.0f, 3.0f ) ) );

		// --- how it looks ---
		AddGrouped( "Look", ffglqs::ParamRange::Create( "Size", 1.0f, ffglqs::ParamRange::Range( 0.1f, 5.0f ) ) );
		AddGrouped( "Look", ffglqs::ParamRange::Create( "Size Variation", 0.4f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Look", ffglqs::ParamRange::Create( "Glow", 0.5f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Look", ffglqs::ParamRange::Create( "Streak", 0.3f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Look", ffglqs::ParamRange::Create( "Trail", 0.4f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Look", ffglqs::ParamRange::Create( "Twinkle", 0.4f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );
		AddGrouped( "Look", ffglqs::ParamRange::Create( "Brightness", 1.0f, ffglqs::ParamRange::Range( 0.0f, 3.0f ) ) );

		// --- audio ---
		audioParam = ffglqs::ParamFFT::Create( "Audio" );
		AddGrouped( "Audio", audioParam );
		// ffglqs::Audio starts with gain 0 and every bin is fft * fft * gain, so
		// without this every band reads zero forever and Audio Trigger is inert
		// with no other symptom. The argument is decibels: 0 dB is unity.
		this->audioParams[ audioParam ].SetGain( 0.0f );
		AddGrouped( "Audio", ffglqs::ParamRange::Create( "Audio Trigger", 0.0f, ffglqs::ParamRange::Range( 0.0f, 1.0f ) ) );

		// --- output ---
		AddGrouped( "Output", ffglqs::ParamOption::Create( "Background", options::Backgrounds(), 0 ) );
		AddGrouped( "Output", ffglqs::ParamOption::Create( "Max Particles", options::MaxParticles(), 2 ) );
		AddGrouped( "Output", ffglqs::ParamRange::CreateInteger( "Seed", 1, ffglqs::ParamRange::Range( 0.0f, 999.0f ) ) );
		AddGrouped( "Output", ffglqs::ParamTrigger::Create( "Clear" ) );
		// Anything new is appended below here, never inserted above: the host
		// and saved compositions address parameters by index.

		presetIndex    = ParamIndex( "Preset" );
		effectIndex    = ParamIndex( "Effect" );
		burstIndex     = ParamIndex( "Burst" );
		clearIndex     = ParamIndex( "Clear" );
		seedIndex      = ParamIndex( "Seed" );

		for( const auto& param : this->params )
			defaults[ param->GetName() ] = param->GetValue();

		system.Reseed( 1 );
		ApplyEffectVisibility( false );
	}

	~SparklesPlugin() override = default;

	// Every host entry point is guarded: an exception unwinding into Resolume
	// through plugMain's C boundary is undefined behaviour and in practice ends
	// the show.

	FFResult InitGL( const FFGLViewportStruct* viewPort ) override
	{
		try
		{
			// Deliberately not ffglqs::Plugin::InitGL: that compiles a shader
			// synthesised from parameter names, which this plugin never draws
			// with, and which would require every name to be a GLSL identifier.
			if( !renderer.Initialise() )
			{
				FFGLLog::LogToHost( "sparkles: renderer failed to initialise" );
				return FF_FAIL;
			}
			return CFFGLPlugin::InitGL( viewPort );
		}
		SPARKLES_CATCH_ALL( "InitGL" )
		try
		{
			renderer.Release();
		}
		catch( ... )
		{
		}
		return FF_FAIL;
	}

	FFResult DeInitGL() override
	{
		try
		{
			renderer.Release();
		}
		SPARKLES_CATCH_ALL( "DeInitGL" )
		return FF_SUCCESS;
	}

	FFResult SetTime( double time ) override
	{
		try
		{
			hostTimeValid = true;
			return ffglqs::Source::SetTime( time );
		}
		SPARKLES_CATCH_ALL( "SetTime" )
		return FF_FAIL;
	}

	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override
	{
		if( pGL == nullptr )
			return FF_FAIL;
		try
		{
			return RenderFrame( pGL->HostFBO );
		}
		SPARKLES_CATCH_ALL( "a frame" )
		// The next frame starts clean rather than building on whatever was
		// half written.
		try
		{
			renderer.ClearTrail();
		}
		catch( ... )
		{
		}
		return FF_FAIL;
	}

	FFResult SetFloatParameter( unsigned int index, float value ) override
	{
		try
		{
			return SetFloatParameterUnguarded( index, value );
		}
		SPARKLES_CATCH_ALL( "SetFloatParameter" )
		return FF_FAIL;
	}

	float GetFloatParameter( unsigned int index ) override
	{
		try
		{
			return ffglqs::Source::GetFloatParameter( index );
		}
		SPARKLES_CATCH_ALL( "GetFloatParameter" )
		return 0.0f;
	}

	char* GetParameterDisplay( unsigned int index ) override
	{
		// The SDK indexes before it bounds-checks, and answers an invalid
		// index with FF_FAIL cast to a pointer. Neither is safe to hand back.
		static char empty[ 1 ] = { '\0' };
		if( index >= this->params.size() )
			return empty;
		try
		{
			char* text = ffglqs::Source::GetParameterDisplay( index );
			return text == reinterpret_cast< char* >( FF_FAIL ) ? empty : text;
		}
		SPARKLES_CATCH_ALL( "GetParameterDisplay" )
		return empty;
	}

	void SetBeatInfo( float newBpm, float newBarPhase ) override
	{
		try
		{
			ffglqs::Source::SetBeatInfo( newBpm, newBarPhase );
		}
		SPARKLES_CATCH_ALL( "SetBeatInfo" )
	}

	void SetHostInfo( const char* hostname, const char* version ) override
	{
		try
		{
			// The SDK assigns both into std::strings; a null there is undefined
			// behaviour, not an exception.
			ffglqs::Source::SetHostInfo( hostname ? hostname : "", version ? version : "" );
		}
		SPARKLES_CATCH_ALL( "SetHostInfo" )
	}

	void SetSampleRate( unsigned int sampleRate ) override
	{
		try
		{
			ffglqs::Source::SetSampleRate( sampleRate );
		}
		SPARKLES_CATCH_ALL( "SetSampleRate" )
	}

	// --- for tests and diagnostics ---

	unsigned int CaughtErrors() const { return caughtErrors; }
	const ParticleSystem& System() const { return system; }
	const ParticleRenderer& Renderer() const { return renderer; }

	/// Index of a parameter by name, or NO_PARAM.
	unsigned int ParamIndex( const std::string& name ) const
	{
		for( size_t index = 0; index < this->params.size(); ++index )
			if( this->params[ index ]->GetName() == name )
				return static_cast< unsigned int >( index );
		return NO_PARAM;
	}

	/// Every parameter as the core sees it.
	SparkleParams ReadParams() const
	{
		SparkleParams p;
		p.effect        = static_cast< Effect >( OptionValue( "Effect" ) );
		p.shape         = static_cast< Shape >( OptionValue( "Shape" ) );
		p.palette       = static_cast< Palette >( OptionValue( "Palette" ) );
		p.colourA       = HueColour( "Colour A" );
		p.colourB       = HueColour( "Colour B" );
		p.colourVariety = ParamValue( "Colour Variety" );

		p.autoEmit  = ParamValue( "Auto" ) > 0.5f;
		p.rate      = ParamValue( "Rate" );
		p.density   = ParamValue( "Density" );
		p.positionX = ParamValue( "Position X" );
		p.positionY = ParamValue( "Position Y" );
		p.spread    = ParamValue( "Spread" );
		p.height    = ParamValue( "Height" );
		p.direction = ParamValue( "Direction" );

		p.speed      = ParamValue( "Speed" );
		p.gravity    = ParamValue( "Gravity" );
		p.drag       = ParamValue( "Drag" );
		p.wind       = ParamValue( "Wind" );
		p.turbulence = ParamValue( "Turbulence" );
		p.lifetime   = ParamValue( "Lifetime" );
		p.timeScale  = ParamValue( "Time Scale" );

		p.size          = ParamValue( "Size" );
		p.sizeVariation = ParamValue( "Size Variation" );
		p.glow          = ParamValue( "Glow" );
		p.streak        = ParamValue( "Streak" );
		p.trail         = ParamValue( "Trail" );
		p.twinkle       = ParamValue( "Twinkle" );
		p.brightness    = ParamValue( "Brightness" );

		p.blackBackground = OptionValue( "Background" ) == 1;
		p.maxParticles    = OptionValue( "Max Particles" );
		return p;
	}

	/// Beats per emission from the Sync option, or 0 for free running.
	int SyncBeats() const { return OptionValue( "Sync" ); }

	/// The bass level the audio trigger listens to. Its own method so a test
	/// can reach it: this value never passes through ReadParams.
	float AudioLevel()
	{
		if( !audioParam )
			return 0.0f;
		return this->audioParams[ audioParam ].GetBass();
	}

protected:
	FFResult RenderFrame( GLuint hostFBO )
	{
		// Refreshes the FFT buffers and the wall-clock delta. Normally called
		// by the quickstart ProcessOpenGL, which this replaces.
		this->UpdateAudioAndTime();

		const GLsizei width  = static_cast< GLsizei >( this->currentViewport.width );
		const GLsizei height = static_cast< GLsizei >( this->currentViewport.height );
		if( width <= 0 || height <= 0 || !renderer.IsInitialised() )
			return FF_FAIL;

		SparkleParams params = ReadParams();

		// Advance at most once per host frame. Resolume can render a source
		// more than once for the same moment (a clip on two layers, a preview),
		// and a simulation advanced on every call would run faster the more
		// places it is shown. The gate only engages once the host has actually
		// delivered a time: a wrong rate is recoverable, a frozen effect is not.
		const bool timeMoved = !hostTimeValid || this->hostTime != lastHostTime;

		// The host's clock when it moves forward by a sane amount, so the show
		// follows the clip's own speed and the frame pacing of the output.
		// Wall-clock time otherwise: no host time, a clip that looped or was
		// scrubbed backwards, or a stall.
		const double hostDelta = this->hostTime - lastHostTime;
		float        dt        = std::min( 0.1f, std::max( 1.0f / 240.0f, this->deltaTime ) );
		if( hostTimeValid && lastHostTimeValid && hostDelta > 0.0 && hostDelta <= 0.25 )
			dt = static_cast< float >( hostDelta );
		lastHostTime      = this->hostTime;
		lastHostTimeValid = hostTimeValid;

		if( timeMoved )
		{
			const float step = dt;

			EmitEvents events;
			events.bursts  = pendingBursts;
			events.clear   = pendingClear;
			pendingBursts  = 0;

			const int  syncBeats = SyncBeats();
			const bool synced    = syncBeats > 0 && this->bpm > 0.0f;
			if( synced )
			{
				// Beat-locked: the clock is the host's, not Rate.
				params.rate = 0.0f;
				if( BeatElapsed( syncBeats ) && params.autoEmit )
					++events.bursts;
			}
			if( AudioHit( step ) )
				++events.bursts;

			system.SetAspect( static_cast< float >( width ) / static_cast< float >( height ) );
			system.Step( params, step, events );
			if( pendingClear )
			{
				renderer.ClearTrail();
				pendingClear = false;
			}
			lastStep = step;

			// One-shot buttons are consumed only on a call that read them.
			this->consumeAllTrigger();
		}

		const size_t confetti = system.BuildInstances( params, instances );
		if( !renderer.Render( hostFBO, width, height, instances, confetti, params, timeMoved ? lastStep : 0.0f ) )
			return FF_FAIL;
		return FF_SUCCESS;
	}

	FFResult SetFloatParameterUnguarded( unsigned int index, float value )
	{
		if( index >= this->params.size() )
			return FF_FAIL;

		// Clamp an option to its own range BEFORE the SDK sees it.
		// ParamOption::SetValue clamps its stored value when the index is out
		// of range but then indexes `options` with the unclamped one, reading
		// past the end of the vector. A composition saved by a later build with
		// more entries in a dropdown reaches exactly that. NaN fails `>= 0` and
		// lands on 0 too, rather than being cast to size_t.
		if( const auto option = std::dynamic_pointer_cast< ffglqs::ParamOption >( this->params[ index ] ) )
		{
			const size_t count = option->options.size();
			if( count == 0 )
				return FF_FAIL;
			if( !( value >= 0.0f ) || value > static_cast< float >( count - 1 ) )
				value = 0.0f;
		}

		// Buttons are latched on the press, here, rather than read at render
		// time: a press and release that both land between two frames would
		// otherwise never be seen.
		const bool down = value > 0.5f;
		if( index == burstIndex )
		{
			if( down && !burstDown )
				++pendingBursts;
			burstDown = down;
		}
		else if( index == clearIndex )
		{
			if( down && !clearDown )
				pendingClear = true;
			clearDown = down;
		}

		const FFResult result = ffglqs::Source::SetFloatParameter( index, value );

		if( index == seedIndex )
			system.Reseed( static_cast< std::uint32_t >( std::max( 0.0f, ParamValue( "Seed" ) ) ) );
		else if( index == effectIndex )
			ApplyEffectVisibility( true );
		else if( index == presetIndex && !applyingPreset )
		{
			const int preset = OptionValue( "Preset" );
			if( preset != lastPreset && preset != 0 )
				ApplyPreset( preset );
			lastPreset = preset;
		}
		return result;
	}

	/// Writes a whole coherent look. Every look-and-motion parameter goes back
	/// to its default first, so a preset never inherits half of the last one.
	void ApplyPreset( int preset )
	{
		std::map< std::string, float > v;
		switch( preset )
		{
		case 1:  // New Year
			v = { { "Effect", 0 }, { "Shape", 7 }, { "Palette", 0 }, { "Rate", 1.6f }, { "Density", 0.6f },
			      { "Spread", 0.85f }, { "Height", 0.72f }, { "Glow", 0.6f }, { "Trail", 0.45f }, { "Twinkle", 0.5f } };
			break;
		case 2:  // Gold Willows
			v = { { "Effect", 0 }, { "Shape", 2 }, { "Palette", 1 }, { "Rate", 0.6f }, { "Density", 0.5f },
			      { "Height", 0.82f }, { "Trail", 0.65f }, { "Glow", 0.55f }, { "Colour Variety", 0.15f } };
			break;
		case 3:  // Wedding Glitter
			v = { { "Effect", 6 }, { "Palette", 1 }, { "Density", 0.45f }, { "Spread", 1.0f }, { "Glow", 0.7f },
			      { "Twinkle", 0.8f }, { "Trail", 0.25f }, { "Streak", 0.15f } };
			break;
		case 4:  // Birthday Confetti
			v = { { "Effect", 3 }, { "Palette", 0 }, { "Density", 0.5f }, { "Spread", 1.0f }, { "Size", 1.2f },
			      { "Trail", 0.0f }, { "Glow", 0.0f } };
			break;
		case 5:  // Victory Cannons
			v = { { "Effect", 4 }, { "Palette", 7 }, { "Rate", 0.4f }, { "Density", 0.7f }, { "Spread", 0.5f },
			      { "Trail", 0.0f }, { "Glow", 0.0f }, { "Size", 1.1f } };
			break;
		case 6:  // Starry Night
			v = { { "Effect", 2 }, { "Palette", 2 }, { "Density", 0.35f }, { "Spread", 1.0f }, { "Glow", 0.6f },
			      { "Twinkle", 0.9f }, { "Trail", 0.0f }, { "Size", 1.2f } };
			break;
		case 7:  // Sparkler Fountain
			v = { { "Effect", 5 }, { "Palette", 1 }, { "Position Y", 0.05f }, { "Direction", 90.0f },
			      { "Spread", 0.3f }, { "Density", 0.6f }, { "Trail", 0.3f }, { "Streak", 0.5f }, { "Glow", 0.6f } };
			break;
		case 8:  // Love Hearts
			v = { { "Effect", 1 }, { "Shape", 4 }, { "Palette", 7 }, { "Rate", 0.8f }, { "Density", 0.6f },
			      { "Spread", 0.8f }, { "Gravity", 0.4f }, { "Trail", 0.35f } };
			break;
		default:
			return;
		}

		static const char* const managed[] = {
			"Effect", "Shape", "Auto", "Rate", "Density", "Sync",
			"Position X", "Position Y", "Spread", "Height", "Direction",
			"Palette", "Colour Variety",
			"Speed", "Gravity", "Drag", "Wind", "Turbulence", "Lifetime", "Time Scale",
			"Size", "Size Variation", "Glow", "Streak", "Trail", "Twinkle", "Brightness",
		};

		applyingPreset = true;
		for( const char* name : managed )
		{
			const auto found = v.find( name );
			PushParam( name, found != v.end() ? found->second : defaults[ name ] );
		}
		// Colour A + B presets bring their own pair; the rest leave the
		// pickers alone, since they are the operator's.
		if( preset == 5 )
		{
			PushHueColour( "Colour A", 0.0f, 0.0f );   // white
			PushHueColour( "Colour B", 0.11f, 0.85f ); // gold
		}
		else if( preset == 8 )
		{
			PushHueColour( "Colour A", 0.95f, 0.65f );  // pink
			PushHueColour( "Colour B", 0.0f, 0.9f );    // red
		}
		applyingPreset = false;
		ApplyEffectVisibility( true );
	}

	/// Shows only the controls that do something for the current effect.
	/// SetParamVisibility edits the record in place and the SDK notes that
	/// Resolume honours it; a host that ignores it simply shows everything.
	void ApplyEffectVisibility( bool raiseEvent )
	{
		const Effect effect      = static_cast< Effect >( OptionValue( "Effect" ) );
		const bool   explodes    = effect == Effect::Fireworks || effect == Effect::Bursts;
		const bool   burstTimed  = explodes || effect == Effect::ConfettiCannon;
		const bool   usesY       = effect == Effect::Bursts || effect == Effect::Sparkles || effect == Effect::Fountain;
		SetVisible( "Shape", explodes, raiseEvent );
		SetVisible( "Rate", burstTimed, raiseEvent );
		SetVisible( "Height", effect == Effect::Fireworks, raiseEvent );
		SetVisible( "Direction", effect == Effect::Fountain, raiseEvent );
		SetVisible( "Position Y", usesY, raiseEvent );
		SetVisible( "Position X", effect != Effect::ConfettiCannon, raiseEvent );
	}

	/// True once every `beats` beats of the host's clock.
	bool BeatElapsed( int beats )
	{
		// barPhase runs 0..1 across one 4/4 bar; a beat is a quarter of it.
		const int beat = static_cast< int >( std::floor( std::clamp( this->barPhase, 0.0f, 0.9999f ) * 4.0f ) );
		bool      fired = false;
		if( lastBeat >= 0 && beat != lastBeat )
		{
			++beatCounter;
			fired = beatCounter % std::max( 1, beats ) == 0;
		}
		lastBeat = beat;
		return fired;
	}

	/// Onset detection on the bass: a hit is a jump well above the recent
	/// average. Relative rather than an absolute threshold, because the level
	/// Resolume delivers depends on the source and its gain.
	bool AudioHit( float dt )
	{
		const float sensitivity = ParamValue( "Audio Trigger" );
		const float level       = AudioLevel();
		const float average     = audioAverage;
		audioAverage += ( level - audioAverage ) * std::min( 1.0f, dt * 3.0f );
		audioCooldown = std::max( 0.0f, audioCooldown - dt );
		if( sensitivity <= 0.0f || audioCooldown > 0.0f )
			return false;
		const float ratio = 1.25f + ( 1.0f - sensitivity ) * 1.75f;
		if( level > 0.02f && level > average * ratio )
		{
			audioCooldown = 0.15f;
			return true;
		}
		return false;
	}

	void NoteCaught( const char* where, const char* what ) noexcept
	{
		try
		{
			++caughtErrors;
			// 1, 2, 4, 8...: a failure on every frame must not flood the log.
			if( ( caughtErrors & ( caughtErrors - 1 ) ) == 0 )
			{
				FFGLLog::LogToHost( ( std::string( "sparkles: error in " ) + where + ": " + what + " (" +
				                      std::to_string( caughtErrors ) + " so far)" )
				                        .c_str() );
			}
		}
		catch( ... )
		{
		}
	}

	template< typename ParamType >
	void AddGrouped( const char* group, std::shared_ptr< ParamType > param )
	{
		const auto index = static_cast< unsigned int >( this->params.size() );
		this->AddParam( param );
		this->SetParamGroup( index, group );
	}

	/// Hue, saturation, brightness and alpha in a row: Resolume shows them as
	/// one colour picker.
	void AddHueColour( const char* name, float hue, float saturation )
	{
		const auto first = static_cast< unsigned int >( this->params.size() );
		this->AddHueColorParam( name );
		this->params[ first ]->SetValue( hue );
		this->params[ first + 1 ]->SetValue( saturation );
		for( unsigned int i = first; i < first + 4; ++i )
			this->SetParamGroup( i, "Colour" );
	}

	RGB HueColour( const std::string& name ) const
	{
		const unsigned int first = ParamIndex( name );
		if( first == NO_PARAM || first + 2 >= this->params.size() )
			return {};
		const float h = this->params[ first ]->GetValue();
		const float s = std::clamp( this->params[ first + 1 ]->GetValue(), 0.0f, 1.0f );
		const float v = std::clamp( this->params[ first + 2 ]->GetValue(), 0.0f, 1.0f );
		const float hh = ( h - std::floor( h ) ) * 6.0f;
		const float f  = hh - std::floor( hh );
		const float p  = v * ( 1.0f - s );
		const float q  = v * ( 1.0f - s * f );
		const float t  = v * ( 1.0f - s * ( 1.0f - f ) );
		switch( static_cast< int >( hh ) % 6 )
		{
		case 0:  return { v, t, p };
		case 1:  return { q, v, p };
		case 2:  return { p, v, t };
		case 3:  return { p, q, v };
		case 4:  return { t, p, v };
		default: return { v, p, q };
		}
	}

	float ParamValue( const std::string& name ) const
	{
		const unsigned int index = ParamIndex( name );
		return index == NO_PARAM ? 0.0f : this->params[ index ]->GetValue();
	}

	/// An option's stored value (not its position).
	int OptionValue( const std::string& name ) const
	{
		const unsigned int index = ParamIndex( name );
		if( index == NO_PARAM )
			return 0;
		const auto option = std::dynamic_pointer_cast< ffglqs::ParamOption >( this->params[ index ] );
		return option ? static_cast< int >( option->GetRealValue() ) : 0;
	}

	/// Sets a parameter and tells the host, so its control follows. For an
	/// option, `value` is the position in the list.
	void PushParam( const std::string& name, float value )
	{
		const unsigned int index = ParamIndex( name );
		if( index == NO_PARAM )
			return;
		this->params[ index ]->SetValue( value );
		this->RaiseParamEvent( index, FF_EVENT_FLAG_VALUE );
	}

	void PushHueColour( const std::string& name, float hue, float saturation )
	{
		const unsigned int first = ParamIndex( name );
		if( first == NO_PARAM )
			return;
		this->params[ first ]->SetValue( hue );
		this->params[ first + 1 ]->SetValue( saturation );
		this->params[ first + 2 ]->SetValue( 1.0f );
		for( unsigned int i = first; i < first + 3; ++i )
			this->RaiseParamEvent( i, FF_EVENT_FLAG_VALUE );
	}

	void SetVisible( const std::string& name, bool visible, bool raiseEvent )
	{
		const unsigned int index = ParamIndex( name );
		if( index != NO_PARAM )
			this->SetParamVisibility( index, visible, raiseEvent );
	}

	ParticleSystem   system;
	ParticleRenderer renderer;
	std::vector< Instance > instances;

	std::shared_ptr< ffglqs::ParamFFT > audioParam;
	std::map< std::string, float >      defaults;

	unsigned int presetIndex = NO_PARAM;
	unsigned int effectIndex = NO_PARAM;
	unsigned int burstIndex  = NO_PARAM;
	unsigned int clearIndex  = NO_PARAM;
	unsigned int seedIndex   = NO_PARAM;

	int  pendingBursts  = 0;
	bool pendingClear   = false;
	bool burstDown      = false;
	bool clearDown      = false;
	bool applyingPreset = false;
	int  lastPreset     = 0;

	bool   hostTimeValid    = false;
	double lastHostTime     = -1.0;
	bool   lastHostTimeValid = false;
	float  lastStep         = 1.0f / 60.0f;

	int          lastBeat    = -1;
	unsigned int beatCounter = 0;

	float audioAverage  = 0.0f;
	float audioCooldown = 0.0f;

	unsigned int caughtErrors = 0;
};

}  // namespace sparkles
