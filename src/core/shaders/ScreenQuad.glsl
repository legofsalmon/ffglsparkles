#version 410 core

// Shared vertex stage for the full-screen passes. ffglex::FFGLScreenQuad
// supplies location 0 = position, location 1 = uv in 0..1 with v increasing upward.

layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;

out vec2 uv;

void main()
{
	gl_Position = vPosition;
	uv          = vUV;
}
