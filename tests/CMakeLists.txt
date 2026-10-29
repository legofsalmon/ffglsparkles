# Headless GL backend. EGL is the only one that works on a CI machine with no
# display server, which is where these tests actually earn their keep.
set( SPARKLES_TEST_BACKEND "" )

if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    find_package( PkgConfig QUIET )
    if( PkgConfig_FOUND )
        pkg_check_modules( EGL egl )
    endif()
    if( EGL_FOUND )
        set( SPARKLES_TEST_BACKEND "EGL" )
    endif()
endif()

if( NOT SPARKLES_TEST_BACKEND )
    find_package( glfw3 QUIET )
    if( glfw3_FOUND )
        set( SPARKLES_TEST_BACKEND "GLFW" )
    endif()
endif()

if( NOT SPARKLES_TEST_BACKEND )
    message( WARNING
        "No headless OpenGL backend found (EGL on Linux, glfw3 elsewhere). "
        "Skipping tests; configure with -DSPARKLES_BUILD_TESTS=OFF to silence this." )
    return()
endif()

add_executable( sparkles_tests
    main.cpp
    test_simulation.cpp
    test_render.cpp
    test_plugin.cpp
    harness/GLContext.h   harness/GLContext.cpp
    harness/TestRunner.h
    harness/Readback.h
)

target_link_libraries( sparkles_tests PRIVATE sparkles::core sparkles::ffgl_sdk )
target_include_directories( sparkles_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    # The plugin class lives in a header so this can drive it; only the
    # CFFGLPluginInfo registration stays in the .cpp, once per binary.
    "${CMAKE_SOURCE_DIR}/src/plugins" )

if( SPARKLES_TEST_BACKEND STREQUAL "EGL" )
    target_compile_definitions( sparkles_tests PRIVATE SPARKLES_TEST_EGL )
    target_include_directories( sparkles_tests PRIVATE ${EGL_INCLUDE_DIRS} )
    target_link_libraries( sparkles_tests PRIVATE ${EGL_LIBRARIES} )
else()
    target_compile_definitions( sparkles_tests PRIVATE SPARKLES_TEST_GLFW )
    target_link_libraries( sparkles_tests PRIVATE glfw )
endif()

if( MSVC )
    target_compile_options( sparkles_tests PRIVATE /W4 )
else()
    target_compile_options( sparkles_tests PRIVATE -Wall -Wextra )
endif()

add_test( NAME sparkles_tests COMMAND sparkles_tests )

message( STATUS "  test backend : ${SPARKLES_TEST_BACKEND}" )
