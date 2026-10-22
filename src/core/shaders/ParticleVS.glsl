#version 410 core

// One instanced quad per particle. The CPU decides what each particle is; this
// only places and stretches the quad.

layout( location = 0 ) in vec2 Corner;   // -1..1, the quad's own corners
layout( location = 1 ) in vec4 PosVel;   // world x, y, vx, vy
layout( location = 2 ) in vec4 Colour;   // straight rgb, alpha
layout( location = 3 ) in vec4 Shape;    // size x, size y, rotation, kind

uniform float Aspect;   // world width in screen heights
uniform float Glow;     // 0..1
uniform float Streak;   // 0..1

out vec2      local;    // position inside the particle, in units of its core radius
out vec4      colour;
out float     tailLength;
flat out int  kind;

const int KIND_CONFETTI = 2;

void main()
{
	kind   = int( Shape.w + 0.5 );
	colour = Colour;

	vec2 centre = PosVel.xy;
	vec2 offset;

	if( kind == KIND_CONFETTI )
	{
		// A rotated rectangle; the CPU already narrowed it for tumbling.
		float c = cos( Shape.z ), s = sin( Shape.z );
		vec2  extent = Corner * Shape.xy;
		offset     = vec2( c * extent.x - s * extent.y, s * extent.x + c * extent.y );
		local      = Corner;
		tailLength = 0.0;
	}
	else
	{
		// Light: the quad covers the halo, which grows with Glow, and is
		// stretched backwards along the velocity for motion blur.
		float radius = Shape.x;
		float reach  = 1.0 + Glow * 3.0;
		vec2  vel    = PosVel.zw;
		float speed  = length( vel );
		vec2  dir    = speed > 1e-5 ? vel / speed : vec2( 1.0, 0.0 );
		vec2  perp   = vec2( -dir.y, dir.x );

		// In core radii. Capped so a fast spark does not become a laser.
		float tail = min( speed * Streak * 0.06 / max( radius, 1e-5 ), 24.0 );
		tailLength = tail;

		// The quad spans [-reach - tail, reach] along the motion: the tail
		// trails behind, the head stays round.
		float along  = mix( -reach - tail, reach, Corner.x * 0.5 + 0.5 );
		float across = Corner.y * reach;
		local        = vec2( along, across );
		offset       = ( dir * along + perp * across ) * radius;
	}

	vec2 world  = centre + offset;
	gl_Position = vec4( world.x / Aspect * 2.0 - 1.0, world.y * 2.0 - 1.0, 0.0, 1.0 );
}
