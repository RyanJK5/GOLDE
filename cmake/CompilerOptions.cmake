add_library(gol_compiler_options INTERFACE)

if(MSVC)
    target_compile_options(gol_compiler_options INTERFACE 
        /W4 
        /FS
    )
    
    target_compile_options(gol_compiler_options INTERFACE
        $<$<CONFIG:Debug>:/WX;/Od;/Zi;/RTC1>
        $<$<CONFIG:Release>:/WX-;/O2;/DNDEBUG>
    )

    target_link_options(gol_compiler_options INTERFACE
        $<$<CONFIG:Debug>:/INCREMENTAL:NO>
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gol_compiler_options INTERFACE 
        $<$<CXX_COMPILER_ID:GNU>:-Wno-nontrivial-memcall>
    )

    target_compile_options(gol_compiler_options INTERFACE
        $<$<CONFIG:Debug>:-g3;-O0;-Wall;-Wextra;-Wpedantic;-Werror;-fno-omit-frame-pointer>
        $<$<CONFIG:Release>:-O3;-DNDEBUG;-Wno-error>
    )

    if(WIN32)
        # Remove problematic flags that prevent C runtime linking on Windows
        string(REPLACE "-nostartfiles" "" CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
        string(REPLACE "-nostdlib" "" CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE}")
    endif()
endif()
