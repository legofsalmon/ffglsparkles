#pragma once

#include <cstdint>

namespace sparkles {

/// PCG32: small, fast and, unlike std::mt19937 with std::uniform_real_distribution,
/// guaranteed to produce the same sequence on every compiler and standard
/// library. The Seed parameter is a promise that a look can be reproduced, and
/// that promise has to hold between the Windows and macOS builds.
class Random
{
public:
	explicit Random( std::uint64_t seed = 1 ) { Seed( seed ); }

	void Seed( std::uint64_t seed )
	{
		state = 0;
		Next();
		state += seed + 0x853c49e6748fea9bULL;
		Next();
	}

	std::uint32_t Next()
	{
		const std::uint64_t old = state;
		state                   = old * 6364136223846793005ULL + 1442695040888963407ULL;
		const auto xorshifted   = static_cast< std::uint32_t >( ( ( old >> 18u ) ^ old ) >> 27u );
		const auto rot          = static_cast< std::uint32_t >( old >> 59u );
		return ( xorshifted >> rot ) | ( xorshifted << ( ( 32u - rot ) & 31u ) );
	}

	/// Uniform in [0, 1).
	float Unit() { return static_cast< float >( Next() >> 8 ) * ( 1.0f / 16777216.0f ); }

	/// Uniform in [lo, hi).
	float Range( float lo, float hi ) { return lo + ( hi - lo ) * Unit(); }

	/// Uniform in [-1, 1).
	float Signed() { return Unit() * 2.0f - 1.0f; }

	/// Uniform integer in [0, count).
	int Index( int count ) { return count <= 0 ? 0 : static_cast< int >( Unit() * static_cast< float >( count ) ) % count; }

	bool Chance( float probability ) { return Unit() < probability; }

private:
	std::uint64_t state = 0;
};

}  // namespace sparkles
