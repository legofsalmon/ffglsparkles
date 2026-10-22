#include "ParticleRenderer.h"

#include <ParticleFS.glsl.h>
#include <ParticleVS.glsl.h>
#include <Composite.glsl.h>
#include <ScreenQuad.glsl.h>
#include <Trail.glsl.h>

#include <ffgl/FFGLLog.h>
#include <ffglex/FFGLScopedBufferBinding.h>
#include <ffglex/FFGLScopedFBOBinding.h>
#include <ffglex/FFGLScopedSamplerActivation.h>
#include <ffglex/FFGLScopedShaderBinding.h>
#include <ffglex/FFGLScopedTextureBinding.h>
#include <ffglex/FFGLScopedVAOBinding.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace sparkles {

namespace {

/// Blending on for the scope, then back to the FFGL default on the way out.
///
/// FFGL hosts hand a plugin the context in its default state and expect it
/// back that way: blending disabled, ONE / ZERO, ADD. Leaving additive blending
/// enabled would quietly change how the host draws the next layer.
class ScopedBlend
{
public:
	ScopedBlend()
	{
		glEnable( GL_BLEND );
		glBlendEquation( GL_FUNC_ADD );
	}
	~ScopedBlend()
	{
		glBlendFunc( GL_ONE, GL_ZERO );
		glDisable( GL_BLEND );
	}
	ScopedBlend( const ScopedBlend& )            = delete;
	ScopedBlend& operator=( const ScopedBlend& ) = delete;
};

bool CompileOne( ffglex::FFGLShader& shader, const char* vertex, const char* fragment, const char* name )
{
	if( shader.Compile( std::string( vertex ), std::string( fragment ) ) )
		return true;
	FFGLLog::LogToHost( ( std::string( "sparkles: shader failed to compile: " ) + name ).c_str() );
	return false;
}

}  // namespace

ParticleRenderer::~ParticleRenderer()
{
	// GL objects must be released from DeInitGL with the context current;
	// by the time the destructor runs the host may have destroyed it.
	if( vao != 0 || cornerVBO != 0 || instanceVBO != 0 )
		FFGLLog::LogToHost( "sparkles: ParticleRenderer destroyed without Release() - GL objects leaked" );
}

bool ParticleRenderer::Initialise()
{
	Release();
	if( !CompileShaders() || !quad.Initialise() || !CreateBuffers() )
	{
		Release();
		return false;
	}
	initialised = true;
	needsClear  = true;
	return CheckGL( "ParticleRenderer::Initialise" );
}

bool ParticleRenderer::CompileShaders()
{
	return CompileOne( particleShader, shaders::ParticleVS, shaders::ParticleFS, "Particle" ) &&
	       CompileOne( trailShader, shaders::ScreenQuad, shaders::Trail, "Trail" ) &&
	       CompileOne( compositeShader, shaders::ScreenQuad, shaders::Composite, "Composite" );
}

bool ParticleRenderer::CreateBuffers()
{
	glGenVertexArrays( 1, &vao );
	glGenBuffers( 1, &cornerVBO );
	glGenBuffers( 1, &instanceVBO );
	if( vao == 0 || cornerVBO == 0 || instanceVBO == 0 )
		return false;

	// Two triangles as a strip.
	static const float corners[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };

	ffglex::ScopedVAOBinding vaoBinding( vao );
	{
		ffglex::ScopedVBOBinding vbo( cornerVBO );
		glBufferData( GL_ARRAY_BUFFER, sizeof( corners ), corners, GL_STATIC_DRAW );
		glEnableVertexAttribArray( 0 );
		glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof( float ), nullptr );
		// The scoped binding unbinds GL_ARRAY_BUFFER on exit, which is fine:
		// the VAO captured the buffer in the attribute pointer.
	}
	{
		ffglex::ScopedVBOBinding vbo( instanceVBO );
		instanceCapacity = 1024;
		glBufferData( GL_ARRAY_BUFFER, instanceCapacity * sizeof( Instance ), nullptr, GL_STREAM_DRAW );
		for( GLuint location = 1; location <= 3; ++location )
		{
			glEnableVertexAttribArray( location );
			glVertexAttribDivisor( location, 1 );
		}
		PointInstanceAttributes( 0 );
	}
	return true;
}

void ParticleRenderer::PointInstanceAttributes( size_t firstInstance ) const
{
	// OpenGL 4.1 has no base-instance draw (that is 4.2), so drawing a sub-range
	// of the instances means pointing the attributes further into the buffer.
	// Expects instanceVBO bound to GL_ARRAY_BUFFER and the VAO bound.
	const size_t   base   = firstInstance * sizeof( Instance );
	const GLsizei  stride = sizeof( Instance );
	glVertexAttribPointer( 1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast< const void* >( base + offsetof( Instance, x ) ) );
	glVertexAttribPointer( 2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast< const void* >( base + offsetof( Instance, r ) ) );
	glVertexAttribPointer( 3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast< const void* >( base + offsetof( Instance, sizeX ) ) );
}

void ParticleRenderer::Release()
{
	particleShader.FreeGLResources();
	trailShader.FreeGLResources();
	compositeShader.FreeGLResources();
	quad.Release();
	if( vao != 0 )
		glDeleteVertexArrays( 1, &vao );
	if( cornerVBO != 0 )
		glDeleteBuffers( 1, &cornerVBO );
	if( instanceVBO != 0 )
		glDeleteBuffers( 1, &instanceVBO );
	vao = cornerVBO = instanceVBO = 0;
	instanceCapacity = 0;
	current.Release();
	targets[ 0 ].Release();
	targets[ 1 ].Release();
	initialised = false;
}

