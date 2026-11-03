#pragma once
#include "wizard/central.hpp"

namespace wizard::central {
struct JournalWrite {
    std::string room;
    std::uint64_t serial{}, offset{};
    std::shared_ptr<const std::string> bytes;
    Json summary;
    // Terminal authority is immutable; serialization runs on the disk executor.
    std::shared_ptr<const MatchSession> finished;
};
struct JournalResult {
    std::string room, error;
    std::uint64_t serial{}, offset{};
    std::shared_ptr<const std::string> replay;
};
class JournalStore {
  public:
    JournalStore(std::filesystem::path, Content);
    JournalResult write(const JournalWrite &) const;
    std::size_t abortUnfinished() const;
  private:
    std::filesystem::path directory_;
    Content content_;
};
} // namespace wizard::central
