#ifndef PUBG_H
#define PUBG_H
#include <dpp/dpp.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace PUBG {
struct PostFunFact;
struct PubgPlayerSummary;
struct PubgPost;
using IdSet         = std::set<std::string>;
using IdToSnowflake = std::unordered_map<std::string, dpp::snowflake>;
using IdToSet       = std::unordered_map<std::string, std::set<std::string>>;
using IdToId        = std::unordered_map<std::string, std::string>;
using IdToInt       = std::unordered_map<std::string, int>;
using IdToPPSummary = std::unordered_map<std::string, PubgPlayerSummary>;
using PPostVec      = std::vector<PubgPost>;

enum FunFactType{
    ASSISTANT,
    DAMAGE,
    KILLER,
    LONGEST,
    REVIVER,
    RIDER,
    SWIMMER,
    WALKER,
    LOOTER,
};

struct PostFunFact{
    std::string user{};
    int type{};
    double data{};
};

struct PubgPlayerSummary{
    std::string name{};
    int kills{};
    int revives{};
    int assists{};
    double longestKill{};
    double damageDealt{};
};

struct PubgPost{
    double duration{};
    bool isWon{};
    std::unordered_map<std::string, PubgPlayerSummary>players{}; // PubgId as key not discord snowflake
    std::string mapName{};
    std::string gameMode{};
    int winPlace{};
    time_t matchStartTime{};
    PostFunFact funFact{};
};

dpp::task<void> PubgPollingMain(std::string& P_TOKEN, IdToId& pubglastKnownMatches, dpp::cluster& bot, const dpp::snowflake& JBBChannel, std::chrono::system_clock& myclock);
}

#endif