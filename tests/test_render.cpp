// The renderer against a real (software) OpenGL 4.1 core context: what reaches
// the host's framebuffer, and what state is left behind.

#include "harness/Readback.h"
#include "harness/TestRunner.h"

#include <ParticleRenderer.h>
#include <ParticleSystem.h>

#include <chrono>
#include <cstdio>
#include <vector>

using namespace sparkles;
using namespace sparkles::test;

namespace {

constexpr GLsizei W     = 320;
constexpr GLsizei H     = 180;
constexpr float   FRAME = 1.0f / 60.0f;

/// A renderer, a simulation and a stand-in host framebuffer, released in the
/// right order while the context is still current.
struct Scene
{
	ParticleRenderer        renderer;
	ParticleSystem          system;
	HostFramebuffer         host;
	std::vector< Instance > instances;
	SparkleParams           params;

	Scene()
	{
		CHECK( renderer.Initialise() );
		CHECK( host.Allocate( W, H ) );
		system.SetAspect( static_cast< float >( W ) / static_cast< float >( H ) );
	}
	~Scene()
	{
		renderer.Release();
		host.target.Release();
	}

	void Frame( EmitEvents events = {} )
	{
		system.Step( params, FRAME, events );
		const size_t confetti = system.BuildInstances( params, instances );
		CHECK( renderer.Render( host.FBO(), W, H, instances, confetti, params, FRAME ) );
	}
};

EmitEvents Bursts( int count )
{
	EmitEvents events;
	events.bursts = count;
	return events;
}

}  // namespace

TEST( NothingEmittedIsFullyTransparent )
{
	Scene scene;
	scene.params.autoEmit = false;
	for( int i = 0; i < 5; ++i )
		scene.Frame();
	const PixelStats stats = Analyse( scene.host.Read() );
	CHECK( stats.lit == 0 );
}

TEST( FireworksReachTheHostAsPremultipliedOverlay )
{
	Scene scene;
	scene.params.effect = Effect::Bursts;
	scene.params.rate   = 4.0f;
	scene.params.glow   = 1.0f;
	scene.params.brightness = 3.0f;  // push additive light well past 1
	for( int i = 0; i < 40; ++i )
		scene.Frame( Bursts( i == 0 ? 3 : 0 ) );

	const PixelStats stats = Analyse( scene.host.Read() );
	CHECK( stats.lit > 200 );
	CHECK( stats.badPremult == 0 );
	// An overlay, not a frame: most of the picture stays see-through.
	CHECK( stats.opaque < W * H / 4 );
}

TEST( ConfettiIsOpaquePaper )
{
	Scene scene;
	scene.params.effect = Effect::ConfettiCannon;
	scene.params.size   = 3.0f;
	scene.params.trail  = 0.0f;
	for( int i = 0; i < 30; ++i )
		scene.Frame( Bursts( i == 0 ? 1 : 0 ) );
	const PixelStats stats = Analyse( scene.host.Read() );
	CHECK( stats.opaque > 50 );
	CHECK( stats.badPremult == 0 );
}

TEST( BlackBackgroundIsOpaqueEverywhere )
{
	Scene scene;
	scene.params.blackBackground = true;
	scene.params.effect          = Effect::Sparkles;
	for( int i = 0; i < 10; ++i )
		scene.Frame();
	const PixelStats stats = Analyse( scene.host.Read() );
	CHECK( stats.opaque == W * H );
}

TEST( TheTrailFadesAllTheWayToZero )
{
	// Multiplying alone stalls in half float and leaves a permanent haze over
	// a transparent layer. The longest trail must still come back to nothing.
	Scene scene;
	scene.params.effect     = Effect::Bursts;
	scene.params.trail      = 1.0f;
	scene.params.brightness = 3.0f;
	for( int i = 0; i < 30; ++i )
		scene.Frame( Bursts( i == 0 ? 4 : 0 ) );
	CHECK( Analyse( scene.host.Read() ).lit > 0 );

	EmitEvents clear;
	clear.clear            = true;
	scene.params.autoEmit  = false;
	scene.Frame( clear );
	for( int i = 0; i < 20 * 60; ++i )
		scene.Frame();

	float peak = 0.0f;
	for( float v : ReadFloat( scene.renderer.Accumulation() ) )
		peak = v > peak ? v : peak;
	CHECK( peak == 0.0f );
	CHECK( Analyse( scene.host.Read() ).lit == 0 );
}

