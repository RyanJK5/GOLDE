#include <filesystem>

#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
    if (argc > 0) {
        const auto executablePath = std::filesystem::absolute(argv[0]);
        std::filesystem::current_path(executablePath.parent_path());
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}