#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>

namespace tht {

struct StratumConfig {
    std::string host;
    std::string port = "3333";
    std::string user;   // wallet.worker
    std::string pass;   // usually "x"
};

class StratumClient {
public:
    using OnMessage = std::function<void(const std::string&)>;

    StratumClient() = default;
    ~StratumClient();

    bool connect(const StratumConfig& cfg, std::string& err);
    void close();

    bool send_subscribe();
    bool send_extranonce_subscribe();
    bool send_authorize(const std::string& user, const std::string& pass);

    // Базовый submit (есть у тебя)
    bool send_submit(const std::string& user,
                     const std::string& job_id,
                     const std::string& extranonce2_hex,
                     const std::string& ntime_hex,
                     const std::string& nonce_hex);

    // === УДОБНАЯ ОБЁРТКА ДЛЯ GPU ===
    // user берём из cfg_.user, job_id — из set_current_job_id(...)
    bool submit_share(const std::string& extranonce2_hex,
                      const std::string& ntime_hex,
                      const std::string& nonce_hex);

    // Сообщить текущий job_id для последующих submit'ов
    void set_current_job_id(const std::string& job_id);

    void set_on_message(OnMessage cb) { on_message_ = std::move(cb); }
    bool is_connected() const { return connected_.load(); }

private:
    bool send_line(const std::string& s);
    void reader_loop();

private:
    int sock_ = -1;
    std::thread reader_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> connected_{false};
    StratumConfig cfg_;
    OnMessage on_message_;
    int next_id_ = 1;

    // хранит актуальный job_id для submit_share()
    std::string current_job_id_;
};

bool parse_stratum_url(const std::string& url, std::string& host, std::string& port);

} // namespace tht
