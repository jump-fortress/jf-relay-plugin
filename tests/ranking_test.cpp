#include "ranking.h"
#include <cstdlib>
#include <iostream>

void Check(bool result) { if (!result) { std::cerr << "Test failed\n"; std::exit(1); } }
int main() {
    Ranking ranking;
    Check(ranking.Update("JF1 jump_rush 123 456 789"));
    Check(ranking.Read().userids.size() == 3);
    Check(RelativeRank({456, 123, 789}, 123, 1, 1) == 3);
    Check(RelativeRank({456, 123, 789}, 123, 1, -1) == 1);
    Check(RelativeRank({123}, 0, 0, 1) == 1);
    Check(RelativeRank({123, 456}, 0, 0, -1) == 2);
    Check(RelativeRank({123}, 123, 1, 1) == 1);
    Check(!ranking.Update("JF1 jump_rush 123 123"));
    Check(!ranking.Update("JF1 jump_rush -1"));
    Check(!ranking.Update("JF1 jump_rush 123;quit"));
    Check(!ranking.Update("JF1 ../jump_rush 123"));
    Check(ranking.Read().userids.size() == 3);
    Check(ranking.Update("JF1 jump_rush"));
    Check(ranking.Read().userids.empty());
    Check(ranking.Update("CLEAR"));
    Check(ranking.Read().map.empty());
    std::cout << "Ranking tests passed\n";
}
