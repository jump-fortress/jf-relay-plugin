#include "ranking.h"
#include <algorithm>
#include <charconv>
#include <set>
#include <sstream>

bool Ranking::Update(const std::string& line) {
    if (line == "CLEAR") {
        Clear();
        return true;
    }

    if (line.rfind("SELECT ", 0) == 0) {
        int userid = 0;
        const auto result = std::from_chars(line.data() + 7, line.data() + line.size(), userid);
        if (result.ec != std::errc{} || result.ptr != line.data() + line.size() || userid <= 0) return false;

        std::lock_guard<std::mutex> lock(mutex_);
        if (std::find(snapshot_.userids.begin(), snapshot_.userids.end(), userid) == snapshot_.userids.end()) return true;

        selection_ = userid;
        return true;
    }

    std::istringstream input(line);
    std::string version, map, token;
    if (!(input >> version >> map) || version != "JF1" || map.size() > 128
        || map.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) {
        return false;
    }

    std::vector<int> ids;
    std::set<int> unique;
    while (input >> token) {
        int id = 0;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), id);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size()
            || id <= 0 || !unique.insert(id).second || ids.size() >= 100) {
            return false;
        }
        ids.push_back(id);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.map != map) selection_ = 0;

    snapshot_ = {map, ids, std::chrono::steady_clock::now()};
    return true;
}

void Ranking::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = {};
    selection_ = 0;
}

int Ranking::TakeSelection() {
    std::lock_guard<std::mutex> lock(mutex_);

    const int userid = selection_;
    selection_ = 0;
    return userid;
}

Snapshot Ranking::Read() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

int RelativeRank(const std::vector<int>& ids, int lastUserid, int lastRank, int direction) {
    if (ids.empty()) return 0;

    const auto found = std::find(ids.begin(), ids.end(), lastUserid);
    if (found != ids.end()) lastRank = static_cast<int>(found - ids.begin()) + 1;
    if (lastRank <= 0) return direction > 0 ? 1 : static_cast<int>(ids.size());

    return std::clamp(lastRank + direction, 1, static_cast<int>(ids.size()));
}