void ParticleRenderer::ClearTrail()
{
	needsClear = true;
}

float ParticleRenderer::TrailKeep( float trail, float dt )
{
	trail = std::clamp( trail, 0.0f, 1.0f );
	if( trail <= 0.001f )
		return 0.0f;
	// As a half-life, so the look holds at any frame rate: 20 ms (barely
	// there) up to 1.5 s (long smoky streaks).
	const float halfLife = 0.02f + trail * trail * 1.48f;
	const float keep     = std::pow( 0.5f, std::max( dt, 1.0f / 240.0f ) / halfLife );
	return std::min( keep, 0.995f );
}

bool ParticleRenderer::EnsureTargets( GLsizei width, GLsizei height )
{
	if( current.IsValid() && current.GetWidth() == width && current.GetHeight() == height )
		return true;
	// Half float: additive light needs headroom above 1, and the trail fade
	// needs more than 8 bits or it bands and never quite reaches zero.
	if( !current.Allocate( width, height, GL_RGBA16F ) || !targets[ 0 ].Allocate( width, height, GL_RGBA16F ) ||
	    !targets[ 1 ].Allocate( width, height, GL_RGBA16F ) )
		return false;
	needsClear = true;
	return true;
}

bool ParticleRenderer::Render( GLuint hostFBO, GLsizei width, GLsizei height, const std::vector< Instance >& instances,
                               size_t confetti, const SparkleParams& params, float dt )
{
	if( !initialised || width <= 0 || height <= 0 )
		return false;
	if( !EnsureTargets( width, height ) )
		return false;

	if( needsClear )
	{
		targets[ 0 ].Clear();
		targets[ 1 ].Clear();
		needsClear = false;
	}

	const RenderTarget& previous = targets[ front ];
	const RenderTarget& next     = targets[ front ^ 1 ];

	// --- this frame's particles, on their own ---
	{
		ScopedViewport           viewport( width, height );
		ffglex::ScopedFBOBinding fbo( current.GetFBO(), ffglex::ScopedFBOBinding::RB_REVERT );
		current.Clear();

		const size_t count = instances.size();
		if( count > 0 )
		{
			ffglex::ScopedShaderBinding shader( particleShader.GetGLID() );
			ffglex::ScopedVAOBinding    vaoBinding( vao );
			ffglex::ScopedVBOBinding    vbo( instanceVBO );

			particleShader.Set( "Aspect", static_cast< float >( width ) / static_cast< float >( height ) );
			particleShader.Set( "Glow", std::clamp( params.glow, 0.0f, 1.0f ) );
			particleShader.Set( "Streak", std::clamp( params.streak, 0.0f, 1.0f ) );

			// Orphan and refill: the driver hands back fresh storage instead of
			// waiting for last frame's draw to finish reading the old one.
			if( count > instanceCapacity )
				instanceCapacity = std::max( count, instanceCapacity * 2 );
			glBufferData( GL_ARRAY_BUFFER, instanceCapacity * sizeof( Instance ), nullptr, GL_STREAM_DRAW );
			glBufferSubData( GL_ARRAY_BUFFER, 0, count * sizeof( Instance ), instances.data() );

			ScopedBlend blend;
			if( confetti > 0 )
			{
				glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
				PointInstanceAttributes( 0 );
				glDrawArraysInstanced( GL_TRIANGLE_STRIP, 0, 4, static_cast< GLsizei >( confetti ) );
			}
			if( count > confetti )
			{
				glBlendFunc( GL_ONE, GL_ONE );
				PointInstanceAttributes( confetti );
				glDrawArraysInstanced( GL_TRIANGLE_STRIP, 0, 4, static_cast< GLsizei >( count - confetti ) );
			}
			PointInstanceAttributes( 0 );
		}
	}

	// --- merged with last frame's, dimmed ---
	{
		ScopedViewport                  viewport( width, height );
		ffglex::ScopedFBOBinding        fbo( next.GetFBO(), ffglex::ScopedFBOBinding::RB_REVERT );
		ffglex::ScopedShaderBinding     shader( trailShader.GetGLID() );
		ffglex::ScopedSamplerActivation sampler1( 1 );
		ffglex::Scoped2DTextureBinding  texture1( current.GetTexture() );
		ffglex::ScopedSamplerActivation sampler0( 0 );
		ffglex::Scoped2DTextureBinding  texture0( previous.GetTexture() );
		trailShader.Set( "Previous", 0 );
		trailShader.Set( "Current", 1 );
		trailShader.Set( "Keep", TrailKeep( params.trail, dt ) );
		quad.Draw();
	}

	front ^= 1;

	// --- to the host ---
	{
		ScopedViewport                  viewport( width, height );
		ffglex::ScopedFBOBinding        fbo( hostFBO, ffglex::ScopedFBOBinding::RB_REVERT );
		ffglex::ScopedShaderBinding     shader( compositeShader.GetGLID() );
		ffglex::ScopedSamplerActivation sampler( 0 );
		ffglex::Scoped2DTextureBinding  texture( targets[ front ].GetTexture() );
		compositeShader.Set( "Accumulated", 0 );
		compositeShader.Set( "Brightness", std::max( 0.0f, params.brightness ) );
		compositeShader.Set( "Opaque", params.blackBackground ? 1.0f : 0.0f );
		quad.Draw();
	}

	return CheckGL( "ParticleRenderer::Render" );
}

}  // namespace sparkles
