#version 410 core

// The trail. This frame's particles are drawn on their own into Current; here
// they are merged with last frame's picture, dimmed.
//
// Merged with max, not added. Adding would make anything that holds still (a
// fountain's nozzle, a heart that has stopped expanding) build up to
// 1 / (1 - Keep) times its own brightness, which at a long trail is thirty
// times over and burns out to a white blob. With max, a trail is never
// brighter than the light that left it. Both inputs are valid premultiplied
// colour, and so is their component-wise max.
//
// The subtraction matters too: multiplying alone never reaches zero in a
// half-float target. Once a value is small enough that `v * Keep` rounds back
// to `v`, it stays forever as a faint permanent haze over the layers beneath.

in vec2 uv;
out vec4 fragColor;

uniform sampler2D Previous;
uniform sampler2D Current;
uniform float     Keep;   // 0 = no trail, just under 1 = long trail

void main()
{
	vec4 faded = max( texture( Previous, uv ) * Keep - vec4( 1.0 / 512.0 ), vec4( 0.0 ) );
	fragColor  = max( faded, texture( Current, uv ) );
}
