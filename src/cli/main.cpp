#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include "miner/MinerApp.hpp"

using namespace tht;

static void print_usage(const char* argv0) {
    std::printf(
        "Usage: %s [--selftest] [-g <id>] [-w <n>] [-c <threads>] [--url <stratum>] [--user <u>] [--pass <p>]\n"
        "  --selftest          : run CUDA self-test and exit\n"
        "  -g <id>             : GPU index (default 0)\n"
        "  -w <n>              : workers per GPU (default 1)\n"
        "  -c <threads>        : CPU miner threads (default 0=disabled)\n"
        "  --url <stratum>     : stratum+tcp://host:port\n"
        "  --user <u>          : wallet.worker\n"
        "  --pass <p>          : password\n"
        , argv0
    );
}

int main(int argc, char** argv) {
    MinerConfig cfg;

    // значения по умолчанию
    cfg.device_id   = 0;
    cfg.workers     = 1;
    cfg.cpu_threads = 0;
    cfg.selftest    = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];

        if (a == "--selftest") {
            cfg.selftest = true;
        } else if (a == "-g" && i + 1 < argc) {
            cfg.device_id = std::atoi(argv[++i]);
        } else if (a == "-w" && i + 1 < argc) {
            cfg.workers = std::atoi(argv[++i]);
            if (cfg.workers < 1) cfg.workers = 1;
        } else if (a == "-c" && i + 1 < argc) {
            cfg.cpu_threads = std::atoi(argv[++i]);
            if (cfg.cpu_threads < 0) cfg.cpu_threads = 0;
        } else if (a == "--url" && i + 1 < argc) {
            cfg.url = argv[++i];
        } else if (a == "--user" && i + 1 < argc) {
            cfg.user = argv[++i];
        } else if (a == "--pass" && i + 1 < argc) {
            cfg.pass = argv[++i];
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "Unknown arg: %s\n", a.c_str());
            print_usage(argv[0]);
            return 1;
        }
    }

    // Базовая валидация: одновременно CPU и GPU — конфликт
    if (cfg.cpu_threads > 0 && (cfg.workers > 0 || !cfg.selftest)) {
        // Разрешаем сценарии:
        // 1) чисто CPU:      -c N   (без -g/-w)
        // 2) чисто GPU:      -g X -w Y (без -c)
        // 3) selftest CUDA:  --selftest
        // Если указан -c и -g/-w вместе — предупреждаем и отключаем GPU.
        if (cfg.workers != 1 || cfg.device_id != 0) {
            std::fprintf(stderr, "[WARN] -c и -g/-w заданы вместе: используем CPU (%d потоков), игнорируем GPU.\n",
                         cfg.cpu_threads);
        }
        // На стороне MinerApp логика уже корректно стартует CPU‑майнер при -c>0.
    }

    MinerApp app;
    return app.run(cfg);
}
