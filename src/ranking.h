#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct Snapshot {
    std::string map;
    std::vector<int> userids;
    std::chrono::steady_clock::time_point received;
};

class Ranking {
public:
    bool Update(const std::string& line);
    void Clear();
    Snapshot Read() const;
private:
    mutable std::mutex mutex_;
    Snapshot snapshot_;
};

int RelativeRank(const std::vector<int>& ids, int lastUserid, int lastRank, int direction);
