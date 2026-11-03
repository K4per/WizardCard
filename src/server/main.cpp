#include "wizard/central_server.hpp"
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/rand.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

namespace {
using namespace std::chrono_literals;
using wizard::Json;
using Tcp = asio::ip::tcp;

void logEvent(const char *event) {
    std::cout << Json{{"event", event}}.dump() << std::endl;
}

std::string secureHex(std::size_t bytes) {
    std::vector<unsigned char> data(bytes);
    if (RAND_bytes(data.data(), static_cast<int>(data.size())) != 1)
        throw std::runtime_error("secure random generator unavailable");
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes * 2);
    for (auto value : data) {
        result.push_back(digits[value >> 4]);
        result.push_back(digits[value & 15]);
    }
    return result;
}

std::uint32_t secureSeed() {
    std::uint32_t value{};
    if (RAND_bytes(reinterpret_cast<unsigned char *>(&value), sizeof(value)) != 1)
        throw std::runtime_error("secure random generator unavailable");
    return value;
}

struct Options {
    std::filesystem::path assets{"assets/release-v260"};
    std::filesystem::path data{"var/wizard-server"};
    std::filesystem::path certificate, privateKey;
    std::string bind{"0.0.0.0"};
    unsigned short port{24443}, healthPort{24444};
};

unsigned short parsePort(const std::string &value) {
    unsigned port{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !port || port > 65535)
        throw std::invalid_argument("port must be between 1 and 65535");
    return static_cast<unsigned short>(port);
}

Options parseOptions(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help") {
            std::cout << "wizard_server --cert FILE --key FILE [--assets DIR] [--data DIR] "
                         "[--bind ADDRESS] [--port PORT] [--health-port PORT]\n";
            std::exit(0);
        }
        if (i + 1 >= argc) throw std::invalid_argument("missing value for " + key);
        const std::string value = argv[++i];
        if (key == "--assets") options.assets = value;
        else if (key == "--data") options.data = value;
        else if (key == "--cert") options.certificate = value;
        else if (key == "--key") options.privateKey = value;
        else if (key == "--bind") options.bind = value;
        else if (key == "--port") options.port = parsePort(value);
        else if (key == "--health-port") options.healthPort = parsePort(value);
        else throw std::invalid_argument("unknown option " + key);
    }
    if (options.certificate.empty() || options.privateKey.empty())
        throw std::invalid_argument("--cert and --key are required");
    return options;
}

class GamePool {
    struct Job { std::size_t bytes; std::function<void()> run; };
    std::mutex mutex_;
    std::condition_variable ready_, drained_;
    std::map<std::string, std::deque<Job>> queues_;
    std::deque<std::string> scheduled_;
    std::set<std::string> activeKeys_;
    std::vector<std::thread> workers_;
    std::size_t queuedJobs_{}, queuedBytes_{}, activeJobs_{};
    bool stopping_{};

    void work() {
        for (;;) {
            std::string key;
            Job job;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [&] { return stopping_ || !scheduled_.empty(); });
                if (stopping_ && scheduled_.empty()) return;
                key = std::move(scheduled_.front());
                scheduled_.pop_front();
                auto &queue = queues_.at(key);
                job = std::move(queue.front());
                queue.pop_front();
                --queuedJobs_;
                queuedBytes_ -= job.bytes;
                ++activeJobs_;
            }
            try { job.run(); } catch (...) { logEvent("game_job_error"); }
            {
                std::lock_guard lock(mutex_);
                --activeJobs_;
                auto found = queues_.find(key);
                if (found == queues_.end() || found->second.empty()) {
                    if (found != queues_.end()) queues_.erase(found);
                    activeKeys_.erase(key);
                } else {
                    scheduled_.push_back(key);
                }
                if (!queuedJobs_ && !activeJobs_) drained_.notify_all();
            }
        }
    }

  public:
    explicit GamePool(std::size_t workers) {
        for (std::size_t i = 0; i < workers; ++i) workers_.emplace_back([this] { work(); });
    }
    ~GamePool() { stop(); }
    bool post(std::string key, std::size_t bytes, std::function<void()> run) {
        std::lock_guard lock(mutex_);
        if (stopping_ || queuedJobs_ >= 8192 || bytes > 32 * 1024 * 1024 - queuedBytes_) return false;
        queues_[key].push_back({bytes, std::move(run)});
        ++queuedJobs_;
        queuedBytes_ += bytes;
        if (activeKeys_.insert(key).second) scheduled_.push_back(std::move(key));
        ready_.notify_one();
        return true;
    }
    bool drainFor(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        return drained_.wait_for(lock, timeout, [&] { return !queuedJobs_ && !activeJobs_; });
    }
    void stop() {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
            ready_.notify_all();
        }
        for (auto &worker : workers_) if (worker.joinable()) worker.join();
    }
};

