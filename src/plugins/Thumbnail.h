#pragma once

#include <ffgl/FFGLThumbnailInfo.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace sparkles {

/// Draws the browser thumbnail from scratch: a firework bursting over a night
/// sky with a scatter of glints. CFFGLThumbnailInfo takes raw pixels, so this
/// needs no image decoder and no files next to the binary.
namespace thumbnail {

/// Resolume draws thumbnails at 160x120, so matching it avoids a resample.
constexpr FFUInt32 WIDTH  = 160;
constexpr FFUInt32 HEIGHT = 120;

namespace detail {

inline float Hash( int i, int salt )
{
	std::uint32_t h = static_cast< std::uint32_t >( i ) * 374761393u + static_cast< std::uint32_t >( salt ) * 668265263u;
	h = ( h ^ ( h >> 13 ) ) * 1274126177u;
	h = h ^ ( h >> 16 );
	return static_cast< float >( h & 0xFFFFFFu ) / static_cast< float >( 0xFFFFFF );
}

inline unsigned char ToByte( float value )
{
	const float clamped = value < 0.0f ? 0.0f : ( value > 1.0f ? 1.0f : value );
	return static_cast< unsigned char >( clamped * 255.0f + 0.5f );
}

}  // namespace detail

inline std::vector< CFFGLColor > Generate()
{
	const int w = static_cast< int >( WIDTH );
	const int h = static_cast< int >( HEIGHT );
	std::vector< float > r( w * h ), g( w * h ), b( w * h );

	// Night sky, lighter at the horizon.
	for( int y = 0; y < h; ++y )
		for( int x = 0; x < w; ++x )
		{
			const float v       = static_cast< float >( y ) / static_cast< float >( h );
			r[ y * w + x ]      = 0.02f + 0.05f * v;
			g[ y * w + x ]      = 0.02f + 0.04f * v;
			b[ y * w + x ]      = 0.08f + 0.10f * v;
		}

	auto splat = [ & ]( float cx, float cy, float radius, float cr, float cg, float cb ) {
		const int x0 = static_cast< int >( cx - radius * 3.0f ), x1 = static_cast< int >( cx + radius * 3.0f );
		const int y0 = static_cast< int >( cy - radius * 3.0f ), y1 = static_cast< int >( cy + radius * 3.0f );
		for( int y = y0; y <= y1; ++y )
			for( int x = x0; x <= x1; ++x )
			{
				if( x < 0 || y < 0 || x >= w || y >= h )
					continue;
				const float dx = ( static_cast< float >( x ) - cx ) / radius;
				const float dy = ( static_cast< float >( y ) - cy ) / radius;
				const float k  = std::exp( -( dx * dx + dy * dy ) );
				r[ y * w + x ] += cr * k;
				g[ y * w + x ] += cg * k;
				b[ y * w + x ] += cb * k;
			}
	};

	// Two shells: gold on the left, magenta-blue on the right. Each star is
	// drawn as a short streak of dots leading out from the centre.
	struct Shell { float x, y, radius, r, g, b; int stars; };
	const Shell shells[] = { { 58.0f, 50.0f, 34.0f, 1.0f, 0.75f, 0.3f, 36 },
	                         { 112.0f, 40.0f, 26.0f, 0.9f, 0.3f, 1.0f, 28 } };
	int seed = 0;
	for( const Shell& s : shells )
	{
		splat( s.x, s.y, 5.0f, s.r * 0.6f, s.g * 0.6f, s.b * 0.6f );
		for( int i = 0; i < s.stars; ++i, ++seed )
		{
			const float a    = 6.2831853f * ( static_cast< float >( i ) + detail::Hash( seed, 1 ) * 0.5f ) / static_cast< float >( s.stars );
			const float dist = s.radius * ( 0.8f + 0.2f * detail::Hash( seed, 2 ) );
			for( int t = 0; t < 6; ++t )
			{
				const float f = 0.55f + 0.09f * static_cast< float >( t );
				const float k = 0.15f + 0.17f * static_cast< float >( t );
				// Gravity bends the tips down a little (image y grows downward).
				splat( s.x + std::cos( a ) * dist * f, s.y + std::sin( a ) * dist * f + f * f * 6.0f, 0.9f,
				       s.r * k, s.g * k, s.b * k );
			}
		}
	}

	// Glints along the bottom.
	for( int i = 0; i < 26; ++i )
	{
		const float x = detail::Hash( i, 7 ) * static_cast< float >( w );
		const float y = 80.0f + detail::Hash( i, 8 ) * 36.0f;
		const float k = 0.4f + 0.6f * detail::Hash( i, 9 );
		splat( x, y, 0.8f, k, k * 0.95f, k * 0.8f );
	}

	std::vector< CFFGLColor > pixels;
	pixels.reserve( static_cast< size_t >( w * h ) );
	for( int i = 0; i < w * h; ++i )
		pixels.emplace_back( detail::ToByte( r[ i ] ), detail::ToByte( g[ i ] ), detail::ToByte( b[ i ] ),
		                     static_cast< unsigned char >( 255 ) );
	return pixels;
}

}  // namespace thumbnail
}  // namespace sparkles