TEST( NoTrailMeansNoGhosts )
{
	Scene scene;
	scene.params.effect = Effect::Bursts;
	scene.params.trail  = 0.0f;
	scene.Frame( Bursts( 2 ) );
	EmitEvents clear;
	clear.clear           = true;
	scene.params.autoEmit = false;
	scene.Frame( clear );
	CHECK( Analyse( scene.host.Read() ).lit == 0 );
}

TEST( HostStateIsLeftAsFound )
{
	Scene scene;
	scene.params.effect = Effect::ConfettiCannon;

	// What a host might have bound around the call.
	HostFramebuffer other;
	CHECK( other.Allocate( 16, 16 ) );
	glBindFramebuffer( GL_FRAMEBUFFER, other.FBO() );
	glViewport( 3, 4, 50, 60 );

	scene.Frame( Bursts( 1 ) );
	scene.Frame();

	GLint fbo = 0, program = 0, vao = 0, viewport[ 4 ] = {};
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &fbo );
	glGetIntegerv( GL_CURRENT_PROGRAM, &program );
	glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &vao );
	glGetIntegerv( GL_VIEWPORT, viewport );
	CHECK( static_cast< GLuint >( fbo ) == other.FBO() );
	CHECK( program == 0 );
	CHECK( vao == 0 );
	CHECK( viewport[ 0 ] == 3 && viewport[ 1 ] == 4 && viewport[ 2 ] == 50 && viewport[ 3 ] == 60 );
	CHECK( glIsEnabled( GL_BLEND ) == GL_FALSE );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	other.target.Release();
}

TEST( AResizeReallocatesCleanly )
{
	ParticleRenderer renderer;
	CHECK( renderer.Initialise() );
	HostFramebuffer small, large;
	CHECK( small.Allocate( 64, 36 ) );
	CHECK( large.Allocate( 256, 144 ) );
	SparkleParams           params;
	std::vector< Instance > none;
	CHECK( renderer.Render( small.FBO(), 64, 36, none, 0, params, FRAME ) );
	CHECK( renderer.Render( large.FBO(), 256, 144, none, 0, params, FRAME ) );
	CHECK( renderer.Accumulation().GetWidth() == 256 );
	renderer.Release();
	renderer.Release();  // twice is harmless
	small.target.Release();
	large.target.Release();
}

namespace sparkles::test {

void RunBenchmark()
{
	constexpr GLsizei BW = 1920, BH = 1080;
	ParticleRenderer renderer;
	HostFramebuffer  host;
	if( !renderer.Initialise() || !host.Allocate( BW, BH ) )
	{
		std::printf( "benchmark: could not initialise\n" );
		return;
	}

	for( int maxParticles : { 5000, 20000, 80000 } )
	{
		ParticleSystem system;
		system.SetAspect( 16.0f / 9.0f );
		SparkleParams params;
		params.effect       = Effect::Bursts;
		params.shape        = Shape::Willow;  // trails: the heaviest shape
		params.density      = 1.0f;
		params.rate         = 10.0f;
		params.maxParticles = maxParticles;
		std::vector< Instance > instances;

		// Fill to the cap first.
		EmitEvents salvo;
		salvo.bursts = 4;
		for( int i = 0; i < 600; ++i )
			system.Step( params, FRAME, salvo );

		double simMs = 0.0, frameMs = 0.0;
		const int frames = 60;
		for( int i = 0; i < frames; ++i )
		{
			const auto t0 = std::chrono::steady_clock::now();
			system.Step( params, FRAME, salvo );
			const size_t confetti = system.BuildInstances( params, instances );
			const auto t1 = std::chrono::steady_clock::now();
			renderer.Render( host.FBO(), BW, BH, instances, confetti, params, FRAME );
			glFinish();
			const auto t2 = std::chrono::steady_clock::now();
			simMs   += std::chrono::duration< double, std::milli >( t1 - t0 ).count();
			frameMs += std::chrono::duration< double, std::milli >( t2 - t0 ).count();
		}
		std::printf( "cap %6d (%zu live): simulation + instances %.2f ms, whole frame %.2f ms at 1080p\n", maxParticles,
		             system.Count(), simMs / frames, frameMs / frames );
	}

	renderer.Release();
	host.target.Release();
}

}  // namespace sparkles::test
