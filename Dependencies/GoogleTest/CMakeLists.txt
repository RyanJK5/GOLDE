cmake_minimum_required(VERSION 3.25)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.16.0.zip
  SYSTEM
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)


FetchContent_MakeAvailable(googletest)

# Enable testing for the parent project if it hasn't been already
enable_testing()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	set_target_properties(gtest PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:gtest,INTERFACE_INCLUDE_DIRECTORIES>)
	target_compile_options(gtest PRIVATE -Wno-character-conversion -Wno-error)
	target_compile_options(gtest_main PRIVATE -Wno-character-conversion -Wno-error)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	target_compile_options(gtest PRIVATE /WX-)
	target_compile_options(gtest_main PRIVATE /WX-)
endif()