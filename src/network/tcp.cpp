#include "wizard/network.hpp"
#include <algorithm>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
namespace wizard::net {
namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid = INVALID_SOCKET;
void closeSocket(Socket s) {
    closesocket(s);
}
int error() {
    return WSAGetLastError();
}
bool pending(int e) {
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
}
void nonblock(Socket s) {
    u_long on = 1;
    if (ioctlsocket(s, FIONBIO, &on))
        throw std::runtime_error("无法设置非阻塞连接");
}
struct Startup {
    Startup() {
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d))
            throw std::runtime_error("无法初始化网络");
    }
    ~Startup() {
        WSACleanup();
    }
};
#else
using Socket = int;
constexpr Socket invalid = -1;
void closeSocket(Socket s) {
    ::close(s);
}
int error() {
    return errno;
}
bool pending(int e) {
    return e == EAGAIN || e == EWOULDBLOCK || e == EINPROGRESS;
}
void nonblock(Socket s) {
    if (fcntl(s, F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("无法设置非阻塞连接");
}
struct Startup {};
#endif
} // namespace
struct Tcp::Impl {
    Startup startup;
    Socket listener{invalid}, socket{invalid};
    bool connecting{};
    Decoder decoder;
    std::vector<unsigned char> output;
    std::size_t offset{};
    std::chrono::steady_clock::time_point since;
};
Tcp::Tcp() : impl_(std::make_unique<Impl>()) {}
Tcp::~Tcp() {
    close();
}
void Tcp::disconnect() {
    if (impl_->socket != invalid)
        closeSocket(impl_->socket);
    impl_->socket = invalid;
    impl_->connecting = false;
    impl_->decoder.clear();
    impl_->output.clear();
    impl_->offset = 0;
}
void Tcp::close() {
    disconnect();
    if (impl_->listener != invalid)
        closeSocket(impl_->listener);
    impl_->listener = invalid;
}
bool Tcp::connected() const {
    return impl_->socket != invalid && !impl_->connecting;
}
bool Tcp::listening() const {
    return impl_->listener != invalid;
}
std::size_t Tcp::queued() const {
    return impl_->output.size() - impl_->offset;
}
void Tcp::listen(unsigned short port) {
    close();
    if (!port)
        throw std::runtime_error("端口必须为1–65535");
    auto s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == invalid)
        throw std::runtime_error("无法创建监听套接字");
    impl_->listener = s;
#ifdef _WIN32
    int exclusive = 1;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive),
               sizeof(exclusive));
#endif
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (::bind(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)) || ::listen(s, 1)) {
        close();
        throw std::runtime_error("无法创建房间：端口已占用或不可用");
    }
    nonblock(s);
}
void Tcp::connect(const std::string &address, unsigned short port) {
    close();
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (!port || inet_pton(AF_INET, address.c_str(), &a.sin_addr) != 1)
        throw std::runtime_error("请输入有效IPv4地址和端口");
    auto s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == invalid)
        throw std::runtime_error("无法创建连接");
    impl_->socket = s;
    nonblock(s);
    int on = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&on), sizeof(on));
    auto r = ::connect(s, reinterpret_cast<sockaddr *>(&a), sizeof(a));
    if (r && !pending(error())) {
        disconnect();
        throw std::runtime_error("无法连接房主，请检查地址与防火墙");
    }
    impl_->connecting = r != 0;
    impl_->since = std::chrono::steady_clock::now();
}
void Tcp::send(const Json &j) {
    if (!connected())
        throw std::runtime_error("连接尚未建立");
    auto b = frame(j);
    if (impl_->output.size() - impl_->offset + b.size() > maxSend)
        throw std::runtime_error("发送队列超过4MiB");
    if (impl_->offset) {
        impl_->output.erase(impl_->output.begin(), impl_->output.begin() + impl_->offset);
        impl_->offset = 0;
    }
    impl_->output.insert(impl_->output.end(), b.begin(), b.end());
}
std::vector<Json> Tcp::poll() {
    auto &i = *impl_;
    if (i.listener != invalid && i.socket == invalid) {
        auto s = ::accept(i.listener, nullptr, nullptr);
        if (s != invalid) {
            i.socket = s;
            nonblock(s);
            int on = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&on), sizeof(on));
        }
    }
    if (i.connecting) {
        fd_set w, e;
        FD_ZERO(&w);
        FD_ZERO(&e);
        FD_SET(i.socket, &w);
        FD_SET(i.socket, &e);
        timeval timeout{};
        auto r = select(static_cast<int>(i.socket + 1), nullptr, &w, &e, &timeout);
        if (r < 0)
            throw std::runtime_error("连接检查失败");
        if (r > 0) {
            int err = 0;
#ifdef _WIN32
            int len = sizeof(err);
#else
            socklen_t len = sizeof(err);
#endif
            getsockopt(i.socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len);
            if (err) {
                disconnect();
                throw std::runtime_error("无法连接房主");
            }
            i.connecting = false;
        } else if (std::chrono::steady_clock::now() - i.since > std::chrono::seconds(6)) {
            disconnect();
            throw std::runtime_error("连接超时，请检查地址和防火墙");
        }
    }
    if (!connected())
        return {};
    // Bounded work per tick keeps rendering, heartbeats and input responsive.
    std::vector<Json> result;
    unsigned char b[16384];
    for (int count = 0; count < 16; ++count) {
        auto n = ::recv(i.socket, reinterpret_cast<char *>(b), sizeof(b), 0);
        if (n == 0) {
            disconnect();
            if (!result.empty())
                break; // deliver final frames before reporting EOF
            throw std::runtime_error("对方已断开连接");
        }
        if (n < 0) {
            if (pending(error()))
                break;
            disconnect();
            if (!result.empty())
                break;
            throw std::runtime_error("接收连接中断");
        }
        auto messages = i.decoder.feed(b, static_cast<std::size_t>(n));
        result.insert(result.end(), messages.begin(), messages.end());
        if (result.size() > 256)
            throw std::runtime_error("单帧消息数量超限");
    }
    for (int count = 0; count < 16 && i.offset < i.output.size(); ++count) {
        auto remaining = std::min<std::size_t>(16384, i.output.size() - i.offset);
#ifdef _WIN32
        constexpr int flags = 0;
#else
        constexpr int flags = MSG_NOSIGNAL;
#endif
        auto n = ::send(i.socket, reinterpret_cast<const char *>(i.output.data() + i.offset),
                        static_cast<int>(remaining), flags);
        if (n < 0) {
            if (pending(error()))
                break;
            disconnect();
            throw std::runtime_error("发送连接中断");
        }
        if (!n)
            break;
        i.offset += static_cast<std::size_t>(n);
    }
    if (i.offset == i.output.size()) {
        i.output.clear();
        i.offset = 0;
    }
    return result;
}
} // namespace wizard::net
