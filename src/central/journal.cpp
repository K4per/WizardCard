#include "wizard/central_journal.hpp"
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <system_error>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

namespace wizard::central {
namespace {
void validRoom(const std::string &room) {
    if (room.size() != 8 || room.find_first_not_of("ABCDEFGHJKLMNPQRSTUVWXYZ23456789") != std::string::npos)
        throw std::invalid_argument("invalid journal room code");
}
void durableAppend(const std::filesystem::path &path, std::uint64_t offset, const std::string &bytes) {
    if (!std::filesystem::exists(path)) {
        if (offset) throw std::runtime_error("journal missing acknowledged prefix");
        std::ofstream create(path, std::ios::binary);
        if (!create) throw std::runtime_error("cannot create journal");
    }
    if (std::filesystem::file_size(path) < offset) throw std::runtime_error("journal acknowledged prefix truncated");
    std::filesystem::resize_file(path, offset);
#ifdef _WIN32
    auto *file = _wfopen(path.c_str(), L"ab");
#else
    auto *file = std::fopen(path.c_str(), "ab");
#endif
    if (!file) throw std::system_error(errno, std::generic_category(), "journal open");
    const auto written = std::fwrite(bytes.data(), 1, bytes.size(), file);
    int status = std::fflush(file);
#ifdef _WIN32
    if (!status) status = _commit(_fileno(file));
#else
    if (!status) status = ::fsync(fileno(file));
#endif
    const auto closed = std::fclose(file);
    if (written != bytes.size() || status || closed) throw std::runtime_error("journal write or sync failed");
}
void durableJson(const std::filesystem::path &path, const Json &data) {
    writeAtomicJson(path, data);
#ifndef _WIN32
    const auto file = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (file < 0) throw std::system_error(errno, std::generic_category(), "metadata open");
    const auto status = ::fsync(file); ::close(file);
    if (status) throw std::system_error(errno, std::generic_category(), "metadata sync");
    const auto directory = ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) throw std::system_error(errno, std::generic_category(), "directory open");
    const auto directoryStatus = ::fsync(directory); ::close(directory);
    if (directoryStatus) throw std::system_error(errno, std::generic_category(), "directory sync");
#endif
}
}
JournalStore::JournalStore(std::filesystem::path directory, Content content)
    : directory_(std::move(directory)), content_(std::move(content)) {}
JournalResult JournalStore::write(const JournalWrite &write) const {
    JournalResult result{write.room, {}, write.serial, write.offset, {}};
    try {
        validRoom(write.room);
        if (!write.bytes) throw std::invalid_argument("missing journal payload");
        const auto storageId = write.summary.value("storageId", write.room);
        if (storageId.empty() || storageId.size() > 160 || storageId.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
            throw std::invalid_argument("invalid journal storage identity");
        const auto directory = directory_ / storageId;
        std::filesystem::create_directories(directory);
        durableAppend(directory / "journal.ndjson", write.offset, *write.bytes);
        if (write.finished && write.summary.at("phase") == "completed") {
            auto recording = write.finished->recording();
            auto bytes = recording.dump();
            if (bytes.size() > replayLimit) throw std::runtime_error("terminal replay exceeds 32 MiB");
            (void)wizard::replay(content_, recording);
            durableJson(directory / "replay.json", recording);
            result.replay = std::make_shared<const std::string>(std::move(bytes));
        }
        if (write.finished && write.finished->technicalPaused())
            durableJson(directory / "diagnostic.json", write.finished->diagnostic());
        auto summary = write.summary;
        summary["journalOffset"] = write.offset + write.bytes->size();
        durableJson(directory / "metadata.json", summary);
        result.offset = write.offset + write.bytes->size();
    } catch (const std::exception &e) { result.error = e.what(); result.replay.reset(); }
    return result;
}
std::size_t JournalStore::abortUnfinished() const {
    std::filesystem::create_directories(directory_);
    std::size_t recovered = 0;
    for (const auto &entry : std::filesystem::directory_iterator(directory_)) {
        if (!entry.is_directory()) continue;
        const auto name = entry.path().filename().string();
        if (name.size() < 8 || name.size() > 160 || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos) continue;
        const auto journal = entry.path() / "journal.ndjson";
        if (!std::filesystem::exists(journal)) continue;
        Json summary = {{"phase", "playing"}, {"revision", "0"}};
        const auto metadata = entry.path() / "metadata.json";
        if (std::filesystem::exists(metadata)) summary = readJson(metadata);
        if (summary.at("phase") == "completed" || summary.at("phase") == "aborted") continue;
        std::ifstream input(journal, std::ios::binary);
        std::uint64_t valid = 0;
        std::string line;
        while (std::getline(input, line)) {
            if (input.eof()) break; // A valid JSON value without its newline is an incomplete append.
            const auto row = Json::parse(line, nullptr, false);
            if (row.is_discarded() || !row.is_object()) break;
            valid += line.size() + 1;
            if (row.contains("revision")) summary["revision"] = row.at("revision");
            if (row.contains("summary")) summary = row.at("summary");
        }
        input.close();
        std::filesystem::resize_file(journal, valid);
        summary["phase"] = "aborted"; summary["abortReason"] = "server_restart"; summary["journalOffset"] = valid;
        durableJson(metadata, summary); ++recovered;
    }
    return recovered;
}
} // namespace wizard::central