class DiskPool {
    struct Job { std::size_t bytes; std::function<void()> run; };
    std::mutex mutex_;
    std::condition_variable ready_, drained_;
    std::deque<Job> queue_;
    std::thread worker_;
    std::size_t queuedBytes_{}, active_{};
    bool stopping_{};

    void work() {
        for (;;) {
            Job job;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty()) return;
                job = std::move(queue_.front());
                queue_.pop_front();
                queuedBytes_ -= job.bytes;
                ++active_;
            }
            try { job.run(); } catch (...) { logEvent("disk_job_error"); }
            {
                std::lock_guard lock(mutex_);
                --active_;
                if (queue_.empty() && !active_) drained_.notify_all();
            }
        }
    }

  public:
    DiskPool() : worker_([this] { work(); }) {}
    ~DiskPool() { stop(); }
    bool post(std::size_t bytes, std::function<void()> run) {
        std::lock_guard lock(mutex_);
        if (stopping_ || bytes > 32 * 1024 * 1024 - queuedBytes_) return false;
        queue_.push_back({bytes, std::move(run)});
        queuedBytes_ += bytes;
        ready_.notify_one();
        return true;
    }
    bool drainFor(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        return drained_.wait_for(lock, timeout, [&] { return queue_.empty() && !active_; });
    }
    void stop() {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
            ready_.notify_all();
        }
        if (worker_.joinable()) worker_.join();
    }
};

class Runtime;

class Client : public std::enable_shared_from_this<Client> {
    Runtime &runtime_;
    std::uint64_t id_;
    asio::ssl::stream<Tcp::socket> stream_;
    asio::steady_timer handshakeTimer_, heartbeatTimer_;
    wizard::net::Decoder decoder_;
    std::array<unsigned char, 16384> input_{};
    std::deque<std::shared_ptr<std::vector<unsigned char>>> output_;
    std::size_t outputBytes_{}, queuedMessages_{}, queuedBytes_{};
    std::chrono::steady_clock::time_point lastReceive_{std::chrono::steady_clock::now()}, rateAt_{lastReceive_};
    double rateTokens_{40};
    bool authenticated_{}, writing_{}, closed_{};

    void read();
    void write();
    void heartbeat();
    void close();
    void completeRequest(std::size_t bytes);
    friend class Runtime;

  public:
    Client(Runtime &, std::uint64_t, Tcp::socket &&, asio::ssl::context &);
    void start();
    void send(const Json &);
};

class Runtime {
    Options options_;
    wizard::Content content_;
    std::string instanceId_{secureHex(16)};
    wizard::central::Authority authority_;
    wizard::central::JournalStore journal_;
    asio::io_context io_{1};
    asio::ssl::context tls_{asio::ssl::context::tls_server};
    Tcp::acceptor acceptor_{io_}, healthAcceptor_{io_};
    asio::signal_set signals_{io_, SIGINT, SIGTERM};
    asio::steady_timer tickTimer_{io_}, stopTimer_{io_};
    GamePool games_{2};
    DiskPool disk_;
    std::map<std::uint64_t, std::shared_ptr<Client>> clients_;
    std::uint64_t nextConnection_{1};
    unsigned healthConnections_{};
    int dataLock_{-1};
    bool ready_{}, stopping_{};

    static std::string roomKey(wizard::central::Connection id) { return "connection-" + std::to_string(id); }

    void accept() {
        if (stopping_) return;
        acceptor_.async_accept([this](const asio::error_code &error, Tcp::socket socket) {
            if (!error && !stopping_) {
                if (clients_.size() >= 64) {
                    asio::error_code ignored;
                    socket.close(ignored);
                } else {
                    const auto id = nextConnection_++;
                    auto client = std::make_shared<Client>(*this, id, std::move(socket), tls_);
                    clients_.emplace(id, client);
                    client->start();
                }
            }
            if (!stopping_) accept();
        });
    }

