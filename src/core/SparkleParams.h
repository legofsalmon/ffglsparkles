#pragma once

#include <cstdint>

namespace sparkles {

/// What gets emitted. The numeric value is the position in the Effect dropdown,
/// and a saved composition stores the position, so entries are appended and
/// never reordered.
enum class Effect : int
{
	Fireworks      = 0,  ///< Rockets rise from the bottom and explode.
	Bursts         = 1,  ///< Explosions appear in place, no rocket.
	Sparkles       = 2,  ///< Twinkling stars scattered over an area.
	ConfettiRain   = 3,  ///< Paper flakes falling from the top.
	ConfettiCannon = 4,  ///< Paper fired from the two bottom corners.
	Fountain       = 5,  ///< A continuous spray of sparks, like a gerb.
	GlitterRain    = 6,  ///< Golden sparks drifting down from the top.
	COUNT
};

/// Shape of a firework or burst explosion. Appended, never reordered.
enum class Shape : int
{
	Peony   = 0,  ///< Even sphere of stars.
	Ring    = 1,  ///< A thin tilted ring.
	Willow  = 2,  ///< Slow, drooping, long-lived gold.
	Palm    = 3,  ///< A handful of thick arms.
	Heart   = 4,
	Star    = 5,  ///< Five-pointed star outline.
	Crackle = 6,  ///< Peony whose stars pop into flashes.
	Random  = 7,  ///< Any of the above, per burst.
	COUNT
};

/// Colour scheme. Appended, never reordered.
enum class Palette : int
{
	Rainbow = 0,
	Gold    = 1,
	Silver  = 2,
	Fire    = 3,
	Ice     = 4,
	Pastel  = 5,
	Neon    = 6,
	TwoTone = 7,  ///< Colour A and Colour B.
	Single  = 8,  ///< Colour A only.
	COUNT
};

struct RGB
{
	float r = 1.0f, g = 1.0f, b = 1.0f;
};

/// Every parameter the simulation and renderer read, as plain values.
///
/// The plugin fills this from its parameter list each frame. Keeping the core
/// independent of the FFGL parameter classes is what lets the tests drive the
/// simulation directly with known values.
struct SparkleParams
{
	Effect  effect  = Effect::Fireworks;
	Shape   shape   = Shape::Peony;
	Palette palette = Palette::Rainbow;
	RGB     colourA = { 1.0f, 0.75f, 0.2f };
	RGB     colourB = { 0.3f, 0.6f, 1.0f };

	/// 0 = every particle in a burst shares one palette colour, 1 = each picks its own.
	float colourVariety = 0.3f;

	// --- emission ---
	bool  autoEmit  = true;
	float rate      = 1.0f;   ///< Bursts per second, for burst-type effects.
	float density   = 0.5f;   ///< 0..1, particles per burst or per second.
	float height    = 0.7f;   ///< Fireworks: where rockets burst, 0 = bottom, 1 = top.
	float positionX = 0.5f;   ///< 0..1 across the frame.
	float positionY = 0.5f;   ///< 0..1 up the frame.
	float spread    = 0.6f;   ///< 0..1: width of the emission area, or cone width.
	float direction = 90.0f;  ///< Fountain: degrees, 90 = straight up.

	// --- motion ---
	float speed      = 1.0f;  ///< Multiplier on every launch and explosion velocity.
	float gravity    = 1.0f;  ///< Multiplier on each kind's natural gravity.
	float drag       = 1.0f;  ///< Multiplier on each kind's natural air drag.
	float wind       = 0.0f;  ///< -1..1, sideways push.
	float turbulence = 0.0f;  ///< 0..1, swirling noise.
	float lifetime   = 1.0f;  ///< Multiplier on each kind's natural lifetime.
	float timeScale  = 1.0f;  ///< Slow motion below 1.

	// --- look ---
	float size          = 1.0f;
	float sizeVariation = 0.4f;
	float glow          = 0.5f;
	float streak        = 0.3f;  ///< Motion-blur stretch along the velocity.
	float trail         = 0.4f;  ///< 0..1 persistence of the previous frame.
	float twinkle       = 0.4f;
	float brightness    = 1.0f;

	// --- output ---
	bool blackBackground = false;
	int  maxParticles    = 20000;
};

/// One-shot requests that arrive alongside the parameters for a single step.
struct EmitEvents
{
	int  bursts = 0;  ///< Bursts to fire now, from the button, a beat or an audio hit.
	bool clear  = false;
};

}  // namespace sparkles
