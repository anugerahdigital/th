#include "MinerApp.hpp"
#include "CpuMiner.hpp"
#include <cstdio>
#include <unistd.h>
#include <atomic>
#include <cctype>
#include <vector>
#include <thread>
#include <random>
#include <algorithm>
#include <openssl/sha.h>

using namespace tht;

// ===== utils (те же, что раньше) =====
void MinerApp::trim(std::string& s){ size_t a=0,b=s.size(); while(a<b&&std::isspace((unsigned char)s[a]))++a; while(b>a&&std::isspace((unsigned char)s[b-1]))--b; s=s.substr(a,b-a);}
std::string MinerApp::strip_quotes(const std::string& s){ if(s.size()>=2&&s.front()=='"'&&s.back()=='"') return s.substr(1,s.size()-2); return s;}

// subscribe result
bool MinerApp::parse_subscribe_result(const std::string& line, std::string& en1, int& en2sz){
    auto pos=line.find("\"result\":"); if(pos==std::string::npos) return false;
    auto am=line.find("]],",pos); if(am==std::string::npos) return false;
    auto q1=line.find('"',am+3); if(q1==std::string::npos) return false;
    auto q2=line.find('"',q1+1); if(q2==std::string::npos) return false;
    en1=line.substr(q1+1,q2-(q1+1));
    auto comma=line.find(',',q2+1); if(comma==std::string::npos) return false;
    size_t i=comma+1; while(i<line.size()&&std::isspace((unsigned char)line[i]))++i;
    size_t j=i; while(j<line.size()&&(std::isdigit((unsigned char)line[j])||line[j]=='+'))++j;
    if(j==i) return false;
    en2sz=std::atoi(line.substr(i,j-i).c_str());
    return true;
}

// notify params
bool MinerApp::extract_notify_params(const std::string& line, std::vector<std::string>& out){
    auto p=line.find("\"params\""); if(p==std::string::npos) return false;
    auto lb=line.find('[',p); if(lb==std::string::npos) return false;
    out.clear(); size_t i=lb+1; bool in=false,esc=false; int lvl=0; size_t st=i;
    auto push=[&](size_t a,size_t b){ std::string e=line.substr(a,b-a); trim(e); if(!e.empty()) out.push_back(e); };
    while(i<line.size()){
        char c=line[i];
        if(in){ if(esc) esc=false; else if(c=='\\') esc=true; else if(c=='"') in=false; ++i; continue; }
        if(c=='"'){ in=true; ++i; continue; }
        if(c=='['){ ++lvl; ++i; continue; }
        if(c==']'){
            if(lvl==0){ push(st,i); return true; } else { --lvl; ++i; continue; }
        }
        if(c==',' && lvl==0){ push(st,i); st=i+1; ++i; continue; }
        ++i;
    }
    return false;
}

void MinerApp::pretty_print_job(const Job& j) const{
    std::printf("[JOB] id=%s\n", j.job_id.c_str());
    std::printf("      prevhash   = %s\n", j.prevhash.c_str());
    std::printf("      coinbase1  = %s\n", j.coinbase1.c_str());
    std::printf("      coinbase2  = %s\n", j.coinbase2.c_str());
    std::printf("      merkle     = %s\n", j.merkle.c_str());
    std::printf("      version    = %s  nbits=%s  ntime=%s  clean=%s\n",
                j.version.c_str(), j.nbits.c_str(), j.ntime.c_str(), j.clean.c_str());
}

