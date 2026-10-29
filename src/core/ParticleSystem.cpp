#include "ParticleSystem.h"

#include <algorithm>
#include <cmath>

namespace sparkles {

namespace {

constexpr float PI     = 3.14159265358979f;
constexpr float TWO_PI = 6.28318530717959f;

// Base sizes in screen heights, before the Size parameter. Chosen by eye at
// 1080p: a spark is ~5 px across, a confetti flake ~13 px.
constexpr float SIZE_SPARK    = 0.0045f;
constexpr float SIZE_ROCKET   = 0.0040f;
constexpr float SIZE_EMBER    = 0.0028f;
constexpr float SIZE_STAR     = 0.0085f;
constexpr float SIZE_FLASH    = 0.0060f;
constexpr float SIZE_CONFETTI = 0.0120f;

// Rockets and willows shed this many embers per second of their own life.
constexpr float EMBERS_PER_SECOND_ROCKET = 90.0f;
constexpr float EMBERS_PER_SECOND_SPARK  = 30.0f;
// Embers are decoration: they stop before the budget is full so that a dense
// show never loses explosion stars to its own dust.
constexpr float EMBER_BUDGET = 0.8f;

/// Stateless per-frame noise. Flicker must not draw from the simulation's
/// generator: the host may ask for a frame more than once, and the look must
/// not depend on how often it does.
float Hash01( std::uint32_t a, std::uint32_t b )
{
	std::uint32_t h = a * 374761393u + b * 668265263u;
	h               = ( h ^ ( h >> 13 ) ) * 1274126177u;
	h               = h ^ ( h >> 16 );
	return static_cast< float >( h & 0xFFFFFFu ) / 16777216.0f;
}

float Clamp01( float v ) { return v < 0.0f ? 0.0f : ( v > 1.0f ? 1.0f : v ); }

RGB Hsv( float h, float s, float v )
{
	h = h - std::floor( h );
	const float i = std::floor( h * 6.0f );
	const float f = h * 6.0f - i;
	const float p = v * ( 1.0f - s );
	const float q = v * ( 1.0f - f * s );
	const float t = v * ( 1.0f - ( 1.0f - f ) * s );
	switch( static_cast< int >( i ) % 6 )
	{
	case 0:  return { v, t, p };
	case 1:  return { q, v, p };
	case 2:  return { p, v, t };
	case 3:  return { p, q, v };
	case 4:  return { t, p, v };
	default: return { v, p, q };
	}
}

RGB Lerp( const RGB& a, const RGB& b, float t )
{
	return { a.r + ( b.r - a.r ) * t, a.g + ( b.g - a.g ) * t, a.b + ( b.b - a.b ) * t };
}

/// Effects that fire discrete bursts on a clock, as opposed to a continuous flow.
bool IsBurstEffect( Effect effect )
{
	return effect == Effect::Fireworks || effect == Effect::Bursts || effect == Effect::ConfettiCannon;
}

/// Particles per second for the stream effects at a given density.
float StreamRate( Effect effect, float density )
{
	switch( effect )
	{
	case Effect::Sparkles:     return 10.0f + density * 390.0f;
	case Effect::ConfettiRain: return 5.0f + density * 295.0f;
	case Effect::Fountain:     return 60.0f + density * 1440.0f;
	case Effect::GlitterRain:  return 20.0f + density * 780.0f;
	default:                   return 0.0f;
	}
}

}  // namespace

ParticleSystem::ParticleSystem()
{
	particles.reserve( 4096 );
	Reseed( 1 );
}

void ParticleSystem::Reseed( std::uint32_t seed )
{
	random.Seed( seed );
}

void ParticleSystem::Clear()
{
	particles.clear();
	spawned.clear();
	simTime     = 0.0f;
	nextBurstIn = 0.0f;
	streamCarry = 0.0f;
	surge       = 0.0f;
	burstsFired = 0;
}

void ParticleSystem::SetAspect( float newAspect )
{
	if( !( newAspect > 0.05f && newAspect < 20.0f ) )
		return;
	if( std::fabs( newAspect - aspect ) < 1e-5f )
		return;
	// Keep particles where they were relative to the frame, so a resolution
	// change mid-show does not slide everything sideways.
	const float scale = newAspect / aspect;
	for( Particle& p : particles )
	{
		p.x *= scale;
		p.vx *= scale;
	}
	aspect = newAspect;
}

bool ParticleSystem::Room( const SparkleParams& params ) const
{
	return particles.size() + spawned.size() < static_cast< size_t >( std::max( 0, params.maxParticles ) );
}

Particle& ParticleSystem::Spawn()
{
	// While stepping, `particles` is being iterated by reference, so children
	// wait in a side buffer and join at the end of the step.
	std::vector< Particle >& into = stepping ? spawned : particles;
	into.emplace_back();
	return into.back();
}

float ParticleSystem::SizeJitter( const SparkleParams& params )
{
	return std::max( 0.15f, 1.0f + params.sizeVariation * random.Signed() * 0.85f );
}

RGB ParticleSystem::PaletteColour( const SparkleParams& params, float t )
{
	t = t - std::floor( t );
	switch( params.palette )
	{
	case Palette::Rainbow:
		return Hsv( t, 0.85f, 1.0f );
	case Palette::Gold:
	{
		const RGB deep  = { 1.0f, 0.55f, 0.12f };
		const RGB gold  = { 1.0f, 0.78f, 0.30f };
		const RGB white = { 1.0f, 0.93f, 0.72f };
		return t < 0.5f ? Lerp( deep, gold, t * 2.0f ) : Lerp( gold, white, ( t - 0.5f ) * 2.0f );
	}
	case Palette::Silver:
		return Lerp( { 0.78f, 0.85f, 1.0f }, { 1.0f, 1.0f, 1.0f }, t );
	case Palette::Fire:
		return Hsv( 0.0f + t * 0.13f, 0.92f - t * 0.25f, 1.0f );
	case Palette::Ice:
		return t < 0.8f ? Hsv( 0.5f + t * 0.2f, 0.75f, 1.0f ) : RGB{ 0.9f, 0.97f, 1.0f };
	case Palette::Pastel:
		return Hsv( t, 0.35f, 1.0f );
	case Palette::Neon:
	{
		static const RGB neon[] = {
			{ 1.0f, 0.10f, 0.80f }, { 0.10f, 1.0f, 1.0f }, { 0.55f, 1.0f, 0.10f },
			{ 0.25f, 0.35f, 1.0f }, { 1.0f, 0.95f, 0.10f },
		};
		return neon[ static_cast< int >( t * 5.0f ) % 5 ];
	}
	case Palette::TwoTone:
		return t < 0.5f ? params.colourA : params.colourB;
	case Palette::Single:
	default:
		return params.colourA;
	}
}

RGB ParticleSystem::PickColour( const SparkleParams& params, const RGB& burstColour )
{
	if( random.Chance( params.colourVariety ) )
		return PaletteColour( params, random.Unit() );
	return burstColour;
}

// ---------------------------------------------------------------------------
// Emission
// ---------------------------------------------------------------------------

void ParticleSystem::Fire( const SparkleParams& params )
{
	switch( params.effect )
	{
	case Effect::Fireworks:
		LaunchRocket( params );
		break;
	case Effect::Bursts:
	{
		const float x = ( params.positionX + random.Signed() * params.spread * 0.5f ) * aspect;
		const float y = params.positionY + random.Signed() * params.spread * 0.35f;
		Explode( params, std::clamp( x, 0.02f * aspect, 0.98f * aspect ), std::clamp( y, 0.05f, 0.95f ),
		         params.shape, false );
		break;
	}
	case Effect::ConfettiCannon:
		FireCannons( params );
		break;
	case Effect::Sparkles:
	{
		// A cloud at once, then the surge keeps the flow up for a moment.
		const int cloud = static_cast< int >( 20.0f + params.density * 180.0f );
		for( int i = 0; i < cloud && Room( params ); ++i )
			EmitSparkle( params );
		surge = 0.6f;
		++burstsFired;
		break;
	}
	default:
		surge = 0.6f;
		++burstsFired;
		break;
	}
}

void ParticleSystem::LaunchRocket( const SparkleParams& params )
{
	if( !Room( params ) )
		return;
	++burstsFired;

	const float target  = std::clamp( params.height + random.Signed() * 0.08f, 0.12f, 0.97f );
	const float g       = 0.55f;
	const float start   = -0.02f;
	const float vy      = std::sqrt( 2.0f * g * ( target - start ) );

	Particle& p = Spawn();
	p.kind       = Kind::Rocket;
	p.x          = std::clamp( params.positionX + random.Signed() * params.spread * 0.5f, 0.03f, 0.97f ) * aspect;
	p.y          = start;
	p.vx         = random.Signed() * 0.04f;
	p.vy         = vy;
	p.gravity    = g;
	p.drag       = 0.0f;
	// Fuse at the apex: a shell that bursts on the way up looks like a dud.
	p.life       = vy / g;
	p.size       = SIZE_ROCKET * SizeJitter( params );
	p.colour     = { 1.0f, 0.82f, 0.55f };
	p.flags      = FLAG_TRAIL | FLAG_FLICKER;
	p.burstShape = params.shape;
}

void ParticleSystem::Explode( const SparkleParams& params, float x, float y, Shape shape, bool fromRocket )
{
	if( !fromRocket )
		++burstsFired;

	if( shape == Shape::Random || shape >= Shape::COUNT )
		shape = static_cast< Shape >( random.Index( static_cast< int >( Shape::Random ) ) );

	const float speed     = 0.42f * params.speed * random.Range( 0.85f, 1.15f );
	const RGB   burstHue  = PaletteColour( params, random.Unit() );
	int         count     = static_cast< int >( 40.0f + params.density * 360.0f );

	// The burst itself: one big short flash, so the moment reads at a distance.
	if( Room( params ) )
	{
		Particle& f = Spawn();
		f.kind      = Kind::Flash;
		f.x         = x;
		f.y         = y;
		f.life      = 0.18f;
		f.size      = SIZE_FLASH * 7.0f;
		f.colour    = Lerp( burstHue, { 1.0f, 1.0f, 1.0f }, 0.6f );
	}

	auto star = [ & ]( float vx, float vy ) -> Particle* {
		if( !Room( params ) )
			return nullptr;
		Particle& p = Spawn();
		p.kind      = Kind::Spark;
		p.x         = x;
		p.y         = y;
		p.vx        = vx;
		p.vy        = vy;
		p.drag      = 1.6f;
		p.gravity   = 0.22f;
		p.life      = random.Range( 1.4f, 2.0f );
		p.size      = SIZE_SPARK * SizeJitter( params );
		p.colour    = PickColour( params, burstHue );
		p.flags     = FLAG_HOT;
		p.twinklePhase = random.Range( 0.0f, TWO_PI );
		p.twinkleRate  = random.Range( 18.0f, 30.0f );
		return &p;
	};

	switch( shape )
	{
	case Shape::Peony:
	case Shape::Crackle:
	case Shape::Willow:
	default:
	{
		const bool willow  = shape == Shape::Willow;
		const bool crackle = shape == Shape::Crackle;
		if( willow )
			count = count * 3 / 5;  // every star trails, so fewer stars
		for( int i = 0; i < count; ++i )
		{
			// A uniform direction on a sphere, seen from the front: stars bunch
			// at the rim like a real shell instead of filling a flat disc.
			const float z   = random.Signed();
			const float a   = random.Range( 0.0f, TWO_PI );
			const float r   = std::sqrt( std::max( 0.0f, 1.0f - z * z ) );
			const float s   = speed * ( willow ? 0.75f : 1.0f ) * random.Range( 0.92f, 1.0f );
			Particle*   p   = star( std::cos( a ) * r * s, std::sin( a ) * r * s );
			if( p == nullptr )
				break;
			if( willow )
			{
				p->drag    = 1.1f;
				p->gravity = 0.13f;
				p->life    = random.Range( 3.0f, 4.5f );
				p->colour  = Lerp( p->colour, { 1.0f, 0.72f, 0.28f }, 0.6f );
				p->flags  |= FLAG_TRAIL | FLAG_FLICKER;
			}
			else if( crackle )
			{
				p->life   = random.Range( 0.9f, 1.3f );
				p->colour = Lerp( p->colour, { 1.0f, 0.9f, 0.6f }, 0.5f );
				p->flags |= FLAG_CRACKLE;
			}
			else if( random.Chance( 0.2f ) )
			{
				p->flags |= FLAG_TRAIL;
			}
		}
		break;
	}
	case Shape::Ring:
	{
		const float tilt   = random.Range( 0.0f, PI );
		const float squash = random.Range( 0.25f, 1.0f );
		const float ct = std::cos( tilt ), st = std::sin( tilt );
		for( int i = 0; i < count; ++i )
		{
			const float a  = TWO_PI * ( static_cast< float >( i ) + random.Unit() * 0.3f ) / static_cast< float >( count );
			const float lx = std::cos( a ) * speed;
			const float ly = std::sin( a ) * speed * squash;
			if( star( lx * ct - ly * st, lx * st + ly * ct ) == nullptr )
				break;
		}
		break;
	}
	case Shape::Palm:
	{
		const int   arms   = 6 + random.Index( 4 );
		const float offset = random.Range( 0.0f, TWO_PI );
		const int   perArm = std::max( 4, count / arms );
		for( int arm = 0; arm < arms; ++arm )
		{
			const float a = offset + TWO_PI * static_cast< float >( arm ) / static_cast< float >( arms );
			for( int i = 0; i < perArm; ++i )
			{
				const float aj = a + random.Signed() * 0.05f;
				const float s  = speed * 1.1f * random.Range( 0.3f, 1.0f );
				Particle*   p  = star( std::cos( aj ) * s, std::sin( aj ) * s );
				if( p == nullptr )
					break;
				p->drag    = 1.3f;
				p->gravity = 0.28f;
				p->life    = random.Range( 1.6f, 2.4f );
				if( i % 3 == 0 )
					p->flags |= FLAG_TRAIL;
			}
		}
		break;
	}
	case Shape::Heart:
	{
		for( int i = 0; i < count; ++i )
		{
			const float t  = random.Range( 0.0f, TWO_PI );
			const float st = std::sin( t );
			const float hx = 16.0f * st * st * st / 17.0f;
			const float hy = ( 13.0f * std::cos( t ) - 5.0f * std::cos( 2.0f * t ) - 2.0f * std::cos( 3.0f * t ) -
			                   std::cos( 4.0f * t ) ) / 17.0f;
			const float j  = random.Range( 0.96f, 1.04f );
			Particle*   p  = star( hx * speed * j, ( hy + 0.1f ) * speed * j );
			if( p == nullptr )
				break;
			// Less gravity, so the outline survives long enough to be read.
			p->gravity = 0.10f;
		}
		break;
	}
	case Shape::Star:
	{
		const float spin = PI * 0.5f + random.Signed() * 0.25f;
		for( int i = 0; i < count; ++i )
		{
			// Walk the ten-vertex outline, alternating outer and inner points.
			const float u    = random.Unit() * 10.0f;
			const int   edge = static_cast< int >( u ) % 10;
			const float f    = u - std::floor( u );
			auto vertex = [ & ]( int k, float& vx, float& vy ) {
				const float radius = ( k % 2 == 0 ) ? 1.0f : 0.42f;
				const float a      = spin + TWO_PI * static_cast< float >( k ) / 10.0f;
				vx = std::cos( a ) * radius;
				vy = std::sin( a ) * radius;
			};
			float ax, ay, bx, by;
			vertex( edge, ax, ay );
			vertex( edge + 1, bx, by );
			Particle* p = star( ( ax + ( bx - ax ) * f ) * speed, ( ay + ( by - ay ) * f ) * speed );
			if( p == nullptr )
				break;
			p->gravity = 0.10f;
		}
		break;
	}
	}
}

void ParticleSystem::FireCannons( const SparkleParams& params )
{
	++burstsFired;
	const int perSide = static_cast< int >( 60.0f + params.density * 440.0f );
	for( int side = 0; side < 2; ++side )
	{
		const float x       = side == 0 ? 0.0f : aspect;
		const float inward  = side == 0 ? 1.0f : -1.0f;
		// 70 degrees from horizontal, opened out by Spread.
		const float base    = 70.0f * PI / 180.0f;
		const float cone    = ( 8.0f + params.spread * 25.0f ) * PI / 180.0f;
		for( int i = 0; i < perSide && Room( params ); ++i )
		{
			const float a = base + random.Signed() * cone;
			const float s = 2.6f * params.speed * random.Range( 0.55f, 1.0f );
			EmitConfettiFlake( params, x, -0.01f, std::cos( a ) * s * inward, std::sin( a ) * s );
		}
	}
}

void ParticleSystem::EmitConfettiFlake( const SparkleParams& params, float x, float y, float vx, float vy )
{
	Particle& p   = Spawn();
	p.kind        = Kind::Confetti;
	p.x           = x;
	p.y           = y;
	p.vx          = vx;
	p.vy          = vy;
	p.drag        = 2.4f;
	p.gravity     = 0.38f;
	p.life        = random.Range( 6.0f, 9.0f );
	p.size        = SIZE_CONFETTI * SizeJitter( params );
	p.aspect      = random.Range( 0.35f, 0.7f );
	p.rotation    = random.Range( 0.0f, TWO_PI );
	p.spin        = random.Signed() * 4.0f;
	p.flip        = random.Range( 0.0f, TWO_PI );
	p.flipRate    = random.Range( 4.0f, 11.0f ) * ( random.Chance( 0.5f ) ? 1.0f : -1.0f );
	p.colour      = PaletteColour( params, random.Unit() );
}

void ParticleSystem::EmitSparkle( const SparkleParams& params )
{
	Particle& p    = Spawn();
	p.kind         = Kind::Star;
	p.x            = ( params.positionX + random.Signed() * params.spread * 0.5f ) * aspect;
	p.y            = params.positionY + random.Signed() * params.spread * 0.5f;
	p.vx           = random.Signed() * 0.02f;
	p.vy           = random.Signed() * 0.02f;
	p.drag         = 0.5f;
	p.gravity      = 0.02f;
	p.life         = random.Range( 1.0f, 2.6f );
	p.size         = SIZE_STAR * SizeJitter( params );
	p.colour       = PaletteColour( params, random.Unit() );
	p.rotation     = random.Signed() * 0.3f;
	p.twinklePhase = random.Range( 0.0f, TWO_PI );
	p.twinkleRate  = random.Range( 5.0f, 12.0f );
}

void ParticleSystem::EmitFountainSpark( const SparkleParams& params )
{
	const float a = ( params.direction + random.Signed() * ( 4.0f + params.spread * 41.0f ) ) * PI / 180.0f;
	const float s = 0.85f * params.speed * random.Range( 0.75f, 1.0f );

	Particle& p    = Spawn();
	p.kind         = Kind::Spark;
	p.x            = params.positionX * aspect;
	p.y            = params.positionY;
	p.vx           = std::cos( a ) * s;
	p.vy           = std::sin( a ) * s;
	p.drag         = 0.7f;
	p.gravity      = 0.75f;
	p.life         = random.Range( 0.9f, 1.6f );
	p.size         = SIZE_SPARK * 0.8f * SizeJitter( params );
	p.colour       = PaletteColour( params, random.Unit() );
	p.flags        = FLAG_HOT | FLAG_FLICKER;
	p.twinklePhase = random.Range( 0.0f, TWO_PI );
	p.twinkleRate  = random.Range( 18.0f, 30.0f );
	if( random.Chance( 0.04f ) )
		p.flags |= FLAG_CRACKLE;
}

void ParticleSystem::EmitGlitter( const SparkleParams& params )
{
	Particle& p    = Spawn();
	p.kind         = Kind::Spark;
	p.x            = ( params.positionX + random.Signed() * params.spread * 0.5f ) * aspect;
	p.y            = 1.02f + random.Unit() * 0.03f;
	p.vx           = random.Signed() * 0.02f;
	p.vy           = -random.Range( 0.04f, 0.14f ) * params.speed;
	p.drag         = 0.6f;
	p.gravity      = 0.06f;
	p.life         = random.Range( 4.0f, 7.0f );
	p.size         = SIZE_SPARK * 0.9f * SizeJitter( params );
	p.colour       = PaletteColour( params, random.Unit() );
	p.flags        = FLAG_FLICKER;
	p.twinklePhase = random.Range( 0.0f, TWO_PI );
	p.twinkleRate  = random.Range( 6.0f, 16.0f );
}

void ParticleSystem::EmitStream( const SparkleParams& params, int count )
{
	for( int i = 0; i < count && Room( params ); ++i )
	{
		switch( params.effect )
		{
		case Effect::Sparkles:
			EmitSparkle( params );
			break;
		case Effect::ConfettiRain:
		{
			const float x = ( params.positionX + random.Signed() * params.spread * 0.5f ) * aspect;
			EmitConfettiFlake( params, x, 1.03f + random.Unit() * 0.05f, random.Signed() * 0.05f,
			                   -random.Range( 0.02f, 0.1f ) * params.speed );
			break;
		}
		case Effect::Fountain:
			EmitFountainSpark( params );
			break;
		case Effect::GlitterRain:
			EmitGlitter( params );
			break;
		default:
			return;
		}
	}
}

void ParticleSystem::ShedEmber( const SparkleParams& params, const Particle& from )
{
	Particle& p = Spawn();
	p.kind      = Kind::Ember;
	p.x         = from.x + random.Signed() * 0.002f;
	p.y         = from.y + random.Signed() * 0.002f;
	p.vx        = from.vx * 0.1f + random.Signed() * 0.03f;
	p.vy        = from.vy * 0.1f + random.Signed() * 0.03f;
	p.drag      = 2.0f;
	p.gravity   = 0.12f;
	p.life      = random.Range( 0.25f, 0.6f ) * ( from.kind == Kind::Rocket ? 1.0f : 1.6f );
	p.size      = SIZE_EMBER * SizeJitter( params );
	p.colour    = Lerp( from.colour, { 1.0f, 0.7f, 0.3f }, 0.5f );
	p.flags     = FLAG_FLICKER;
}

void ParticleSystem::Crackle( const SparkleParams& params, const Particle& from )
{
	const int pops = 2 + random.Index( 3 );
	for( int i = 0; i < pops && Room( params ); ++i )
	{
		Particle& p = Spawn();
		p.kind      = Kind::Flash;
		p.x         = from.x + random.Signed() * 0.012f;
		p.y         = from.y + random.Signed() * 0.012f;
		p.life      = random.Range( 0.05f, 0.13f );
		p.size      = SIZE_FLASH * random.Range( 0.8f, 1.4f );
		p.colour    = { 1.0f, 0.95f, 0.8f };
	}
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

void ParticleSystem::Step( const SparkleParams& params, float wallDt, const EmitEvents& events )
{
	if( events.clear )
		Clear();

	const float dt = std::clamp( wallDt, 0.0f, 0.25f ) * std::max( 0.0f, params.timeScale );
	simTime += dt;
	++stepCount;

	// --- emission ---
	for( int i = 0; i < events.bursts; ++i )
		Fire( params );

	if( IsBurstEffect( params.effect ) )
	{
		if( params.autoEmit && params.rate > 0.0f )
		{
			nextBurstIn -= dt;
			// Bounded: a huge dt (or a huge rate) must not stall the frame.
			for( int guard = 0; nextBurstIn <= 0.0f && guard < 16; ++guard )
			{
				Fire( params );
				// Jittered, so a steady rate does not tick like a metronome.
				nextBurstIn += random.Range( 0.6f, 1.4f ) / std::max( 0.02f, params.rate );
			}
			if( nextBurstIn <= 0.0f )
				nextBurstIn = 0.0f;
		}
	}
	else
	{
		float flow = params.autoEmit ? 1.0f : 0.0f;
		if( surge > 0.0f )
		{
			flow  = std::max( flow, 1.0f ) * 4.0f;
			surge = std::max( 0.0f, surge - dt );
		}
		streamCarry += StreamRate( params.effect, params.density ) * flow * dt;
		const int whole = static_cast< int >( std::min( streamCarry, 20000.0f ) );
		streamCarry -= static_cast< float >( whole );
		EmitStream( params, whole );
	}

	// Anything emitted above is already in `particles`; from here on children
	// are queued.
	stepping = true;

	const float lifeScale  = std::max( 0.05f, params.lifetime );
	const float windAccel  = params.wind * 0.35f;
	const float turbulence = params.turbulence * 0.6f;
	const bool  emberRoom  = particles.size() < static_cast< size_t >( params.maxParticles * EMBER_BUDGET );

	size_t alive = 0;
	for( size_t i = 0; i < particles.size(); ++i )
	{
		Particle& p = particles[ i ];
		// A rocket's fuse is set by the burst height, not by Lifetime.
		p.age += p.kind == Kind::Rocket ? dt : dt / lifeScale;

		bool dead = p.age >= p.life;

		if( !dead )
		{
			const float drag    = p.drag * params.drag;
			const float gravity = p.gravity * params.gravity;
			const bool  paper   = p.kind == Kind::Confetti;

			p.vy -= gravity * dt;
			p.vx += windAccel * dt * ( paper ? 1.5f : 1.0f );
			if( turbulence > 0.0f )
			{
				const float t = simTime;
				p.vx += turbulence * dt * std::sin( p.y * 9.0f + t * 1.3f + static_cast< float >( i % 7 ) );
				p.vy += turbulence * dt * std::cos( p.x * 8.0f - t * 1.1f );
			}
			if( paper )
			{
				// Flutter: a tumbling flake slides sideways as it turns.
				p.vx += std::sin( p.flip ) * 0.25f * dt;
				p.flip += p.flipRate * dt;
				p.rotation += p.spin * dt;
			}
			const float damping = std::exp( -drag * dt );
			p.vx *= damping;
			p.vy *= damping;
			p.x += p.vx * dt;
			p.y += p.vy * dt;

			// Rockets do not wait for Lifetime: their fuse is set by the height.
			if( p.kind == Kind::Rocket && p.vy <= 0.0f )
				dead = true;

			// Fallen out of frame for good.
			if( p.y < -0.15f || p.x < -0.3f * aspect || p.x > 1.3f * aspect )
			{
				dead = true;
				p.flags &= static_cast< std::uint8_t >( ~FLAG_CRACKLE );
				if( p.kind == Kind::Rocket )
					p.kind = Kind::Spark;  // no explosion off-screen
			}

			if( !dead && ( p.flags & FLAG_TRAIL ) && emberRoom )
			{
				const float perSecond = p.kind == Kind::Rocket ? EMBERS_PER_SECOND_ROCKET : EMBERS_PER_SECOND_SPARK;
				p.trailClock += dt * perSecond;
				while( p.trailClock >= 1.0f && Room( params ) )
				{
					p.trailClock -= 1.0f;
					ShedEmber( params, p );
				}
				if( p.trailClock >= 1.0f )
					p.trailClock = 0.0f;
			}
		}

		if( dead )
		{
			if( p.kind == Kind::Rocket )
				Explode( params, p.x, p.y, p.burstShape, true );
			else if( p.flags & FLAG_CRACKLE )
				Crackle( params, p );
			continue;
		}

		if( alive != i )
			particles[ alive ] = p;
		++alive;
	}
	particles.resize( alive );

	stepping = false;
	particles.insert( particles.end(), spawned.begin(), spawned.end() );
	spawned.clear();
}

// ---------------------------------------------------------------------------
// Instances
// ---------------------------------------------------------------------------

size_t ParticleSystem::BuildInstances( const SparkleParams& params, std::vector< Instance >& out )
{
	out.clear();
	out.reserve( particles.size() );

	const float sizeScale = std::max( 0.0f, params.size );
	const float twinkle   = Clamp01( params.twinkle );

	std::uint32_t index = 0;
	auto emit = [ & ]( const Particle& p ) {
		++index;
		const float t = p.life > 0.0f ? Clamp01( p.age / p.life ) : 1.0f;
		Instance    inst;
		inst.x  = p.x;
		inst.y  = p.y;
		inst.vx = p.vx;
		inst.vy = p.vy;

		RGB   c     = p.colour;
		float alpha = 1.0f;
		float sx    = p.size * sizeScale;
		float sy    = sx;

		switch( p.kind )
		{
		case Kind::Confetti:
			// Seen edge-on as it tumbles: width collapses and the face darkens.
			sx = p.size * sizeScale * std::max( 0.08f, std::fabs( std::cos( p.flip ) ) );
			sy = p.size * sizeScale * p.aspect;
			{
				const float shade = 0.55f + 0.45f * std::fabs( std::cos( p.flip ) );
				c = { c.r * shade, c.g * shade, c.b * shade };
			}
			alpha = 1.0f - std::max( 0.0f, ( t - 0.85f ) / 0.15f );
			break;
		case Kind::Flash:
			alpha = ( 1.0f - t ) * ( 1.0f - t );
			sx = sy = p.size * sizeScale * ( 1.0f + t * 0.6f );
			break;
		case Kind::Star:
		{
			const float s = 0.5f + 0.5f * std::sin( p.twinklePhase + p.age * p.twinkleRate );
			// Fade in and out so stars never pop.
			alpha = std::min( 1.0f, t * 6.0f ) * std::min( 1.0f, ( 1.0f - t ) * 3.0f );
			alpha *= ( 1.0f - twinkle ) + twinkle * ( s * s * s * 1.6f );
			break;
		}
		default:
		{
			// Sparks, rockets, embers.
			alpha = 1.0f - std::max( 0.0f, ( t - 0.55f ) / 0.45f );
			alpha *= alpha;
			if( p.kind != Kind::Rocket && t > 0.4f )
			{
				// The glittering end of a star's life.
				const float s = 0.5f + 0.5f * std::sin( p.twinklePhase + p.age * p.twinkleRate );
				alpha *= ( 1.0f - twinkle * 0.8f ) + twinkle * 0.8f * s * 1.3f;
			}
			if( p.flags & FLAG_HOT )
			{
				const float heat = 1.0f - std::min( 1.0f, t / 0.18f );
				c = Lerp( c, { 1.0f, 1.0f, 1.0f }, heat * 0.85f );
				sx = sy = sx * ( 1.0f + heat * 0.5f );
			}
			if( p.kind == Kind::Ember )
				alpha *= 0.6f;
			break;
		}
		}

		if( p.flags & FLAG_FLICKER )
			alpha *= 0.45f + 0.55f * Hash01( index, stepCount );

		inst.r        = c.r;
		inst.g        = c.g;
		inst.b        = c.b;
		inst.a        = Clamp01( alpha );
		inst.sizeX    = sx;
		inst.sizeY    = sy;
		inst.rotation = p.rotation;
		inst.kind     = static_cast< float >( p.kind );
		if( inst.a > 0.002f && sx > 0.0f )
			out.push_back( inst );
	};

	for( const Particle& p : particles )
		if( p.kind == Kind::Confetti )
			emit( p );
	const size_t confetti = out.size();
	for( const Particle& p : particles )
		if( p.kind != Kind::Confetti )
			emit( p );
	return confetti;
}

}  // namespace sparkles
