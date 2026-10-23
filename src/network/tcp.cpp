#include "wizard/network.hpp"
#include <algorithm>
#include <cstring>
#include <future>
#include <string>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
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
void closeSocket(Socket s) { closesocket(s); }
int error() { return WSAGetLastError(); }
bool pending(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
bool refused(int e) { return e == WSAECONNREFUSED; }
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
    ~Startup() { WSACleanup(); }
};
int socketLength(std::size_t n) { return static_cast<int>(n); }
std::string resolverError(int e) { return gai_strerrorA(e); }
#else
using Socket = int;
constexpr Socket invalid = -1;
void closeSocket(Socket s) { ::close(s); }
int error() { return errno; }
bool pending(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINPROGRESS; }
bool refused(int e) { return e == ECONNREFUSED; }
void nonblock(Socket s) {
    if (fcntl(s, F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("无法设置非阻塞连接");
}
struct Startup {};
int socketLength(std::size_t n) { return static_cast<socklen_t>(n); }
std::string resolverError(int e) { return gai_strerror(e); }
#endif

struct ResolvedAddress {
    sockaddr_storage address{};
    std::size_t length{};
    int family{}, type{}, protocol{};
};
struct ResolveResult {
    std::vector<ResolvedAddress> addresses;
    std::string error;
};

ResolveResult resolve(const std::string &host, unsigned short port) {
    // RFC 6761 reserves .invalid for guaranteed negative answers. Handle it
    // locally: DNS interception/fake-IP proxies may otherwise synthesize an
    // address and even accept the TCP connection for a nonexistent hostname.
    auto domain = host;
    if (!domain.empty() && domain.back() == '.')
        domain.pop_back();
    std::transform(domain.begin(), domain.end(), domain.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c);
    });
    if (domain == "invalid" ||
        (domain.size() > 8 && domain.compare(domain.size() - 8, 8, ".invalid") == 0))
        return {{}, "域名解析失败：.invalid 是保留的无效域名"};
#ifdef _WIN32
    Startup startup;
#endif
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    auto service = std::to_string(port);
    addrinfo *list = nullptr;
    auto status = getaddrinfo(host.c_str(), service.c_str(), &hints, &list);
    if (status != 0)
        return {{}, "域名解析失败：" + resolverError(status)};
    ResolveResult result;
    for (auto *entry = list; entry; entry = entry->ai_next) {
        if (!entry->ai_addr || entry->ai_addrlen > sizeof(sockaddr_storage))
            continue;
        ResolvedAddress address;
        std::memcpy(&address.address, entry->ai_addr, entry->ai_addrlen);
        address.length = entry->ai_addrlen;
        address.family = entry->ai_family;
        address.type = entry->ai_socktype;
        address.protocol = entry->ai_protocol;
        result.addresses.push_back(address);
    }
    freeaddrinfo(list);
    if (result.addresses.empty())
        result.error = "域名解析成功，但没有可用的 IPv4/IPv6 地址";
    return result;
}
} // namespace

struct Tcp::Impl {
    Startup startup;
    Socket listener{invalid}, socket{invalid};
    bool connecting{}, resolving{};
    Decoder decoder;
    std::vector<unsigned char> output;
    std::size_t offset{};
    std::chrono::steady_clock::time_point since;
    std::future<ResolveResult> resolution;
    std::vector<ResolvedAddress> addresses;
    std::size_t nextAddress{};
    std::string connectError;
};

Tcp::Tcp() : impl_(std::make_unique<Impl>()) {}
Tcp::~Tcp() { close(); }
void Tcp::disconnect() {
    if (impl_->socket != invalid)
        closeSocket(impl_->socket);
    impl_->socket = invalid;
    impl_->connecting = false;
    impl_->resolving = false;
    impl_->resolution = {};
    impl_->addresses.clear();
    impl_->nextAddress = 0;
    impl_->connectError.clear();
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
bool Tcp::connected() const { return impl_->socket != invalid && !impl_->connecting; }
bool Tcp::connecting() const { return impl_->connecting || impl_->resolving; }
bool Tcp::listening() const { return impl_->listener != invalid; }
std::size_t Tcp::queued() const { return impl_->output.size() - impl_->offset; }

void Tcp::listen(unsigned short port) {
    close();
    if (!port)
        throw std::runtime_error("端口必须为1–65535");
    auto s = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (s != invalid) {
#ifdef _WIN32
        int exclusive = 1;
        setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive),
                   sizeof(exclusive));
#endif
        int onlyV6 = 0;
        if (!setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char *>(&onlyV6),
                        sizeof(onlyV6))) {
            sockaddr_in6 a{};
            a.sin6_family = AF_INET6;
            a.sin6_addr = in6addr_any;
            a.sin6_port = htons(port);
            if (!::bind(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)) && !::listen(s, 1)) {
                try {
                    nonblock(s);
                } catch (...) {
                    closeSocket(s);
                    throw;
                }
                impl_->listener = s;
                return;
            }
        }
        closeSocket(s);
    }

    s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == invalid)
        throw std::runtime_error("无法创建 IPv4/IPv6 监听套接字");
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
    try {
        nonblock(s);
    } catch (...) {
        close();
        throw;
    }
}

