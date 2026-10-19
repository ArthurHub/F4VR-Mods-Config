#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

// Files on disk for the tests.
namespace test_files
{
    /**
     * A folder of its own for one test, removed with all that is in it when the test ends.
     */
    struct TempFolder
    {
        std::filesystem::path path;

        TempFolder()
        {
            static std::atomic counter = 0;
            const auto time = std::chrono::steady_clock::now().time_since_epoch().count();
            path = std::filesystem::temp_directory_path() / ("ModsConfigVR_tests_" + std::to_string(time) + "_" + std::to_string(counter++));
            std::filesystem::create_directories(path);
        }

        TempFolder(const TempFolder&) = delete;
        TempFolder& operator=(const TempFolder&) = delete;

        ~TempFolder()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }

        /**
         * Writes a file by its path from the folder, with the folders on the way to it.
         */
        void write(const std::filesystem::path& file, const std::string& text) const
        {
            std::filesystem::create_directories((path / file).parent_path());
            std::ofstream(path / file, std::ios::binary | std::ios::trunc) << text;
        }
    };

    /**
     * A file that cannot be written while this lives, and can again after. Windows makes a file read-only only
     * when it has no write permission for anyone.
     */
    struct ReadOnly
    {
        static constexpr auto WRITE = std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write;

        std::filesystem::path path;

        explicit ReadOnly(std::filesystem::path file)
            : path(std::move(file))
        {
            std::filesystem::permissions(path, WRITE, std::filesystem::perm_options::remove);
        }

        ReadOnly(const ReadOnly&) = delete;
        ReadOnly& operator=(const ReadOnly&) = delete;

        ~ReadOnly()
        {
            std::error_code error;
            std::filesystem::permissions(path, WRITE, std::filesystem::perm_options::add, error);
        }
    };
}
