set(SOURCES
    src/PatternIO.cpp
)

set(HEADERS
    include/PatternIO.hpp
)

add_library(GOLIOLib STATIC ${SOURCES} ${HEADERS})

target_include_directories(GOLIOLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(GOLIOLib
    PRIVATE
        gol_compiler_options
    PUBLIC
        GOLAlgoLib
)
