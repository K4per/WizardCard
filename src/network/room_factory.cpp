#include "wizard/network.hpp"

namespace wizard::net {
std::unique_ptr<RoomSession> createRoom(Content content, std::filesystem::path replayPath) {
    return std::make_unique<Peer>(std::move(content), std::move(replayPath));
}
} // namespace wizard::net
