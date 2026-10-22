// The CPU simulation, without a renderer. These run in microseconds and pin the
// behaviour every parameter promises.

#include "harness/TestRunner.h"

#include <ParticleRenderer.h>
#include <ParticleSystem.h>

#include <cmath>
#include <vector>

using namespace sparkles;

namespace {

constexpr float FRAME = 1.0f / 60.0f;

void Run( ParticleSystem& system, const SparkleParams& params, float seconds, EmitEvents first = {} )
{
	const int frames = static_cast< int >( seconds / FRAME );
	for( int i = 0; i < frames; ++i )
	{
		system.Step( params, FRAME, i == 0 ? first : EmitEvents{} );
	}
}

EmitEvents Bursts( int count )
{
	EmitEvents events;
	events.bursts = count;
	return events;
}

size_t CountKind( const ParticleSystem& system, Kind kind )
{
	size_t n = 0;
	for( const Particle& p : system.Particles() )
		if( p.kind == kind )
			++n;
	return n;
}

}  // namespace

TEST( TheSameSeedReplaysTheSameShow )
{
	SparkleParams params;
	params.rate = 3.0f;

	ParticleSystem a, b;
	a.Reseed( 42 );
	b.Reseed( 42 );
	Run( a, params, 3.0f );
	Run( b, params, 3.0f );

	CHECK( a.Count() > 0 );
	CHECK( a.Count() == b.Count() );
	bool identical = a.Count() == b.Count();
	for( size_t i = 0; identical && i < a.Count(); ++i )
		identical = a.Particles()[ i ].x == b.Particles()[ i ].x && a.Particles()[ i ].y == b.Particles()[ i ].y;
	CHECK( identical );
}

TEST( DifferentSeedsGiveDifferentShows )
{
	SparkleParams params;
	params.rate = 3.0f;

	ParticleSystem a, b;
	a.Reseed( 1 );
	b.Reseed( 2 );
	Run( a, params, 2.0f );
	Run( b, params, 2.0f );

	bool differs = a.Count() != b.Count();
	for( size_t i = 0; !differs && i < a.Count(); ++i )
		differs = a.Particles()[ i ].x != b.Particles()[ i ].x;
	CHECK( differs );
}

TEST( EveryEffectEmitsWhenAutoIsOn )
{
	for( int effect = 0; effect < static_cast< int >( Effect::COUNT ); ++effect )
	{
		SparkleParams params;
		params.effect = static_cast< Effect >( effect );
		params.rate   = 2.0f;
		ParticleSystem system;
		Run( system, params, 1.5f );
		if( system.Count() == 0 )
			::sparkles::test::Registry::Instance().Fail( "  effect " + std::to_string( effect ) + " emitted nothing" );
	}
}

TEST( WithAutoOffNothingHappensUntilBurst )
{
	for( int effect = 0; effect < static_cast< int >( Effect::COUNT ); ++effect )
	{
		SparkleParams params;
		params.effect   = static_cast< Effect >( effect );
		params.autoEmit = false;
		ParticleSystem system;
		Run( system, params, 1.0f );
		if( system.Count() != 0 )
			::sparkles::test::Registry::Instance().Fail( "  effect " + std::to_string( effect ) + " emitted with Auto off" );

		Run( system, params, 0.5f, Bursts( 1 ) );
		if( system.Count() == 0 )
			::sparkles::test::Registry::Instance().Fail( "  effect " + std::to_string( effect ) + " ignored Burst" );
	}
}

TEST( RocketsRiseAndExplodeNearTheHeightSetting )
{
	SparkleParams params;
	params.autoEmit = false;
	params.height   = 0.6f;
	params.shape    = Shape::Peony;

	ParticleSystem system;
	system.Step( params, FRAME, Bursts( 1 ) );
	CHECK( CountKind( system, Kind::Rocket ) == 1 );

	// Step until the rocket is gone; the burst's flash marks where it went off.
	float burstY = -1.0f;
	for( int i = 0; i < 600 && burstY < 0.0f; ++i )
	{
		system.Step( params, FRAME, {} );
		if( CountKind( system, Kind::Rocket ) == 0 )
			for( const Particle& p : system.Particles() )
				if( p.kind == Kind::Flash )
					burstY = p.y;
	}
	CHECK( burstY > 0.0f );
	// Height is jittered by +-0.08 so a volley is not a flat line.
	CHECK_NEAR( burstY, 0.6f, 0.1f );
	CHECK( CountKind( system, Kind::Spark ) > 30 );
}

