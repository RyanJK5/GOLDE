include(FetchContent)

# Boost.Multiprecision
FetchContent_Declare(
    boost_multiprecision
    GIT_REPOSITORY https://github.com/boostorg/multiprecision.git
    GIT_TAG master
)
FetchContent_Declare(
    boost_config
    GIT_REPOSITORY https://github.com/boostorg/config.git
    GIT_TAG master
)

FetchContent_MakeAvailable(boost_multiprecision boost_config)

# Force the include directories to be treated as SYSTEM
if(TARGET boost_multiprecision)
    set_target_properties(boost_multiprecision PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES 
        $<TARGET_PROPERTY:boost_multiprecision,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

# FontAwesome
add_library(fontawesome INTERFACE)
target_include_directories(fontawesome INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/FontAwesome)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG master
)
FetchContent_MakeAvailable(glm)

# unordered_dense
FetchContent_Declare(
    unordered_dense
    GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
    GIT_TAG v4.4.0
)
FetchContent_MakeAvailable(unordered_dense)

# stb_image
set(STB_IMAGE_DIR ${CMAKE_BINARY_DIR}/_deps/stb_image-src)
if(NOT EXISTS "${STB_IMAGE_DIR}/stb_image.h")
    file(DOWNLOAD https://raw.githubusercontent.com/nothings/stb/master/stb_image.h 
         "${STB_IMAGE_DIR}/stb_image.h"
         STATUS download_status)
endif()
add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${STB_IMAGE_DIR})

# GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
  SYSTEM
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	set_target_properties(gtest PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:gtest,INTERFACE_INCLUDE_DIRECTORIES>)
	target_compile_options(gtest PRIVATE -Wno-character-conversion -Wno-error)
	target_compile_options(gtest_main PRIVATE -Wno-character-conversion -Wno-error)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(gtest PRIVATE /WX-)
	target_compile_options(gtest_main PRIVATE /WX-)
endif()

# Fetch GLFW from source
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch GLEW and build from the single source file
FetchContent_Declare(
    glew_fetch
    URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip
)
FetchContent_GetProperties(glew_fetch)
if(NOT glew_fetch_POPULATED)
    FetchContent_Populate(glew_fetch)
    # Treat glew.c as C++ so we don't require the user to configure a C compiler
    set_source_files_properties(${glew_fetch_SOURCE_DIR}/src/glew.c PROPERTIES LANGUAGE CXX)
    add_library(glew STATIC ${glew_fetch_SOURCE_DIR}/src/glew.c)
    target_include_directories(glew SYSTEM PUBLIC ${glew_fetch_SOURCE_DIR}/include)
    target_compile_definitions(glew PUBLIC GLEW_STATIC)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(glew PRIVATE -Wno-error)
    endif()
    if(WIN32)
        target_link_libraries(glew PUBLIC opengl32)
    endif()
endif()
message(STATUS "GLEW configured from source")

# Fetch NFD-Extended
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG v1.2.1
)
set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nfd)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nfd PRIVATE -Wno-error)
endif()

add_subdirectory(ImGUI)