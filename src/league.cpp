#include <dpp/dpp.h>
#include <format>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <cpr/cpr.h>
#include "league.h"
#include "nlohmann/json_fwd.hpp"
#include "snowflake.h"
#include "sql.h"
#include <algorithm>

namespace LoL {


    int fillLeagueIdUserIdHashMapFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames){
        // I'm casting the void pointer to map, then dereferencing it to use the operator[]
        static_cast<IdToSnowflake*>(users)->operator[](recordValues[1]) = recordValues[0];
        return 0;
    }

    void populateLeagueIdToMatches(IdToSet& leagueIdToMatches, std::string& L_TOKEN, IdToId& leaguelastKnownMatches, IdToSnowflake& leagueIdToDiscordUser) {
        std::cout << "Here in loop, for " << leagueIdToDiscordUser.size() << std::endl;
        for (const auto& [leagueId, discordId]: leagueIdToDiscordUser) {
            cpr::Response matchesResponse = cpr::Get(cpr::Url{std::format("https://europe.api.riotgames.com/lol/match/v5/matches/by-puuid/{}/ids", leagueId)},
                                                     cpr::Header{{"X-Riot-Token", L_TOKEN}});
            
            std::cout << "League Id: " << leagueId << " Matches response code is " << matchesResponse.status_code << std::endl;
            if (matchesResponse.status_code!=200) {continue;}
            nlohmann::json last20Matches {nlohmann::json::parse(matchesResponse.text)};

            if (leaguelastKnownMatches.contains(leagueId)) {
                std::string& lkMatch {leaguelastKnownMatches[leagueId]};
                if (last20Matches[0]==lkMatch) {break;}
                for (int i{0}; i<last20Matches.size(); i++) {
                    if (last20Matches[i]!=lkMatch) {
                        leagueIdToMatches[leagueId].insert(last20Matches[i]);
                    } else {
                        break;
                    }
                }
            } else { // This League Id hasn't been processed before we just populate the latest one
                if (last20Matches.empty()){continue;}
            }
            leaguelastKnownMatches[leagueId] = last20Matches[0];
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }

    void populateMatchIdToLeagueId(IdToSet& matchIdToLeagueId, IdToSet& leagueIdToMatches) {
        for (const auto& [leagueId, matchIdSet]: leagueIdToMatches) {
            for (std::string matchId: matchIdSet) {
                matchIdToLeagueId[matchId].insert(leagueId);
            }
        }
    }

    void populateLeaguePlayerSummary(LeaguePlayerSummary& currSummary, nlohmann::json& participantStats) {
        currSummary.name            = participantStats["riotIdGameName"].get<std::string>() + participantStats["riotIdTagline"].get<std::string>();
        currSummary.championName    = participantStats["championName"].get<std::string>();
        currSummary.role            = participantStats["role"].get<std::string>();
        currSummary.teamPosition    = participantStats["teamPosition"].get<std::string>();
        currSummary.kills           = participantStats["kills"].get<int>();
        currSummary.deaths          = participantStats["deaths"].get<int>();
        currSummary.assists         = participantStats["assists"].get<int>();
        currSummary.kda             = (currSummary.kills+currSummary.assists)/static_cast<double>(currSummary.deaths); 
    }

    void populateLeaguePost(IdToInt& auraDelta, nlohmann::json& matchJson, LeaguePost& currPost, const IdSet& leagueIdSet) {
        nlohmann::json& matchParticipants {matchJson["metadata"]["participants"]};
        for (int i{}; i<matchParticipants.size(); i++) {
            if (!leagueIdSet.contains(matchParticipants[i])){continue;}
            LeaguePlayerSummary currSummary{};
            populateLeaguePlayerSummary(currSummary, matchJson["info"]["participants"][i]);
            currPost.players[matchParticipants[i]] = currSummary;
            if (!currPost.isWon && matchJson["info"]["participants"][i]["win"]=="True"){currPost.isWon = true;}
            auraDelta[matchParticipants[i]] += currSummary.kills;
        }

        currPost.duration           = matchJson["info"]["gameDuration"].get<int>() / 60.0f;
        currPost.mapId              = matchJson["info"]["mapId"].get<int>();
        currPost.gameMode           = matchJson["info"]["gameMode"].get<std::string>();
        currPost.matchStartTime     = matchJson["info"]["gameStartTimestamp"].get<int64_t>() / 1000;
        currPost.matchEndTime       = matchJson["info"]["gameEndTimestamp"].get<int64_t>() / 1000;
    }

    void populateLeaguePostsAndAuraDelta(std::string& L_TOKEN, IdToSet& matchIdToLeagueId, LPostVec& leaguePosts, IdToInt auraDelta) {
        for (const auto& [matchId, leagueIdSet]: matchIdToLeagueId) {
            cpr::Response matchResponse = cpr::Get(cpr::Url{std::format("https://europe.api.riotgames.com/lol/match/v5/matches/{}", matchId)},
                                                   cpr::Header{{"X-Riot-Token", L_TOKEN}});
            if (matchResponse.status_code!=200) {continue;}
            nlohmann::json matchJson {nlohmann::json::parse(matchResponse.text)};
            LeaguePost currPost{};
            populateLeaguePost(auraDelta, matchJson, currPost, leagueIdSet);
            leaguePosts.push_back(currPost);
        }
    }

    dpp::message getLeaguePostMessage(const dpp::snowflake& channelId, const LeaguePost& leaguePost, IdToSnowflake& leagueIdToDiscordUser) {
        dpp::embed resEmbed{};
        dpp::message resMessage{channelId, ""};

        resMessage.add_file("league.png", dpp::utility::read_file("images/leagueicon.png"));
        resMessage.add_file("map.png", dpp::utility::read_file(std::format("images/maps/{}.png", leaguePost.mapId)));
        resMessage.add_file("jjbLogo.png", dpp::utility::read_file("images/jjb/jjbLogo.png"));
        
        std::string playerString {};
        std::string statsString {};
        for (const auto& [lId,lps]: leaguePost.players) {
            playerString+=std::format("<@{}>\n", leagueIdToDiscordUser[lId].str());
            statsString+=std::format("{} {} @ {}-K:{}-D:{}-A:{}//{:.1f}\n------\n",lps.role, lps.championName, lps.teamPosition, lps.kills, lps.deaths, lps.assists, lps.kda);
        }
        
        if (leaguePost.isWon) {
            resEmbed.set_color(dpp::colors::bright_gold)
                    .set_title("WIN");
            resMessage.set_allowed_mentions(true);
        } else {
            resEmbed.set_color(dpp::colors::alien_gray)
                    .set_title("LOSS");
        }

        resEmbed.set_author("JJB", "https://stormblessed.fr/", "attachment://jjbLogo.png")
                .set_thumbnail("attachment://league.png")
                .set_image("attachment://map.png")
                .add_field(
                leaguePost.gameMode,
                std::format("Match started at <t:{}:R> and ended at <t:{}:R>, lasting {:.1f} minutes.", leaguePost.matchStartTime, leaguePost.matchEndTime, leaguePost.duration)
                )
                .add_field(
                "Players",
                playerString,
                true
                )
                .add_field(
                    "Stats",
                    statsString,
                    true
                )
                .set_timestamp(leaguePost.matchStartTime);

        resMessage.add_embed(resEmbed);
        return resMessage;
    }

    dpp::task<void> LeaguePollingMain(std::string& L_TOKEN, IdToId& leaguelastKnownMatches, dpp::cluster& bot, const dpp::snowflake& JBBChannel, std::chrono::system_clock& myclock) {
        sqlite3* db{};
        char *zErrMsg{};
        sqlite3_open("data/thediscord", &db);
        
        // Populate leagueIdToDiscordUser with the LeagueIds we need
        IdToSnowflake leagueIdToDiscordUser{};
        sqlite3_exec(db, sql::getLeagueIds, fillLeagueIdUserIdHashMapFromRecords, static_cast<void*>(&leagueIdToDiscordUser), &zErrMsg);
        bot.log(dpp::loglevel::ll_info, std::format("leagueIdToDiscordUser {}", leagueIdToDiscordUser.size()));
        
        // For each leagueId:
        // 1. we request /matches/by-puuid
        // 2. compare to the one held in "lastKnownMatch"
        // 3. populate "leagueIdToMatches" string:set()
        IdToSet leagueIdToMatches{};
        populateLeagueIdToMatches(leagueIdToMatches, L_TOKEN, leaguelastKnownMatches, leagueIdToDiscordUser);
        bot.log(dpp::loglevel::ll_info, std::format("leagueIdToMatches {}", leagueIdToMatches.size()));
        for (const auto& [key, matches]: leagueIdToMatches) {
            std::cout << "key is " << key << " values is ";
            for (const auto& e: matches) {
                std::cout << e << " ";
            }
            std::cout << '\n';
        }
        
        // For each leagueId in "leagueIdToMatches" we iterate through
        // the matches and build out a "matchIdToLeagueId" string:set()
        IdToSet matchIdToLeagueId {};
        populateMatchIdToLeagueId(matchIdToLeagueId, leagueIdToMatches);
        bot.log(dpp::loglevel::ll_info, std::format("matchIdToLeagueId {}", matchIdToLeagueId.size()));
        for (const auto& [key, matches]: matchIdToLeagueId) {
            std::cout << "key is " << key << " values is ";
            for (const auto& e: matches) {
                std::cout << e << " ";
            }
            std::cout << '\n';
        }
        
        // For each match in "matchIdToLeagueId"
        // 1. Call the /matches endpoint
        // 2. Populate a LeaguePost struct
        // 3. Populate LeaguePosts, append the post structs to the list
        // 4. Calculate how much aura we add
        LPostVec leaguePosts{};
        IdToInt auraDelta{};
        populateLeaguePostsAndAuraDelta(L_TOKEN, matchIdToLeagueId, leaguePosts, auraDelta);
        bot.log(dpp::loglevel::ll_info, std::format("leaguePosts {}", leaguePosts.size()));

        // Sort the posts list
        std::sort(leaguePosts.begin(), leaguePosts.end(),[](LeaguePost const& lhs, LeaguePost const& rhs){return lhs.matchStartTime < rhs.matchStartTime;});

        // Create a message for each post in the list
        for (const LeaguePost& leaguePost: leaguePosts) {
            co_await bot.co_message_create(getLeaguePostMessage(JBBChannel, leaguePost, leagueIdToDiscordUser));
        }

        // Update the aura for each pubgId in auraDelta
        auto timeNow = std::chrono::duration_cast<std::chrono::milliseconds>((myclock.now()).time_since_epoch()).count();
        for (const auto& [lId, balanceChange]: auraDelta) {
            std::string discordId {leagueIdToDiscordUser[lId].str()};
            sqlite3_exec(db, std::format(sql::updateBalance, balanceChange, discordId).c_str(), NULL, NULL, &zErrMsg);
            sqlite3_exec(db, std::format(sql::transactionInsert, "NULL", discordId, balanceChange, timeNow, "\"League Match\"", 2).c_str(), NULL, NULL, &zErrMsg);
        }

        // Log completion of the league polling function
        bot.log(dpp::loglevel::ll_info, "Finished League polling function");
        sqlite3_close(db);
    }
}