void Tcp::connect(const std::string &address, unsigned short port) {
    close();
    if (address.empty())
        throw std::runtime_error("请输入房主域名或 IP 地址");
    if (!port)
        throw std::runtime_error("端口必须为1–65535");
    auto host = address;
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
        host = host.substr(1, host.size() - 2);
    if (host.empty())
        throw std::runtime_error("请输入有效的域名或 IP 地址");
    impl_->since = std::chrono::steady_clock::now();
    impl_->resolving = true;
    std::promise<ResolveResult> promise;
    impl_->resolution = promise.get_future();
    std::thread([host = std::move(host), port, result = std::move(promise)]() mutable {
        try {
            result.set_value(resolve(host, port));
        } catch (const std::exception &e) {
            result.set_value({{}, std::string("域名解析失败：") + e.what()});
        } catch (...) {
            result.set_value({{}, "域名解析失败：未知解析错误"});
        }
    }).detach();
}

void Tcp::send(const Json &j) {
    if (!connected())
        throw std::runtime_error("连接尚未建立");
    auto b = frame(j);
    if (impl_->output.size() - impl_->offset + b.size() > maxSend)
        throw std::runtime_error("待发送数据超过4MiB：连接速度不足或消息积压，请稍后重试");
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

    if (i.resolving && i.resolution.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto result = i.resolution.get();
        i.resolving = false;
        if (!result.error.empty()) {
            disconnect();
            throw std::runtime_error(result.error + "，请检查域名后重试");
        }
        i.addresses = std::move(result.addresses);
        i.nextAddress = 0;
        i.connectError.clear();
        i.since = std::chrono::steady_clock::now();
    }
    if (i.resolving && std::chrono::steady_clock::now() - i.since > std::chrono::seconds(10)) {
        disconnect();
        throw std::runtime_error("域名解析超时，请检查网络后重试");
    }

    auto startNextAddress = [&]() -> bool {
        while (i.nextAddress < i.addresses.size()) {
            const auto &address = i.addresses[i.nextAddress++];
            i.since = std::chrono::steady_clock::now();
            auto s = ::socket(address.family, address.type, address.protocol);
            if (s == invalid) {
                i.connectError = "无法创建 IPv4/IPv6 连接";
                continue;
            }
            try {
                nonblock(s);
            } catch (...) {
                closeSocket(s);
                i.connectError = "无法设置非阻塞连接";
                continue;
            }
            int on = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&on), sizeof(on));
            i.socket = s;
            auto r = ::connect(s, reinterpret_cast<const sockaddr *>(&address.address),
                               socketLength(address.length));
            if (!r) {
                i.connecting = false;
                return true;
            }
            auto e = error();
            if (pending(e)) {
                i.connecting = true;
                return true;
            }
            i.connectError = refused(e) ? "服务器拒绝连接：请确认房主已创建房间并检查端口" :
                                           "无法连接服务器：请检查地址、端口与防火墙";
            closeSocket(i.socket);
            i.socket = invalid;
        }
        return false;
    };

    if (!i.resolving && i.socket == invalid && !i.addresses.empty() && !startNextAddress()) {
        auto message = i.connectError.empty() ? "没有可连接的服务器地址" : i.connectError;
        disconnect();
        throw std::runtime_error(message);
    }
    if (i.connecting) {
        fd_set writable, exceptional;
        FD_ZERO(&writable);
        FD_ZERO(&exceptional);
        FD_SET(i.socket, &writable);
        FD_SET(i.socket, &exceptional);
        timeval timeout{};
        auto r = select(static_cast<int>(i.socket + 1), nullptr, &writable, &exceptional, &timeout);
        if (r < 0) {
            disconnect();
            throw std::runtime_error("连接检查失败");
        }
        if (r > 0) {
            int err = 0;
#ifdef _WIN32
            int len = sizeof(err);
#else
            socklen_t len = sizeof(err);
#endif
            if (getsockopt(i.socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len))
                err = error();
            if (err) {
                i.connectError = refused(err) ? "服务器拒绝连接：请确认房主已创建房间并检查端口" :
                                                "无法连接服务器：请检查地址、端口与防火墙";
                closeSocket(i.socket);
                i.socket = invalid;
                i.connecting = false;
                if (!startNextAddress()) {
                    auto message = i.connectError;
                    disconnect();
                    throw std::runtime_error(message);
                }
            } else {
                i.connecting = false;
                i.addresses.clear();
                i.nextAddress = 0;
            }
        } else if (std::chrono::steady_clock::now() - i.since > std::chrono::seconds(10)) {
            closeSocket(i.socket);
            i.socket = invalid;
            i.connecting = false;
            if (startNextAddress())
                return {};
            disconnect();
            throw std::runtime_error("连接超时：域名地址均无法连接，请检查端口和防火墙");
        }
    }
    if (!connected())
        return {};

    std::vector<Json> result;
    unsigned char b[16384];
    for (int count = 0; count < 16; ++count) {
        auto n = ::recv(i.socket, reinterpret_cast<char *>(b), sizeof(b), 0);
        if (n == 0) {
            disconnect();
            if (!result.empty())
                break;
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
            throw std::runtime_error("单轮接收消息数量超限");
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