// ===== Доп. хелперы для GPU пути =====
static std::string to_hex(const uint8_t* d, size_t n){
    static const char* hexd="0123456789abcdef";
    std::string s; s.resize(n*2);
    for(size_t i=0;i<n;i++){ s[2*i]=hexd[(d[i]>>4)&0xF]; s[2*i+1]=hexd[d[i]&0xF]; }
    return s;
}
static std::vector<uint8_t> hex_to_bytes(const std::string& h){
    std::vector<uint8_t> out; out.reserve(h.size()/2);
    auto hexv=[](char c)->int{ if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return 10+c-'a'; if(c>='A'&&c<='F') return 10+c-'A'; return -1; };
    for(size_t i=0;i+1<h.size(); i+=2){
        int a=hexv(h[i]), b=hexv(h[i+1]);
        if(a<0||b<0) break;
        out.push_back((uint8_t)((a<<4)|b));
    }
    return out;
}
static std::array<uint8_t,32> sha256d_bytes(const std::vector<uint8_t>& data){
    uint8_t h1[32]; uint8_t h2[32];
    SHA256((const uint8_t*)data.data(), data.size(), h1);
    SHA256(h1, 32, h2);
    std::array<uint8_t,32> out{};
    std::copy(h2,h2+32,out.begin());
    return out;
}
static std::string gen_extranonce2_hex(int bytes){
    if(bytes<=0) bytes=4;
    std::random_device rd; std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(0,255);
    std::string s; s.resize(bytes*2);
    for(int i=0;i<bytes;i++){
        uint8_t v=(uint8_t)dist(rng);
        static const char* hexd="0123456789abcdef";
        s[2*i]=hexd[(v>>4)&0xF]; s[2*i+1]=hexd[v&0xF];
    }
    return s;
}
static std::vector<std::string> parse_merkle_array_hex(const std::string& s){
    std::vector<std::string> out;
    size_t i=0; bool in=false, esc=false; size_t st=0;
    while(i<s.size()){
        char c=s[i];
        if(!in){ if(c=='"'){ in=true; st=i+1; } ++i; continue; }
        if(in){
            if(esc){ esc=false; ++i; continue; }
            if(c=='\\'){ esc=true; ++i; continue; }
            if(c=='"'){ std::string h=s.substr(st, i-st); out.push_back(h); in=false; ++i; continue; }
            ++i;
        }
    }
    return out;
}
static std::string coinbase_txid_le_hex(const Job& j, const std::string& en1, const std::string& en2){
    std::string cb_hex = j.coinbase1 + en1 + en2 + j.coinbase2;
    auto cb = hex_to_bytes(cb_hex);
    auto d  = sha256d_bytes(cb);
    std::reverse(d.begin(), d.end()); // в LE-представление
    return to_hex(d.data(), d.size());
}

