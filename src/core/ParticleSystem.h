#pragma once

#include "Random.h"
#include "SparkleParams.h"

#include <cstdint>
#include <vector>

namespace sparkles {

/// How a particle moves and how it is drawn.
enum class Kind : std::uint8_t
{
	Spark    = 0,  ///< A point of light: explosion stars, fountain and glitter sparks.
	Rocket   = 1,  ///< A rising shell. Explodes when its fuse runs out.
	Confetti = 2,  ///< An opaque paper rectangle. Drawn "over", not added.
	Star     = 3,  ///< A twinkling four-point glint.
	Flash    = 4,  ///< A brief bright pop: the burst itself, and crackle.
	Ember    = 5,  ///< Trail dust shed by a moving spark.
};

enum ParticleFlags : std::uint8_t
{
	FLAG_TRAIL   = 1 << 0,  ///< Sheds embers as it moves.
	FLAG_CRACKLE = 1 << 1,  ///< Pops into flashes when it dies.
	FLAG_HOT     = 1 << 2,  ///< Starts white-hot and cools to its colour.
	FLAG_FLICKER = 1 << 3,  ///< Brightness jitters frame to frame.
};

struct Particle
{
	float x = 0, y = 0;    ///< World units: x in [0, aspect], y in [0, 1], y up.
	float vx = 0, vy = 0;  ///< World units per second.
	float age = 0, life = 1;
	float size = 1;        ///< Base size in screen heights, before the Size parameter.
	float aspect = 1;      ///< Confetti height / width.
	float rotation = 0, spin = 0;
	float flip = 0, flipRate = 0;          ///< Confetti tumbling about its long axis.
	float twinklePhase = 0, twinkleRate = 0;
	float drag = 0, gravity = 0;           ///< Natural values, before the multipliers.
	float trailClock = 0;
	RGB   colour;
	Kind  kind  = Kind::Spark;
	std::uint8_t flags = 0;
	Shape burstShape   = Shape::Peony;     ///< Rockets: what to explode into.
};

/// Per-instance data uploaded to the GPU. Three vec4 attributes.
struct Instance
{
	float x, y, vx, vy;
	float r, g, b, a;          ///< Straight colour and opacity; the shader premultiplies.
	float sizeX, sizeY, rotation, kind;
};
static_assert( sizeof( Instance ) == 12 * sizeof( float ), "Instance must be tightly packed for the vertex layout" );

/// The CPU half: emission, physics, and per-frame instance data.
///
/// Deliberately on the CPU. OpenGL 4.1 core is the ceiling (it is Apple's), so
/// there are no compute shaders or SSBOs to simulate on the GPU, and a
/// transform-feedback simulation would need branching emission logic that is
/// awkward to express there. Tens of thousands of particles is a fraction of a
/// millisecond on one core, and the result is deterministic and testable.
class ParticleSystem
{
public:
	ParticleSystem();

	/// Reseeds the generator. Particles already alive are kept.
	void Reseed( std::uint32_t seed );

	/// Removes every particle and restarts the emission clocks.
	void Clear();

	/// World width in screen heights (frame width / height).
	void SetAspect( float aspect );
	float Aspect() const { return aspect; }

	/// Advances by `dt` seconds of wall time (Time Scale is applied inside).
	void Step( const SparkleParams& params, float dt, const EmitEvents& events );

	/// Fills `out` with confetti first, then lights. Returns how many of the
	/// leading entries are confetti, which are blended differently.
	size_t BuildInstances( const SparkleParams& params, std::vector< Instance >& out );

	const std::vector< Particle >& Particles() const { return particles; }
	size_t Count() const { return particles.size(); }

	/// Bursts and rocket launches since the last Clear(), for tests and diagnostics.
	std::uint64_t BurstsFired() const { return burstsFired; }

	/// Palette lookup, exposed for tests. `t` in [0, 1).
	static RGB PaletteColour( const SparkleParams& params, float t );

	/// Fires a single burst-type emission for the current effect, as the
	/// Burst button does.
	void Fire( const SparkleParams& params );

private:
	bool Room( const SparkleParams& params ) const;
	Particle& Spawn();

	void LaunchRocket( const SparkleParams& params );
	void Explode( const SparkleParams& params, float x, float y, Shape shape, bool fromRocket );
	void FireCannons( const SparkleParams& params );
	void EmitStream( const SparkleParams& params, int count );
	void EmitSparkle( const SparkleParams& params );
	void EmitConfettiFlake( const SparkleParams& params, float x, float y, float vx, float vy );
	void EmitFountainSpark( const SparkleParams& params );
	void EmitGlitter( const SparkleParams& params );
	void ShedEmber( const SparkleParams& params, const Particle& from );
	void Crackle( const SparkleParams& params, const Particle& from );

	RGB  PickColour( const SparkleParams& params, const RGB& burstColour );
	float SizeJitter( const SparkleParams& params );

	std::vector< Particle > particles;
	std::vector< Particle > spawned;  ///< Children born during a step, appended after it.
	Random        random;
	float         aspect          = 16.0f / 9.0f;
	float         simTime         = 0.0f;
	float         nextBurstIn     = 0.0f;
	float         streamCarry     = 0.0f;
	float         surge           = 0.0f;
	std::uint64_t burstsFired     = 0;
	std::uint32_t stepCount       = 0;
	bool          stepping        = false;
};

}  // namespace sparkles