    void healthAccept() {
        if (stopping_) return;
        auto socket = std::make_shared<Tcp::socket>(io_);
        healthAcceptor_.async_accept(*socket, [this, socket](const asio::error_code &error) {
            if (!error && !stopping_) {
                if (healthConnections_ >= 16) {
                    asio::error_code ignored;
                    socket->close(ignored);
                    healthAccept();
                    return;
                }
                ++healthConnections_;
                auto request = std::make_shared<asio::streambuf>(4096);
                auto timer = std::make_shared<asio::steady_timer>(io_, 2s);
                timer->async_wait([socket](const asio::error_code &timeout) {
                    if (!timeout) { asio::error_code ignored; socket->close(ignored); }
                });
                asio::async_read_until(*socket, *request, "\r\n\r\n",
                    [this, socket, request, timer](const asio::error_code &readError, std::size_t) {
                        if (readError) {
                            --healthConnections_;
                            timer->cancel();
                            asio::error_code ignored; socket->close(ignored);
                            return;
                        }
                        std::istream input(request.get());
                        std::string method, path, version;
                        input >> method >> path >> version;
                        const bool known = method == "GET" && (path == "/health" || path == "/ready");
                        const bool healthy = ready_ && !stopping_;
                        const int status = known && (path != "/ready" || healthy) ? 200 : (known ? 503 : 404);
                        Json body{{"status", healthy ? "ok" : "starting"}, {"ready", healthy},
                                  {"rooms", authority_.roomCount()}, {"connections", clients_.size()},
                                  {"resumeCredentials", authority_.credentialCount()}};
                        auto response = std::make_shared<std::string>(
                            "HTTP/1.1 " + std::to_string(status) + (status == 200 ? " OK\r\n" : " Not Ready\r\n") +
                            "Content-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                            std::to_string(body.dump().size()) + "\r\n\r\n" + body.dump());
                        asio::async_write(*socket, asio::buffer(*response), [this, socket, response, timer](const asio::error_code &, std::size_t) {
                            --healthConnections_;
                            timer->cancel();
                            asio::error_code ignored;
                            socket->close(ignored);
                        });
                    });
            }
            if (!stopping_) healthAccept();
        });
    }

    void scheduleTicks() {
        tickTimer_.expires_after(250ms);
        tickTimer_.async_wait([this](const asio::error_code &error) {
            if (error || stopping_) return;
            for (const auto &code : authority_.roomCodes()) {
                games_.post(code, 0, [this, code] {
                    deliver(authority_.tickRoom(code));
                    queueWrite(code);
                });
            }
            scheduleTicks();
        });
    }

    void queueWrite(const std::string &code) {
        auto write = authority_.takeWrite(code);
        if (!write) return;
        const auto serial = write->serial;
        const auto offset = write->offset;
        const auto bytes = write->bytes ? write->bytes->size() : 0;
        if (!disk_.post(bytes, [this, code, request = std::move(*write)]() mutable {
                auto result = journal_.write(request);
                const auto error = result.error;
                if (!games_.post(code, 0, [this, code, result = std::move(result)] {
                        deliver(authority_.persisted(result));
                        queueWrite(code);
                    }))
                    logEvent("persistence_completion_overloaded");
                if (!error.empty()) logEvent("persistence_retry_scheduled");
            })) {
            wizard::central::JournalResult failed{code, "persistence backlog full", serial, offset, {}};
            if (!games_.post(code, 0, [this, failed = std::move(failed), code] {
                    deliver(authority_.persisted(failed));
                }))
                logEvent("persistence_backlog_overloaded");
        }
    }

    void deliver(std::vector<wizard::central::Delivery> deliveries) {
        asio::post(io_, [this, deliveries = std::move(deliveries)] {
            for (const auto &delivery : deliveries) {
                auto found = clients_.find(delivery.connection);
                if (found == clients_.end()) continue;
                if (delivery.message.value("type", std::string{}) == "Welcome") {
                    found->second->authenticated_ = true;
                    found->second->handshakeTimer_.cancel();
                }
                found->second->send(delivery.message);
            }
        });
    }

    void received(const std::shared_ptr<Client> &client, Json message) {
        if (client->closed_) return;
        const auto type = message.value("type", std::string{});
        const auto now = std::chrono::steady_clock::now();
        client->rateTokens_ = std::min(40.0, client->rateTokens_ +
            std::chrono::duration<double>(now - client->rateAt_).count() * 20.0);
        client->rateAt_ = now;
        if (client->rateTokens_ < 1.0) { client->close(); return; }
        client->rateTokens_ -= 1.0;
        if (type == "Heartbeat") {
            if (!client->authenticated_) { client->close(); return; }
            client->send({{"type", "Heartbeat"}});
            return;
        }
        const auto bytes = message.dump().size();
        if (++client->queuedMessages_ > 64 || bytes > 2 * wizard::net::maxMessage - client->queuedBytes_) {
            client->close();
            return;
        }
        client->queuedBytes_ += bytes;
        const auto id = client->id_;
        auto key = authority_.route(id);
        if (key.empty()) key = roomKey(id);
        auto weak = std::weak_ptr<Client>(client);
        if (!games_.post(key, bytes, [this, id, weak, message = std::move(message), bytes, key] {
                auto result = authority_.receive(id, message);
                deliver(result);
                queueWrite(key);
                if (auto connection = weak.lock()) asio::post(io_, [connection, bytes] { connection->completeRequest(bytes); });
            })) {
            client->close();
        }
    }

    void disconnected(std::uint64_t id) {
        auto found = clients_.find(id);
        if (found == clients_.end()) return;
        clients_.erase(found);
        auto key = authority_.route(id);
        if (key.empty()) key = roomKey(id);
        games_.post(key, 0, [this, id, key] {
            deliver(authority_.disconnect(id));
            queueWrite(key);
        });
    }

    void beginStop() {
        if (stopping_) return;
        stopping_ = true;
        // Independent of the I/O loop and worker joins: a wedged filesystem
        // must not prevent termination. Restart scans the durable journal prefix.
        std::signal(SIGALRM, [](int) {
            constexpr char event[] = "{\"event\":\"shutdown_deadline\"}\n";
            (void)::write(STDERR_FILENO, event, sizeof(event) - 1);
            ::_exit(0);
        });
        ::alarm(8);
        ready_ = false;
        asio::error_code ignored;
        acceptor_.close(ignored);
        healthAcceptor_.close(ignored);
        tickTimer_.cancel();
        signals_.cancel();
        for (const auto &code : authority_.roomCodes()) {
            games_.post(code, 0, [this, code] {
                deliver(authority_.abortRoom(code, "server_shutdown"));
                queueWrite(code);
            });
        }
        stopTimer_.expires_after(5s);
        stopTimer_.async_wait([this](const asio::error_code &) {
            auto clients = std::move(clients_);
            clients_.clear();
            for (auto &[id, client] : clients) { (void)id; client->close(); }
            io_.stop();
        });
    }

    void acquireDataLock() {
        std::filesystem::create_directories(options_.data);
        std::filesystem::permissions(options_.data, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
        const auto path = options_.data / ".server.lock";
        dataLock_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (dataLock_ < 0 || ::flock(dataLock_, LOCK_EX | LOCK_NB) < 0)
            throw std::runtime_error("server data directory is already locked or unavailable");
    }

    friend class Client;

  public:
    explicit Runtime(Options options)
        : options_(std::move(options)), content_(wizard::loadContent(options_.assets)),
          authority_(content_, instanceId_, [] { return secureHex(32); }, [] { return secureSeed(); }),
          journal_(options_.data, content_) {
        if (!std::filesystem::is_regular_file(options_.certificate) || !std::filesystem::is_regular_file(options_.privateKey))
            throw std::invalid_argument("TLS certificate and key must be regular files");
        tls_.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
                         asio::ssl::context::no_sslv3 | asio::ssl::context::single_dh_use);
        if (SSL_CTX_set_min_proto_version(tls_.native_handle(), TLS1_2_VERSION) != 1)
            throw std::runtime_error("cannot require TLS 1.2 or later");
        tls_.use_certificate_chain_file(options_.certificate.string());
        tls_.use_private_key_file(options_.privateKey.string(), asio::ssl::context::pem);
        if (SSL_CTX_check_private_key(tls_.native_handle()) != 1)
            throw std::runtime_error("TLS private key does not match certificate");
        acquireDataLock();
        journal_.abortUnfinished();
    }

    ~Runtime() {
        games_.drainFor(5s);
        disk_.drainFor(5s);
        games_.drainFor(2s);
        games_.stop();
        disk_.stop();
        ::alarm(0);
        if (dataLock_ >= 0) { ::flock(dataLock_, LOCK_UN); ::close(dataLock_); }
    }

    void start() {
        const auto endpoint = Tcp::endpoint(asio::ip::make_address(options_.bind), options_.port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(Tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(64);
        healthAcceptor_.open(Tcp::v4());
        healthAcceptor_.set_option(Tcp::acceptor::reuse_address(true));
        healthAcceptor_.bind({asio::ip::address_v4::loopback(), options_.healthPort});
        healthAcceptor_.listen(16);
        ready_ = true;
        logEvent("server_ready");
        accept();
        healthAccept();
        scheduleTicks();
        signals_.async_wait([this](const asio::error_code &error, int) { if (!error) beginStop(); });
    }

    void run() { io_.run(); }
};

Client::Client(Runtime &runtime, std::uint64_t id, Tcp::socket &&socket, asio::ssl::context &context)
    : runtime_(runtime), id_(id), stream_(std::move(socket), context),
      handshakeTimer_(runtime.io_), heartbeatTimer_(runtime.io_) {}

void Client::start() {
    auto self = shared_from_this();
    handshakeTimer_.expires_after(10s);
    handshakeTimer_.async_wait([self](const asio::error_code &error) {
        if (!error && !self->authenticated_) self->close();
    });
    stream_.async_handshake(asio::ssl::stream_base::server, [self](const asio::error_code &error) {
        if (error) { self->close(); return; }
        self->read();
        self->heartbeat();
    });
}

void Client::read() {
    if (closed_) return;
    auto self = shared_from_this();
    stream_.async_read_some(asio::buffer(input_), [self](const asio::error_code &error, std::size_t count) {
        if (error) { self->close(); return; }
        try {
            self->lastReceive_ = std::chrono::steady_clock::now();
            for (auto &message : self->decoder_.feed(self->input_.data(), count))
                self->runtime_.received(self, std::move(message));
        } catch (...) {
            self->close();
            return;
        }
        self->read();
    });
}

void Client::send(const Json &message) {
    if (closed_) return;
    try {
        auto frame = std::make_shared<std::vector<unsigned char>>(wizard::net::frame(message));
        if (frame->size() > wizard::net::maxSend - outputBytes_) { close(); return; }
        outputBytes_ += frame->size();
        output_.push_back(std::move(frame));
        if (!writing_) write();
    } catch (...) { close(); }
}

void Client::write() {
    if (closed_ || output_.empty()) { writing_ = false; return; }
    writing_ = true;
    auto self = shared_from_this();
    auto frame = output_.front();
    asio::async_write(stream_, asio::buffer(*frame), [self, frame](const asio::error_code &error, std::size_t) {
        if (error) { self->close(); return; }
        self->outputBytes_ -= frame->size();
        self->output_.pop_front();
        self->write();
    });
}

void Client::heartbeat() {
    if (closed_) return;
    auto self = shared_from_this();
    heartbeatTimer_.expires_after(2s);
    heartbeatTimer_.async_wait([self](const asio::error_code &error) {
        if (error || self->closed_) return;
        if (std::chrono::steady_clock::now() - self->lastReceive_ >= 6s) { self->close(); return; }
        if (self->authenticated_) self->send({{"type", "Heartbeat"}});
        self->heartbeat();
    });
}

void Client::completeRequest(std::size_t bytes) {
    if (queuedMessages_) --queuedMessages_;
    queuedBytes_ = bytes <= queuedBytes_ ? queuedBytes_ - bytes : 0;
}

void Client::close() {
    if (closed_) return;
    closed_ = true;
    handshakeTimer_.cancel();
    heartbeatTimer_.cancel();
    asio::error_code ignored;
    stream_.lowest_layer().cancel(ignored);
    stream_.lowest_layer().shutdown(Tcp::socket::shutdown_both, ignored);
    stream_.lowest_layer().close(ignored);
    runtime_.disconnected(id_);
}
} // namespace

int main(int argc, char **argv) {
    try {
        Runtime runtime(parseOptions(argc, argv));
        runtime.start();
        runtime.run();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << Json{{"event", "fatal"}, {"message", error.what()}}.dump() << std::endl;
        return 1;
    }
}
