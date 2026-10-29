#version 410 core

// Accumulated light to the host's framebuffer.
//
// Resolume expects premultiplied colour from a plugin, in range: rgb never
// above alpha, alpha never above 1. Additive light breaks both (overlapping
// sparks sum past 1), so the alpha is raised to cover the brightest channel
// and everything is clamped. On a transparent background that makes a white
// spark opaque white and a faint ember a faint, mostly transparent glow: an
// overlay that looks the same over black as it does on its own.

in vec2 uv;
out vec4 fragColor;

uniform sampler2D Accumulated;
uniform float     Brightness;
uniform float     Opaque;   // 1 = composite over black, 0 = keep transparency

void main()
{
	vec4  light = max( texture( Accumulated, uv ), vec4( 0.0 ) );
	vec3  rgb   = min( light.rgb * Brightness, vec3( 1.0 ) );
	float alpha = clamp( max( light.a * min( Brightness, 1.0 ), max( rgb.r, max( rgb.g, rgb.b ) ) ), 0.0, 1.0 );
	rgb         = min( rgb, vec3( alpha ) );
	fragColor   = mix( vec4( rgb, alpha ), vec4( rgb, 1.0 ), Opaque );
}
