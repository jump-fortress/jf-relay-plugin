#pragma once
#include "ranking.h"
#include <atomic>
#include <thread>

class Ipc {
public:
    explicit Ipc(Ranking& ranking) : ranking_(ranking) {}
    ~Ipc() { Stop(); }
    bool Start();
    void Stop();
private:
    void Run();
    Ranking& ranking_;
    std::atomic<bool> stopping_{true};
    std::atomic<int> startup_{0};
    std::thread worker_;
};
