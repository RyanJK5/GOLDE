#include <filesystem>

#include "Game.hpp"

int main(int argc, char* argv[]) {
    if (argc > 0) {
        const auto executablePath = std::filesystem::absolute(argv[0]);
        std::filesystem::current_path(executablePath.parent_path());
    }

    gol::Game game{};
    game.Begin();
}