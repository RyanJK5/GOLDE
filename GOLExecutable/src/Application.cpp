#include <filesystem>

#include "Game.hpp"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

int main(int argc, char* argv[]) {
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
        const auto executablePath = std::filesystem::absolute(argv[0]);
        std::filesystem::current_path(
            executablePath.parent_path().parent_path() / "share");
    }

    Golde::Game game{};
    game.Begin();
}