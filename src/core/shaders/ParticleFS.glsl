#version 410 core

// Writes premultiplied colour. Lights are blended additively (ONE, ONE), so
// overlapping sparks bloom toward white as real ones do; confetti is blended
// "over" (ONE, ONE_MINUS_SRC_ALPHA), because paper occludes.

in vec2      local;
in vec4      colour;
in float     tailLength;
flat in int  kind;

out vec4 fragColor;

uniform float Glow;

const int KIND_CONFETTI = 2;
const int KIND_STAR     = 3;
const int KIND_FLASH    = 4;

void main()
{
	if( kind == KIND_CONFETTI )
	{
		// Anti-aliased edge without derivatives of anything but `local`.
		vec2  edge     = fwidth( local ) * 1.5;
		vec2  inside   = smoothstep( vec2( 1.0 ), vec2( 1.0 ) - edge, abs( local ) );
		float coverage = inside.x * inside.y * colour.a;
		fragColor      = vec4( colour.rgb * coverage, coverage );
		return;
	}

	// Distance from the particle's path: zero along the tail segment behind
	// it, growing outside. Measured in core radii.
	float behind = clamp( -local.x, 0.0, tailLength );
	vec2  d2     = vec2( local.x + behind, local.y );
	float d      = length( d2 );

	// The tail dims along its length, so the head reads as the particle.
	float tailFade = tailLength > 0.0 ? 1.0 - 0.75 * ( behind / tailLength ) : 1.0;
	// A long tail spreads the same light over more pixels.
	float spread   = 1.0 / ( 1.0 + tailLength * 0.08 );

	float core = smoothstep( 1.0, 0.0, d );
	core      *= core;
	float halo = exp( -d * d / ( 1.0 + Glow * 6.0 ) * 1.2 ) * Glow * 0.55;

	float intensity = ( core + halo ) * tailFade * spread;

	if( kind == KIND_STAR )
	{
		// Four-point glint: two thin rays through the centre.
		vec2  a     = abs( local );
		float reach = 1.0 + Glow * 3.0;
		float rays  = exp( -a.y * 6.0 ) * max( 0.0, 1.0 - a.x / reach ) +
		              exp( -a.x * 6.0 ) * max( 0.0, 1.0 - a.y / reach );
		intensity  += rays * 0.7;
	}
	else if( kind == KIND_FLASH )
	{
		intensity = exp( -d * d * 1.5 ) + halo;
	}

	// White-hot centre: the core saturates toward white before the colour.
	vec3  tint = mix( colour.rgb, vec3( 1.0 ), clamp( core - 0.55, 0.0, 1.0 ) * 0.9 );
	float a    = intensity * colour.a;
	fragColor  = vec4( tint * a, a );
}
