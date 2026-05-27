#include <filesystem>

#include "Game.hpp"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

static void SetWorkingDirectory(int argc, char* argv[]) {
#ifdef __APPLE__
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle) {
        CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
        if (resourcesURL) {
            char path[PATH_MAX];
            // Convert the bundle URL into a standard POSIX C-string path
            if (CFURLGetFileSystemRepresentation(resourcesURL, true,
                                                 (UInt8*)path, PATH_MAX)) {
                std::filesystem::current_path(path);
                CFRelease(resourcesURL);
                return; // Successfully changed to the bundle's Resources folder
            }
            CFRelease(resourcesURL);
        }
    }
#endif
    if (argc > 0) {
        constexpr static auto searchDepth = 2;
        auto path = std::filesystem::absolute(argv[0]);
        for (auto i = 0; i < searchDepth; i++) {
            path /= "..";
            const auto sharePath = path / "share";
            if (std::filesystem::exists(sharePath)) {
                std::filesystem::current_path(sharePath);
                break;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    SetWorkingDirectory(argc, argv);
    Golde::Game game{};
    game.Begin();
}