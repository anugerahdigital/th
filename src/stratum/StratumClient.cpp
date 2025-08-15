#include "StratumClient.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>

using namespace tht;

static int dial_tcp(const std::string& host, const std::string& port, std::string& err) {
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res = nullptr;
    int ret = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (ret != 0) {
        err = std::string("getaddrinfo: ") + gai_strerror(ret);
        return -1;
    }
    int sock = -1;
    for (addrinfo* p = res; p; p = p->ai_next) {
        sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;
        if (::connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            freeaddrinfo(res);
            return sock;
        }
        ::close(sock);
        sock = -1;
    }
    freeaddrinfo(res);
    err = "connect failed (no address succeeded)";
    return -1;
}

bool tht::parse_stratum_url(const std::string& url, std::string& host, std::string& port) {
    std::string pfx = "stratum+tcp://";
    if (url.rfind(pfx, 0) != 0) return false;
    std::string rest = url.substr(pfx.size());
    auto colon = rest.rfind(':');
    if (colon == std::string::npos) { host = rest; port = "3333"; return true; }
    host = rest.substr(0, colon);
    port = rest.substr(colon+1);
    return !host.empty() && !port.empty();
}

StratumClient::~StratumClient() { close(); }

bool StratumClient::connect(const StratumConfig& cfg, std::string& err) {
    cfg_ = cfg;
    sock_ = dial_tcp(cfg.host, cfg.port, err);
    if (sock_ < 0) { connected_.store(false); return false; }

    int one = 1;
    ::setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    ::setsockopt(sock_, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

    stop_ = false;
    connected_.store(true);
    reader_ = std::thread(&StratumClient::reader_loop, this);
    return true;
}

void StratumClient::close() {
    stop_ = true;
    if (sock_ >= 0) {
        ::shutdown(sock_, SHUT_RDWR);
        ::close(sock_);
        sock_ = -1;
    }
    if (reader_.joinable()) reader_.join();
    connected_.store(false);
}

bool StratumClient::send_line(const std::string& s) {
    if (sock_ < 0) return false;
    const char* buf = s.c_str();
    size_t left = s.size();
    while (left > 0) {
        ssize_t n = ::send(sock_, buf, left, 0);
        if (n <= 0) return false;
        buf += n;
        left -= (size_t)n;
    }
    return true;
}

bool StratumClient::send_subscribe() {
    int id = next_id_++;
    char msg[192];
    std::snprintf(msg, sizeof(msg),
        "{\"id\":%d,\"method\":\"mining.subscribe\",\"params\":[\"thought-miner-linux/1.7.0\"]}\r\n", id);
    return send_line(msg);
}

bool StratumClient::send_extranonce_subscribe() {
    int id = next_id_++;
    char msg[96];
    std::snprintf(msg, sizeof(msg),
        "{\"id\":%d,\"method\":\"mining.extranonce.subscribe\",\"params\":[]}\r\n", id);
    return send_line(msg);
}

bool StratumClient::send_authorize(const std::string& user, const std::string& pass) {
    int id = next_id_++;
    std::string payload = "{\"id\":" + std::to_string(id) +
        ",\"method\":\"mining.authorize\",\"params\":[\"" + user + "\",\"" + pass + "\"]}\r\n";
    return send_line(payload);
}

bool StratumClient::send_submit(const std::string& user,
                                const std::string& job_id,
                                const std::string& extranonce2_hex,
                                const std::string& ntime_hex,
                                const std::string& nonce_hex) {
    int id = next_id_++;
    std::string json = "{\"id\":" + std::to_string(id) +
        ",\"method\":\"mining.submit\",\"params\":[\"" + user + "\",\"" + job_id + "\",\"" +
        extranonce2_hex + "\",\"" + ntime_hex + "\",\"" + nonce_hex + "\"]}\r\n";
    return send_line(json);
}

// === Новое: submit_share + setter job_id ===
bool StratumClient::submit_share(const std::string& extranonce2_hex,
                                 const std::string& ntime_hex,
                                 const std::string& nonce_hex)
{
    if (cfg_.user.empty() || current_job_id_.empty()) {
        std::printf("[STRATUM] submit_share skipped (user or job_id empty)\n");
        return false;
    }
    return send_submit(cfg_.user, current_job_id_, extranonce2_hex, ntime_hex, nonce_hex);
}

void StratumClient::set_current_job_id(const std::string& job_id) {
    current_job_id_ = job_id;
}

void StratumClient::reader_loop() {
    std::string acc;
    acc.reserve(4096);
    char buf[4096];

    while (!stop_) {
        ssize_t n = ::recv(sock_, buf, sizeof(buf), 0);
        if (n <= 0) {
            if (!stop_) {
                std::printf("[STRATUM] disconnected (recv=%zd, errno=%d)\n", n, errno);
            }
            break;
        }
        acc.append(buf, buf + n);

        size_t pos = 0;
        for (;;) {
            auto nl = acc.find('\n', pos);
            if (nl == std::string::npos) {
                if (pos > 0) acc.erase(0, pos);
                break;
            }
            std::string line = acc.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            pos = nl + 1;

            if (!line.empty() && on_message_) {
                on_message_(line);
            }
        }
    }
    connected_.store(false);
}
