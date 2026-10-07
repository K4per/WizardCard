#include "wizard/content.hpp"
#include <chrono>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace wizard {
void writeAtomicJson(const std::filesystem::path &path, const Json &j) {
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
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
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("无法替换用户数据文件");
#else
        std::filesystem::rename(temporary, path);
#endif
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
} // namespace wizard
