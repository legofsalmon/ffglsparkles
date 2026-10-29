#pragma once

#include <GL.h>
#include <RenderTarget.h>

#include <cstdint>
#include <vector>

namespace sparkles::test {

/// An RGBA8 framebuffer standing in for the host's. Release() before the
/// context goes away.
struct HostFramebuffer
{
	RenderTarget target;

	bool Allocate( GLsizei width, GLsizei height ) { return target.Allocate( width, height, GL_RGBA8 ); }
	GLuint FBO() const { return target.GetFBO(); }

	/// Every pixel as RGBA bytes, bottom row first.
	std::vector< std::uint8_t > Read() const
	{
		std::vector< std::uint8_t > pixels( static_cast< size_t >( target.GetWidth() ) * target.GetHeight() * 4 );
		GLint previous = 0;
		glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &previous );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, target.GetFBO() );
		glPixelStorei( GL_PACK_ALIGNMENT, 1 );
		glReadPixels( 0, 0, target.GetWidth(), target.GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data() );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, static_cast< GLuint >( previous ) );
		return pixels;
	}
};

/// A half-float render target's contents as floats.
inline std::vector< float > ReadFloat( const RenderTarget& target )
{
	std::vector< float > pixels( static_cast< size_t >( target.GetWidth() ) * target.GetHeight() * 4 );
	GLint previous = 0;
	glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &previous );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, target.GetFBO() );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glReadPixels( 0, 0, target.GetWidth(), target.GetHeight(), GL_RGBA, GL_FLOAT, pixels.data() );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, static_cast< GLuint >( previous ) );
	return pixels;
}

struct PixelStats
{
	int  lit          = 0;  ///< Pixels with any alpha.
	int  opaque       = 0;  ///< Pixels with alpha 255.
	int  badPremult   = 0;  ///< Pixels whose colour exceeds their alpha.
	int  maxAlpha     = 0;
};

inline PixelStats Analyse( const std::vector< std::uint8_t >& rgba )
{
	PixelStats stats;
	for( size_t i = 0; i + 3 < rgba.size(); i += 4 )
	{
		const int r = rgba[ i ], g = rgba[ i + 1 ], b = rgba[ i + 2 ], a = rgba[ i + 3 ];
		if( a > 0 )
			++stats.lit;
		if( a == 255 )
			++stats.opaque;
		// One step of slack for 8-bit rounding.
		if( r > a + 1 || g > a + 1 || b > a + 1 )
			++stats.badPremult;
		if( a > stats.maxAlpha )
			stats.maxAlpha = a;
	}
	return stats;
}

}  // namespace sparkles::test