TEST( EveryShapeExplodesIntoStars )
{
	for( int shape = 0; shape < static_cast< int >( Shape::COUNT ); ++shape )
	{
		SparkleParams params;
		params.effect   = Effect::Bursts;
		params.shape    = static_cast< Shape >( shape );
		params.autoEmit = false;
		ParticleSystem system;
		system.Step( params, FRAME, Bursts( 1 ) );
		if( CountKind( system, Kind::Spark ) < 30 )
			::sparkles::test::Registry::Instance().Fail( "  shape " + std::to_string( shape ) + " made too few stars" );
	}
}

TEST( DensityScalesTheBurst )
{
	auto starsAt = []( float density ) {
		SparkleParams params;
		params.effect   = Effect::Bursts;
		params.shape    = Shape::Ring;
		params.autoEmit = false;
		params.density  = density;
		ParticleSystem system;
		system.Step( params, FRAME, Bursts( 1 ) );
		return CountKind( system, Kind::Spark );
	};
	CHECK( starsAt( 1.0f ) > starsAt( 0.5f ) );
	CHECK( starsAt( 0.5f ) > starsAt( 0.0f ) );
}

TEST( MaxParticlesIsNeverExceeded )
{
	SparkleParams params;
	params.effect       = Effect::Fountain;
	params.density      = 1.0f;
	params.maxParticles = 500;

	ParticleSystem system;
	bool within = true;
	for( int i = 0; i < 300; ++i )
	{
		system.Step( params, FRAME, Bursts( i % 10 == 0 ? 1 : 0 ) );
		within = within && system.Count() <= 500;
	}
	CHECK( within );
	CHECK( system.Count() > 400 );
}

TEST( ParticlesDieOutWhenEmissionStops )
{
	SparkleParams params;
	params.autoEmit = false;
	params.shape    = Shape::Willow;  // the longest-lived

	ParticleSystem system;
	Run( system, params, 20.0f, Bursts( 3 ) );
	CHECK( system.Count() == 0 );
}

TEST( ClearRemovesEverything )
{
	SparkleParams params;
	params.effect = Effect::GlitterRain;
	ParticleSystem system;
	Run( system, params, 1.0f );
	CHECK( system.Count() > 0 );

	EmitEvents clear;
	clear.clear  = true;
	params.autoEmit = false;
	system.Step( params, FRAME, clear );
	CHECK( system.Count() == 0 );
}

TEST( GravityAndWindMoveConfetti )
{
	SparkleParams params;
	params.effect   = Effect::ConfettiRain;
	params.autoEmit = false;

	auto meanAfter = [ & ]( float wind, float& meanX, float& meanY ) {
		params.wind = wind;
		ParticleSystem system;
		system.Step( params, FRAME, Bursts( 1 ) );
		Run( system, params, 3.0f );
		meanX = meanY = 0.0f;
		for( const Particle& p : system.Particles() )
		{
			meanX += p.x;
			meanY += p.y;
		}
		const float n = static_cast< float >( std::max< size_t >( 1, system.Count() ) );
		meanX /= n;
		meanY /= n;
	};

	float calmX, calmY, windyX, windyY;
	meanAfter( 0.0f, calmX, calmY );
	meanAfter( 1.0f, windyX, windyY );
	CHECK( calmY < 1.0f );       // it fell from the top
	CHECK( windyX > calmX + 0.05f );
}

TEST( TimeScaleZeroFreezesMotion )
{
	SparkleParams params;
	params.effect = Effect::Bursts;
	params.rate   = 5.0f;
	ParticleSystem system;
	Run( system, params, 1.0f );
	const std::vector< Particle > before = system.Particles();

	params.timeScale = 0.0f;
	Run( system, params, 1.0f );
	CHECK( system.Count() == before.size() );
	bool still = system.Count() == before.size();
	for( size_t i = 0; still && i < before.size(); ++i )
		still = system.Particles()[ i ].x == before[ i ].x && system.Particles()[ i ].y == before[ i ].y;
	CHECK( still );
}

