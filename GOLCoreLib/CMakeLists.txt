set(SOURCES
    src/EditorCommandExecutor.cpp
    src/EditorModel.cpp
    src/Game.cpp
    src/PresetSelection.cpp
    src/SimulationControl.cpp
    src/SimulationEditor.cpp
    src/SimulationWorker.cpp
)

set(HEADERS
    include/EditorCommandExecutor.hpp
    include/EditorModel.hpp
    include/Game.hpp
    include/PresetSelection.hpp
    include/SimulationControl.hpp
    include/SimulationEditor.hpp
    include/SimulationWorker.hpp
)

add_library(GOLCoreLib STATIC ${SOURCES} ${HEADERS})

target_include_directories(GOLCoreLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(GOLCoreLib
    PRIVATE
        gol_compiler_options
    PUBLIC
        GOLAlgoLib
        GOLGraphicsLib
        GOLGuiLib
        GOLLoggingLib
        stb_image
        glew
        glfw
)