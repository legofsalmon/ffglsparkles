add_library( sparkles_core OBJECT
    GL.h               GL.cpp
    RenderTarget.h     RenderTarget.cpp
    Random.h
    SparkleParams.h
    ParticleSystem.h   ParticleSystem.cpp
    ParticleRenderer.h ParticleRenderer.cpp
)
add_library( sparkles::core ALIAS sparkles_core )

target_include_directories( sparkles_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" )

# The version, compiled in from the one place it is declared (the project()
# call). FFGL's plugin info has room for MAJOR and MINOR only; the full
# three-part version rides in the description string, the Windows version
# resource and the macOS Info.plist.
target_compile_definitions( sparkles_core PUBLIC
    SPARKLES_VERSION="${PROJECT_VERSION}"
    SPARKLES_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    SPARKLES_VERSION_MINOR=${PROJECT_VERSION_MINOR}
)
target_link_libraries( sparkles_core PUBLIC sparkles::ffgl_sdk )
set_target_properties( sparkles_core PROPERTIES POSITION_INDEPENDENT_CODE ON )

# Shaders are compiled into the binary. A plugin that needs files sitting next
# to it breaks the first time someone copies just the .dll into Resolume.
sparkles_embed_shaders( sparkles_core
    shaders/ScreenQuad.glsl
    shaders/Trail.glsl
    shaders/ParticleVS.glsl
    shaders/ParticleFS.glsl
    shaders/Composite.glsl
)

if( MSVC )
    target_compile_options( sparkles_core PRIVATE /W4 )
else()
    target_compile_options( sparkles_core PRIVATE -Wall -Wextra )
endif()
