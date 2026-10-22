#include "harness/GLContext.h"
#include "harness/TestRunner.h"

#include <GL.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace sparkles::test {
void RunBenchmark();
void RunSnapshots( const char* directory );
}

int main( int argc, char** argv )
{
	const bool benchmark = ( argc > 1 && std::strcmp( argv[ 1 ], "--profile" ) == 0 );

	sparkles::test::GLContext context;
	if( !context.Create() )
	{
		std::printf( "FATAL: %s\n", context.GetError().c_str() );
		return 2;
	}
	std::printf( "context: %s\n\n", context.Describe().c_str() );

	if( benchmark )
	{
		sparkles::test::RunBenchmark();
		return 0;
	}
	if( argc > 2 && std::strcmp( argv[ 1 ], "--snapshots" ) == 0 )
	{
		sparkles::test::RunSnapshots( argv[ 2 ] );
		return 0;
	}

	auto& registry = sparkles::test::Registry::Instance();

	int passed = 0;
	int failed = 0;

	for( const auto& testCase : registry.Cases() )
	{
		// Errors latched by a previous test would otherwise be blamed on this one.
		sparkles::FlushGLErrors();

		testCase.body();

		auto failures = registry.TakeFailures();

		// A GL error is a failure even when every assertion passed: it means a
		// pass issued an illegal call and the driver quietly ignored it.
		if( !sparkles::CheckGL( testCase.name.c_str() ) )
			failures.push_back( "  left an OpenGL error behind" );

		if( failures.empty() )
		{
			std::printf( "PASS  %s\n", testCase.name.c_str() );
			++passed;
		}
		else
		{
			std::printf( "FAIL  %s\n", testCase.name.c_str() );
			for( const auto& failure : failures )
				std::printf( "%s\n", failure.c_str() );
			++failed;
		}
	}

	std::printf( "\n%d passed, %d failed\n", passed, failed );
	return failed == 0 ? 0 : 1;
}
