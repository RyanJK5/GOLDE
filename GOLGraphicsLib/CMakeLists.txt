set(SOURCES
    src/Camera.cpp
    src/GLDebug.cpp
    src/GraphicsHandler.cpp
    src/ShaderManager.cpp
)

set(HEADERS
    include/Camera.hpp
    include/GLBuffer.hpp
    include/GLDebug.hpp
    include/GLException.hpp
    include/GraphicsHandler.hpp
    include/GraphicsInterop.hpp
    include/ShaderManager.hpp
)


add_library(GOLGraphicsLib STATIC ${SOURCES} ${HEADERS} )

target_include_directories(GOLAlgoLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(GOLGraphicsLib
    PRIVATE
        gol_compiler_options
    PUBLIC
        glm
        glew
        glfw
        imgui_lib
        stb_image
        OpenGL::GL
        GOLAlgoLib
)

# GLEW needs GLEW_STATIC defined
target_compile_definitions(GOLGraphicsLib PUBLIC GLEW_STATIC)