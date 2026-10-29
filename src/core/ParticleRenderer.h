#pragma once

#include "GL.h"
#include "ParticleSystem.h"
#include "RenderTarget.h"

#include <ffglex/FFGLScreenQuad.h>
#include <ffglex/FFGLShader.h>

#include <vector>

namespace sparkles {

/// The GPU half. Each frame:
///
///  1. the particle instances are drawn into a cleared half-float target;
///  2. that is merged with last frame's picture, dimmed, into the trail
///     history (see Trail.glsl for why max and not add);
///  3. the history is composited to the host as premultiplied colour.
///
/// The history is two targets that ping-pong, because step 2 reads last
/// frame's picture while writing this frame's.
class ParticleRenderer
{
public:
	ParticleRenderer() = default;
	~ParticleRenderer();

	ParticleRenderer( const ParticleRenderer& )            = delete;
	ParticleRenderer& operator=( const ParticleRenderer& ) = delete;

	/// Compiles shaders and creates buffers. Needs a current context.
	bool Initialise();
	/// Frees every GL object. Safe to call twice, or without Initialise.
	void Release();

	/// Draws one frame into `hostFBO` at `width` x `height`.
	///
	/// \param instances  From ParticleSystem::BuildInstances.
	/// \param confetti   How many leading instances are confetti.
	/// \param dt         Seconds since the last frame, for the trail fade.
	bool Render( GLuint hostFBO, GLsizei width, GLsizei height, const std::vector< Instance >& instances,
	             size_t confetti, const SparkleParams& params, float dt );

	/// Forgets the trail, so the next frame starts from nothing.
	void ClearTrail();

	bool IsInitialised() const { return initialised; }

	/// The trail history just drawn, which is what reaches the host. For tests.
	const RenderTarget& Accumulation() const { return targets[ front ]; }

	/// The per-frame retention factor for a Trail setting and frame time.
	/// Public so the tests can pin the mapping.
	static float TrailKeep( float trail, float dt );

private:
	bool CompileShaders();
	bool CreateBuffers();
	bool EnsureTargets( GLsizei width, GLsizei height );
	void PointInstanceAttributes( size_t firstInstance ) const;

	ffglex::FFGLShader     particleShader;
	ffglex::FFGLShader     trailShader;
	ffglex::FFGLShader     compositeShader;
	ffglex::FFGLScreenQuad quad;

	GLuint vao         = 0;
	GLuint cornerVBO   = 0;
	GLuint instanceVBO = 0;
	size_t instanceCapacity = 0;

	RenderTarget current;     ///< This frame's particles alone.
	RenderTarget targets[ 2 ];///< The trail history, ping-ponged.
	int          front       = 0;
	bool         needsClear  = true;
	bool         initialised = false;
};

}  // namespace sparkles