// ===== main =====
int MinerApp::run(const MinerConfig& cfg){
    if(cfg.selftest){
        solver_.set_device(cfg.device_id);
        solver_.set_worker_count(cfg.workers);
        solver_.self_test();
        return 0;
    }

    if(!cfg.url.empty()){
        StratumConfig sc; 
        if(!parse_stratum_url(cfg.url, sc.host, sc.port)){ std::fprintf(stderr,"Bad --url\n"); return 1; }
        sc.user=cfg.user; sc.pass=cfg.pass.empty()?"x":cfg.pass;

        std::atomic<bool> need_reconnect{false};
        extranonce1_.clear(); extranonce2_size_=-1; current_job_={};

        // CPU‑майнер создаём всегда, но запускать будем, ТОЛЬКО если cpu_threads>0
        CpuMiner cpu(stratum_, sc.user, cfg.cpu_threads > 0 ? cfg.cpu_threads : 1);
        cpuMiner_ = &cpu;

        // отдельный поток для GPU, чтобы не блокировать приём Stratum
        std::thread gpuThread;

        stratum_.set_on_message([&](const std::string& line){
            std::printf("[STRATUM] %s\n", line.c_str());

            // subscribe
            if(line.find("\"method\"")==std::string::npos && line.find("\"result\"")!=std::string::npos){
                std::string en1; int en2sz=-1;
                if(parse_subscribe_result(line,en1,en2sz)){
                    extranonce1_=en1; extranonce2_size_=en2sz;
                    std::printf("[PARSE] subscribe: extranonce1=%s extranonce2_size=%d\n",en1.c_str(),en2sz);
                }
            }

            // difficulty (share difficulty)
            if (line.find("\"method\":\"mining.set_difficulty\"") != std::string::npos) {
                auto lb = line.find('['), rb = line.find(']', lb + 1);
                if (lb != std::string::npos && rb != std::string::npos) {
                    std::string diff_s = line.substr(lb + 1, rb - lb - 1);
                    trim(diff_s);
                    current_diff_ = std::atof(diff_s.c_str());
                    if (current_diff_ <= 0) current_diff_ = 1.0;
                    std::printf("[PARSE] difficulty=%s -> current_diff_=%.8f\n", diff_s.c_str(), current_diff_);
                }
            }

            // notify -> старт нового джоба
            if(line.find("\"method\":\"mining.notify\"")!=std::string::npos){
                std::vector<std::string> P;
                if(extract_notify_params(line,P) && P.size()>=9){
                    Job j;
                    j.job_id   = strip_quotes(P[0]);
                    j.prevhash = strip_quotes(P[1]);
                    j.coinbase1= strip_quotes(P[2]);
                    j.coinbase2= strip_quotes(P[3]);
                    j.merkle   = P[4];
                    j.version  = strip_quotes(P[5]);
                    j.nbits    = strip_quotes(P[6]);
                    j.ntime    = strip_quotes(P[7]);
                    j.clean    = P[8];

                    current_job_=j;
                    pretty_print_job(current_job_);

                    // сообщим клиенту текущий job_id (для submit_share)
                    stratum_.set_current_job_id(current_job_.job_id);

                    // перезапуск майнинга под новый job
                    cpu.stop();

                    if (cfg.cpu_threads > 0) {
                        // CPU‑ветка
                        cpu.start(current_job_, extranonce1_, extranonce2_size_);
                    } else {
                        // GPU‑ветка
                        auto branch = parse_merkle_array_hex(current_job_.merkle);
                        std::string en2 = gen_extranonce2_hex(extranonce2_size_ > 0 ? extranonce2_size_ : 4);
                        std::string coinbase_txid_le = coinbase_txid_le_hex(current_job_, extranonce1_, en2);

                        if (gpuThread.joinable()) gpuThread.join(); // не плодим потоки
                        gpuThread = std::thread([this, job=current_job_, branch, en2, coinbase_txid_le](){
                            this->solver_.gpu_mine_once_and_submit(
                                job, coinbase_txid_le, branch, en2, job.ntime, this->stratum_, this->current_diff_);
                        });
                    }
                } else {
                    std::printf("[PARSE] notify: (failed to parse)\n");
                }
            }

            // busy=20
            if(line.find("\"error\"")!=std::string::npos && line.find("\"code\":20")!=std::string::npos){
                std::printf("[STRATUM] server busy -> reconnect soon\n");
                need_reconnect.store(true);
            }
        });

        auto do_connect = [&](int attempt)->bool{
            std::string err;
            if(!stratum_.connect(sc, err)){
                std::fprintf(stderr,"Stratum connect error: %s (attempt %d)\n", err.c_str(), attempt);
                return false;
            }
            std::printf("Connected to %s:%s\n", sc.host.c_str(), sc.port.c_str());
            if(!stratum_.send_subscribe()){ std::fprintf(stderr,"Failed subscribe\n"); return false; }
            std::printf("[STRATUM] >> mining.subscribe\n");
            if(!stratum_.send_extranonce_subscribe()){ std::fprintf(stderr,"Failed extranonce.subscribe\n"); return false; }
            std::printf("[STRATUM] >> mining.extranonce.subscribe\n");
            if(!sc.user.empty()){
                if(!stratum_.send_authorize(sc.user, sc.pass)){ std::fprintf(stderr,"Failed authorize\n"); return false; }
                std::printf("[STRATUM] >> mining.authorize user=%s\n", sc.user.c_str());
            }
            return true;
        };

        // разовый CUDA selftest
        solver_.set_device(cfg.device_id);
        solver_.set_worker_count(cfg.workers);
        solver_.self_test();

        int attempt=0;
        while(true){
            need_reconnect.store(false);
            if(!do_connect(++attempt)){
                int backoff = std::min(30, 1 << std::min(attempt, 5));
                std::printf("Reconnect in %d sec...\n", backoff);
                ::sleep(backoff);
                continue;
            }
            std::printf("Listening stratum... (Ctrl+C to exit)\n");
            attempt=0;

            while(true){
                if(need_reconnect.load()) break;
                if(!stratum_.is_connected()) break;
                ::sleep(1);
            }

            // стоп майнеров и перезапуск соединения
            cpu.stop();
            if (gpuThread.joinable()) gpuThread.join();
            stratum_.close();
            int backoff=2; std::printf("Will reconnect in %d sec...\n", backoff); ::sleep(backoff);
        }
    }

    // без URL — только self-test
    solver_.set_device(cfg.device_id);
    solver_.set_worker_count(cfg.workers);
    solver_.self_test();
    std::printf("No --url provided. Self-test done.\n");
    return 0;
}