TEST( AHugeFrameTimeIsBounded )
{
	// A stalled host frame must not fire thousands of bursts at once.
	SparkleParams params;
	params.effect = Effect::Bursts;
	params.rate   = 10.0f;
	ParticleSystem system;
	system.Step( params, 1000.0f, {} );
	CHECK( system.BurstsFired() <= 16 );
}

TEST( RateSetsTheBurstFrequency )
{
	SparkleParams params;
	params.effect = Effect::Bursts;
	params.rate   = 2.0f;
	ParticleSystem system;
	Run( system, params, 20.0f );
	// Jittered intervals average out to the rate.
	CHECK_NEAR( static_cast< double >( system.BurstsFired() ), 40.0, 8.0 );
}

TEST( SingleColourPaletteUsesColourA )
{
	SparkleParams params;
	params.palette = Palette::Single;
	params.colourA = { 0.1f, 0.9f, 0.2f };
	for( float t : { 0.0f, 0.3f, 0.7f, 0.99f } )
	{
		const RGB c = ParticleSystem::PaletteColour( params, t );
		CHECK_NEAR( c.g, 0.9f, 1e-6 );
	}
	params.palette = Palette::TwoTone;
	params.colourB = { 0.8f, 0.1f, 0.1f };
	CHECK_NEAR( ParticleSystem::PaletteColour( params, 0.75f ).r, 0.8f, 1e-6 );
}

TEST( ConfettiIsDrawnBeforeLight )
{
	SparkleParams params;
	params.effect = Effect::ConfettiCannon;
	params.rate   = 2.0f;
	ParticleSystem system;
	Run( system, params, 1.0f );
	params.effect = Effect::Fountain;
	Run( system, params, 0.5f );

	std::vector< Instance > instances;
	const size_t confetti = system.BuildInstances( params, instances );
	CHECK( confetti > 0 );
	CHECK( instances.size() > confetti );
	bool ordered = true;
	for( size_t i = 0; i < instances.size(); ++i )
		ordered = ordered && ( ( i < confetti ) == ( static_cast< int >( instances[ i ].kind ) == static_cast< int >( Kind::Confetti ) ) );
	CHECK( ordered );
}

TEST( BuildingInstancesDoesNotChangeTheSimulation )
{
	// The host may draw a frame more than once; the show must not depend on
	// how often it does.
	SparkleParams params;
	params.effect = Effect::Fountain;
	ParticleSystem a, b;
	std::vector< Instance > scratch;
	for( int i = 0; i < 60; ++i )
	{
		a.Step( params, FRAME, {} );
		b.Step( params, FRAME, {} );
		a.BuildInstances( params, scratch );
		a.BuildInstances( params, scratch );
	}
	CHECK( a.Count() == b.Count() );
	CHECK( a.Count() > 0 && a.Particles().back().x == b.Particles().back().x );
}

TEST( AspectChangeKeepsParticlesInPlace )
{
	SparkleParams params;
	params.effect = Effect::Sparkles;
	ParticleSystem system;
	system.SetAspect( 16.0f / 9.0f );
	Run( system, params, 1.0f );
	const float before = system.Particles().front().x / system.Aspect();
	system.SetAspect( 4.0f / 3.0f );
	CHECK_NEAR( system.Particles().front().x / system.Aspect(), before, 1e-5 );
}

TEST( TrailKeepIsFrameRateIndependent )
{
	CHECK( ParticleRenderer::TrailKeep( 0.0f, FRAME ) == 0.0f );
	CHECK( ParticleRenderer::TrailKeep( 0.5f, FRAME ) < ParticleRenderer::TrailKeep( 0.9f, FRAME ) );
	CHECK( ParticleRenderer::TrailKeep( 1.0f, FRAME ) <= 0.995f );

	// Two frames at 60 fps fade as much as one frame at 30.
	const float at60 = ParticleRenderer::TrailKeep( 0.5f, 1.0f / 60.0f );
	const float at30 = ParticleRenderer::TrailKeep( 0.5f, 1.0f / 30.0f );
	CHECK_NEAR( at60 * at60, at30, 1e-4 );
}
