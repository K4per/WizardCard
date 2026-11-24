#include "wizard/content.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace wizard {
namespace {
void writeJsonFile(const std::filesystem::path &path, const Json &j, bool replace) {
    std::error_code filesystemError;
    std::filesystem::create_directories(path.parent_path(), filesystemError);
    if (filesystemError) throw std::runtime_error("无法创建用户数据目录：" + filesystemError.message());
    static std::atomic<std::uint64_t> serial{};
    std::filesystem::path staging;
    do {
        staging = path.parent_path() / (".wizardcard-tmp-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
            std::to_string(serial++));
    } while (!std::filesystem::create_directory(staging));
    const auto temporary = staging / "data";
    try {
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file)
                throw std::runtime_error("无法写入用户数据文件");
            file << j.dump(2) << '\n';
            file.flush();
            if (!file)
                throw std::runtime_error("用户数据写入失败");
            file.close();
            if (file.fail())
                throw std::runtime_error("用户数据保存失败");
        }
#ifdef _WIN32
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(temporary.c_str(), path.c_str(), flags))
            filesystemError = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
        if (replace) std::filesystem::rename(temporary, path, filesystemError);
        else std::filesystem::create_hard_link(temporary, path, filesystemError);
#endif
        if (filesystemError)
            throw std::filesystem::filesystem_error("无法保存用户数据文件", path, filesystemError);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        std::filesystem::remove(staging, ignored);
        throw;
    }
    std::filesystem::remove(temporary, filesystemError);
    std::filesystem::remove(staging, filesystemError);
}
}
void writeAtomicJson(const std::filesystem::path &path, const Json &j) { writeJsonFile(path, j, true); }
void createAtomicJson(const std::filesystem::path &path, const Json &j) { writeJsonFile(path, j, false); }
} // namespace wizard
