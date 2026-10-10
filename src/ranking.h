#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct Snapshot {
    std::string map;
    std::vector<int> userids;
    std::vector<int> selectableUserids;
    std::chrono::steady_clock::time_point received;
};

class Ranking {
public:
    bool Update(const std::string& line);
    void Clear();
    Snapshot Read() const;
    int TakeSelection();
private:
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    int selection_ = 0;
};

int RelativeRank(const std::vector<int>& ids, int lastUserid, int lastRank, int direction);
