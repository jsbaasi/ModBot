#ifndef LEAGUE_H
#define LEAGUE_H
#include "snowflake.h"
#include <dpp/dpp.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace LoL {
    struct LeaguePlayerSummary;
    struct LeaguePost;
    using IdSet = std::set<std::string>;
    using IdToSnowflake = std::unordered_map<std::string, dpp::snowflake>;
    using IdToSet = std::unordered_map<std::string, std::set<std::string>>;
    using IdToId = std::unordered_map<std::string, std::string>;
    using IdToInt = std::unordered_map<std::string, int>;
    using LPostVec = std::vector<LeaguePost>;
    using IdToLPSummary = std::unordered_map<std::string, LeaguePlayerSummary>;

    struct LeaguePlayerSummary{
        std::string name{};
        std::string championName{};
        std::string role{};
        std::string teamPosition{};
        int kills{};
        int deaths{};
        int assists{};
        double kda{};
    };

    struct LeaguePost {
        bool isWon{};
        IdToLPSummary players{};
        double duration{};
        int mapId{};
        std::string gameMode{};
        time_t matchStartTime{};
        time_t matchEndTime{};
    };

    dpp::task<void> LeaguePollingMain(std::string& L_TOKEN, IdToId& leaguelastKnownMatches, dpp::cluster& bot, const dpp::snowflake& JBBChannel, std::chrono::system_clock& myclock);
}

#endif