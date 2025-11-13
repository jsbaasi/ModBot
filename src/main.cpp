#include <algorithm>
#include <bits/chrono.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <ostream>
#include <set>
#include <string>
#include <sqlite3.h>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <format>
#include <stdlib.h>

#include "json.h"
#include <cpr/cpr.h>
#include <dpp/dpp.h>

#include "daily.h"
#include "random.h"
#include "constants.h"
#include "league.h"
#include "sql.h"
#include "command_listener.h"

/*
So the main functionality of the bot is the tracking stuff, need to make a 10sec/1min/5min loop that
runs continously, collecting data on the users.

https://www.learncpp.com/cpp-tutorial/introduction-to-lambdas-anonymous-functions/
I got to learn about lambda functions, unnamed functions that work better in cases
where you need a function pointer but function is so trivial that you don't want to
pollute the global namespace with


https://www.learncpp.com/cpp-tutorial/a1-static-and-dynamic-libraries/
I need to learn about libraries for sqlite

If I don't give it a txt file the bot just hangs, as if waiting for input but nothing happens when
I type something in

First we need a way to get all registered users, this will mean looking through the main table
that stores all users and their characteristics
Then we have either one table storing all balance history or we have table for each person storing
their balance history, naa one table
*/

time_t UTCStringToTimeT(const char* timestr, const char* fmtstr) {
	std::tm t = {}; // tm_isdst = 0, don't think about it please, this is UTC
	std::istringstream ss(timestr);
	ss.imbue(std::locale()); // "LANG=C", but local

	ss >> std::get_time(&t, fmtstr);
	if (ss.fail())
		return -1;
	// now fix up the day of week, day of year etc
	t.tm_isdst = 0; // no thinking!
	t.tm_wday = -1;
	if (mktime(&t) == -1 && t.tm_wday == -1) // "real error"
		return -1;

	return mktime(&t);
}

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
    FunFactType type{};
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

struct PostWinnerSquad {
    int squadSize{};
    int finalPlace{};
    int totalKills{};
    std::vector<std::string> players{};
};

struct PostSquadWiper {
    int squadSize{};
    int finalPlace{};
    int totalKills{};
    std::vector<std::string> players{};
};

struct PubgPost{
    // TODO I need to add time started member variable, carry on with visual studio experiments on mktime() and tm structs and all that bologna
    int duration{};
    bool isWon{};
    std::unordered_map<std::string, PubgPlayerSummary>players{}; // PubgId as key not discord snowflake
    std::string mapName{};
    std::string gameMode{};
    int winPlace{};
    time_t matchStartTime{};
    PostFunFact funFact{};
    PostWinnerSquad winnerSquad{};
    PostSquadWiper squadWiper{};
};

std::string getEmbedDescriptionString(const PubgPost& pubgPost) {
    std::string description {std::format("Match went on for {:.1f} minutes, started <t:{}:R>. Top ", pubgPost.duration/60.0f, pubgPost.matchStartTime)};
    std::string funFact{};
    switch (pubgPost.funFact.type) {
    case ASSISTANT:
        funFact = "assistant was {} with {:.0f} assists.";
        break;
    case DAMAGE:
        funFact = "damage dealer was {} with {:.1f} damage.";    
        break;
    case KILLER:
        funFact = "killer was {} with {:.0f} kills.";
        break;
    case LONGEST:
        funFact = "distance kill was by {} with {:.1f}m.";
        break;
    case REVIVER:
        funFact = "medic was {} with {:.0f} revives.";
        break;
    case RIDER:
        funFact = "driver was {} with {:.1f}m driven.";
        break;
    case SWIMMER:
        funFact = "swimmer was {} with {:.1f}m swam.";
        break;
    case WALKER:
        funFact = "car-less fucker was {} with {:.1f}m walked.";
        break;
    case LOOTER:
        funFact = "loot goblin was {} with {:.0f} weapons looted.";
        break;
    }
    return description+std::vformat(funFact, std::make_format_args(pubgPost.funFact.user, pubgPost.funFact.data));
}

// Returns a pubg message with embed
dpp::message getPubgPostMessage(dpp::snowflake channelId, const PubgPost& pubgPost, std::unordered_map<std::string, dpp::snowflake>& pubgIdToDiscordUser) {
    dpp::embed resEmbed{};
    dpp::message resMessage{channelId, ""};

    resMessage.add_file("pubg.png", dpp::utility::read_file("images/pubgicon.png"));
    resMessage.add_file("map.png", dpp::utility::read_file(std::format("images/maps/{}.png", pubgPost.mapName)));
    resMessage.add_file("jjbLogo.png", dpp::utility::read_file("images/jjb/jjbLogo.png"));

    if (pubgPost.isWon) {
        resEmbed.set_color(dpp::colors::bright_gold);
        resMessage.set_allowed_mentions(true);
    } else {
        resEmbed.set_color(dpp::colors::alien_gray);
    }
    std::string playerString {};
    std::string statsString {};
    for (const auto& [pubgId,pubgPlayerSummary]: pubgPost.players) {
        playerString+=std::format("<@{}>\n", pubgIdToDiscordUser[pubgId].str());
        statsString+=std::format("K:{}-R:{}-A:{}-LK:{:.1f}m-D:{:.1f}\n------\n", pubgPlayerSummary.kills, pubgPlayerSummary.revives, pubgPlayerSummary.assists, pubgPlayerSummary.longestKill, pubgPlayerSummary.damageDealt);
    }

    resEmbed.set_author("JJB", "https://stormblessed.fr/", "attachment://jjbLogo.png")
            .set_thumbnail("attachment://pubg.png")
            .set_image("attachment://map.png")
            .set_title(std::format("#{} Place", pubgPost.winPlace))
            .add_field(
            pubgPost.gameMode,
            getEmbedDescriptionString(pubgPost)
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
            .set_timestamp(pubgPost.matchStartTime);

    resMessage.add_embed(resEmbed);
    return resMessage;
}

int main(int argc, char* argv[]) {
    if (argc != 2 || (strcmp(argv[1], "dev")!=0 && strcmp(argv[1], "prod")!=0)) {
        std::cerr << "Usage:  " << argv[0] << " dev/prod\n";
        return -1;
    }
    sqlite3* db {};
    int rc{};
    rc = sqlite3_open_v2("data/thediscord", &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != 0) {
        std::cerr << "Bot SQLite database does not exist at data/thediscord";
        return -1;
    }
    sqlite3_close(db);

    const dpp::snowflake* JBBChannel{};
    std::ifstream dTokenStream{};

    if (strcmp(argv[1], "dev")==0) {
        JBBChannel = &constants::JBBChannelDev;
        dTokenStream.open("src/ddevtoken.txt");
    } else {
        JBBChannel = &constants::JBBChannelProd;
        dTokenStream.open("src/dprodtoken.txt");
    }

    if (dTokenStream.peek() == std::ifstream::traits_type::eof()) {
        std::cerr << "Discord Token not present at src/d" << argv[1] << "token.txt\n";
        return -1;
	}
    std::string D_TOKEN{};
    dTokenStream >> D_TOKEN;
    dTokenStream.close();
    std::ifstream pTokenStream{ "src/ptoken.txt" };
    if (pTokenStream.peek() == std::ifstream::traits_type::eof()) {
		std::cerr << "Pubg Token not present at src/ptoken.txt\n";
        return -1;
	}
    std::string P_TOKEN{};
    pTokenStream >> P_TOKEN;
    pTokenStream.close();


    std::ifstream lTokenStream{ "src/ltoken.txt" };
    if (lTokenStream.peek() == std::ifstream::traits_type::eof()) {
		std::cerr << "League Token not present at src/ltoken.txt\n";
        return -1;
	}
    std::string L_TOKEN{};
    lTokenStream >> L_TOKEN;
    lTokenStream.close();


    dpp::cluster bot(D_TOKEN, dpp::intents::i_default_intents | dpp::intents::i_guild_presences);
    bot.on_log([](const dpp::log_t& event) {
		if (event.severity > dpp::ll_trace) {
			std::cout << "{" << dpp::utility::current_date_time() << "} " << dpp::utility::loglevel(event.severity) << ": " << event.message << std::endl;
		}
	});
    std::unordered_map<dpp::presence_status, std::string> presenceStatusToString{
        {dpp::presence_status::ps_offline, std::string{"Offline"}},
        {dpp::presence_status::ps_online, std::string{"Online"}},
        {dpp::presence_status::ps_dnd, std::string{"Do not disturb"}},
        {dpp::presence_status::ps_idle, std::string{"Idle"}},
        {dpp::presence_status::ps_invisible, std::string{"Invisible"}},
    };
    std::unordered_map<int, std::string> funIndexToApiKey{
        {0, "assists"},
        {1, "damageDealt"},
        {2, "kills"},
        {3, "longestKill"},
        {4, "revives"},
        {5, "rideDistance"},
        {6, "swimDistance"},
        {7, "walkDistance"},
        {8, "weaponsAcquired"},
    };
    std::unordered_map<std::string, std::string> apiGamemodeToPost{
        {"squad-fpp", "Squad FPP"},
        {"duo-fpp", "Duo FPP"},
    };
    std::chrono::system_clock myclock{};
    std::unordered_map<dpp::snowflake, dpp::presence> discordPresences{};
    std::unordered_map<dpp::snowflake, int> userDailies{};
    std::unordered_map<std::string, std::string> pubglastKnownMatches{};
    std::unordered_map<std::string, std::string> leaguelastKnownMatches{};

    // This handles the commands that get passed in
    bot.on_slashcommand(&CommandListener::on_slashcommand);

    // This runs when the bot starts and reigsters our timers:
    // 1. User polling
    // 2. Pubg polling
    // 3. League polling
    // And also registers the bot commands
    bot.on_ready([&](const dpp::ready_t& event) {
        
        // ------------------- User polling timer
        bot.start_timer([&](const dpp::timer& timer){
            // bot.request("https://dpp.dev/DPP-Logo.png", dpp::m_get, [&bot](const dpp::http_request_completion_t& callback) {
            //     bot.message_create(dpp::message(1407116920288710726, "").add_file("image.png", callback.body));
            // });
            // dpp::message mymessage {1407116920288710726, "hello <@310692287191580674>"};
            // mymessage.set_allowed_mentions(true);
            // bot.message_create(mymessage);

            /*
            The timer loop will at a high level:
            Sqlite calls to get players
            Discord api calls to get information on players
            Sqlite calls to update stored information on players
            */

            // ------------------- Setup sqlite stuff DONE
            sqlite3* db{};
            char *zErrMsg = 0;
            int rc = sqlite3_open("data/thediscord", &db);
            if( rc ) {
                std::cout << stderr << "Can't open database: " << sqlite3_errmsg(db) << '\n';
                return 0;
            }
            // ------------------- Get users DONE
            std::vector<dpp::snowflake> users {};
            std::string sql = "SELECT * from Users";
            constexpr std::string_view transactionInsert = "INSERT INTO Transactions (TransactionId,UserId,BalanceDelta,Time,TransactionDescription,TransactionType) VALUES ({}, {}, {}, {}, {}, {});";
            rc = sqlite3_exec(db, sql.c_str(), sql::fillUserIdArrayFromRecords, static_cast<void*>(&users), &zErrMsg);
            if( rc != SQLITE_OK ) {
                std::cout << stderr << "SQL error: " << zErrMsg << '\n';
                sqlite3_free(zErrMsg);
            }
            
            // ------------------- For each user, check dailies and update stored information
            for (const dpp::snowflake& userId: users) {
                int userBalanceChange {0};
                long long timeNow = std::chrono::duration_cast<std::chrono::milliseconds>((myclock.now()).time_since_epoch()).count();
                if (!userDailies.contains(userId)) {
                    userDailies[userId] = daily::check::c_emptyCheck;
                }
                // ------------------- Check for dailies if they're not already set
                // ------------------- Checking online
                if (((userDailies[userId] & daily::check::c_wasOnline) != daily::check::c_wasOnline) and
                (discordPresences[userId].status()==dpp::presence_status::ps_online)) {
                    userDailies[userId] |= daily::check::c_wasOnline;
                    userBalanceChange += daily::delta::d_wasOnline;
                    rc = sqlite3_exec(db, std::format(transactionInsert, "NULL", static_cast<long long>(userId), static_cast<int>(daily::delta::d_wasOnline), timeNow, "\"Daily: User was online\"", 1).c_str(), NULL, 0, &zErrMsg);
                    if( rc != SQLITE_OK ){
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                    }
                }
                // ------------------- Checking in-call

                // ------------------- Finally update what we collected from dailies
                if (userBalanceChange>0) {
                    constexpr std::string_view balanceUpdate = "UPDATE Users SET Balance = Balance + {} WHERE UserId = {};";
                    std::string sql = std::format(balanceUpdate, userBalanceChange, userId.str());
                    rc = sqlite3_exec(db, sql.c_str(), NULL, 0, &zErrMsg);
                }

                // ------------------- Update stored information
                // Some kind of insert sql statement to the transactions table
                std::string sql = std::format(transactionInsert, "NULL", static_cast<long long>(userId), 0, timeNow, "\"User is Online\"", 0);
                rc = sqlite3_exec(db, sql.c_str(), NULL, 0, &zErrMsg);
                if( rc != SQLITE_OK ){
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                }

            }

            // ------------------- Close out sql stuff DONE
            sqlite3_close(db);
            return 0;
        }, 1200);

        // ------------------- Pubg polling timer
        // &bot, &lastKnownMatches, &P_TOKEN, &funIndexToApiKey, &JBBChannel
        bot.start_timer([&](const dpp::timer& timer) -> dpp::task<void>{
            // Populate pubgIdToDiscordUser with the PubgIds we need
            sqlite3* db{};
            char *zErrMsg{};
            int rc = sqlite3_open("data/thediscord", &db);
            std::unordered_map<std::string, dpp::snowflake> pubgIdToDiscordUser {};
            std::string sql {"SELECT UserId, PubgId FROM Users WHERE PubgId IS NOT NULL;"};
            // Below line is giving Uncaught exception in tick_timers: basic_string: construction from null is not valid
            rc = sqlite3_exec(db, sql.c_str(), sql::fillPubgIdUserIdHashMapFromRecords, static_cast<void*>(&pubgIdToDiscordUser), &zErrMsg);

            // For each pubgId:
            // 1. we request /players
            // 2. compare to the one held in "lastKnownMatch"
            // 3. populate "pubgIdToMatches" string:set()
            std::unordered_map<std::string, std::set<std::string>> pubgIdToMatches{};
            for (const auto& [pubgId, value]: pubgIdToDiscordUser) {
                cpr::Response playerResponse = cpr::Get(cpr::Url{"https://api.pubg.com/shards/steam/players?filter[playerIds]=" + pubgId},
                                                        cpr::Header{{"accept", "application/vnd.api+json"}},
                                                        cpr::Bearer{P_TOKEN});
                if (playerResponse.status_code!=200) {continue;}
                nlohmann::json playerJson {nlohmann::json::parse(playerResponse.text)};
                nlohmann::json& currMatches {playerJson["data"][0]["relationships"]["matches"]["data"]};
                
                if (pubglastKnownMatches.contains(pubgId)) {
                    std::string& lkMatch {pubglastKnownMatches[pubgId]};
                    if (currMatches[0]["id"]!=lkMatch) {
                        for (int i{0}; i<currMatches.size(); i++) {
                            if (currMatches[i]["id"]!=lkMatch) {
                                pubgIdToMatches[pubgId].insert(currMatches[i]["id"]);
                            } else {
                                break;
                            }
                        }
                    }
                    pubglastKnownMatches[pubgId] = currMatches[0]["id"];
                } else { // This Pubg Id hasn't been processed before we just populate the latest one
                    if (!currMatches.empty()){
                        pubglastKnownMatches[pubgId] = currMatches[0]["id"];
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            // For each pubgId in "pubgIdToMatches" we iterate through
            // the matches and build out a "matchIdToPubgId" string:set()
            std::unordered_map<std::string, std::set<std::string>> matchIdToPubgId {};
            for (const auto& [pubgId, matchIdSet]: pubgIdToMatches) {
                for (std::string matchId: matchIdSet) {
                    matchIdToPubgId[matchId].insert(pubgId);
                }
            }
            // For each match in "matchIdToPubgId"
            // 1. Call the /matches endpoint
            // 2. Populate a PubgPost struct
            // 3. Populate PubgPosts, append the post structs to the list
            // 4. Calculate how much aura we add
            std::vector<PubgPost> pubgPosts {};
            std::unordered_map<std::string, int> auraDelta{};
            for (const auto& [matchId, pubgIdSet]: matchIdToPubgId) {
                cpr::Response matchResponse = cpr::Get(cpr::Url{"https://api.pubg.com/shards/steam/matches/"+matchId},
                    cpr::Header{{"accept", "application/vnd.api+json"}});
                if (matchResponse.status_code!=200) {continue;}
                nlohmann::json matchJson {nlohmann::json::parse(matchResponse.text)};
                nlohmann::json& matchParticipants {matchJson["included"]};
                if (matchJson["data"]["attributes"]["gameMode"] != "squad-fpp" && matchJson["data"]["attributes"]["gameMode"] != "duo-fpp") {
                    continue;
                }
                PubgPost currPost {};
                currPost.matchStartTime = UTCStringToTimeT(static_cast<std::string>(matchJson["data"]["attributes"]["createdAt"]).c_str(), "%Y-%m-%dT%H:%M:%SZ");
                currPost.mapName = matchJson["data"]["attributes"]["mapName"];
                currPost.winPlace = 101;
                currPost.gameMode = apiGamemodeToPost[matchJson["data"]["attributes"]["gameMode"]];
                int randomFunFactIndex {Random::get(0, constants::LastFunTypeIndex)};
                currPost.funFact = {"", static_cast<FunFactType>(randomFunFactIndex), 0};
                std::string winnerSquad{};
                std::string squadWiper{};
                for (const auto& entity: matchParticipants) {
                    if (entity["type"] == "participant") {
                        const nlohmann::json& participantStats {entity["attributes"]["stats"]};
                        // Get our fun fact data
                        if (participantStats[funIndexToApiKey[randomFunFactIndex]]>currPost.funFact.data){
                            currPost.funFact.data=participantStats[funIndexToApiKey[randomFunFactIndex]];
                            if (pubgIdSet.contains(entity["attributes"]["stats"]["playerId"])){
                                currPost.funFact.user=std::format("<@{}>", pubgIdToDiscordUser[entity["attributes"]["stats"]["playerId"]].str());
                            } else {
                                currPost.funFact.user=participantStats["name"];
                            }
                        }
                        // Get our target player data
                        if (pubgIdSet.contains(participantStats["playerId"])){
                            currPost.duration=std::max(currPost.duration, static_cast<int>(participantStats["timeSurvived"]));
                            PubgPlayerSummary currSummary {
                                participantStats["name"],
                                participantStats["kills"],
                                participantStats["revives"],
                                participantStats["assists"],
                                participantStats["longestKill"],
                                participantStats["damageDealt"]
                            };
                            currPost.players[participantStats["playerId"]]=currSummary;
                            if (participantStats["winPlace"]==1){currPost.isWon=true;}
                            if (currPost.winPlace > participantStats["winPlace"]){currPost.winPlace=participantStats["winPlace"];}
                            auraDelta[participantStats["playerId"]] += (static_cast<int>(participantStats["revives"])*3) + static_cast<int>(participantStats["kills"]);
                        }
                    }
                }
                pubgPosts.push_back(currPost);
            }

            // Sort the posts list
            std::sort(pubgPosts.begin(), pubgPosts.end(),[](PubgPost const& lhs, PubgPost const& rhs){return lhs.matchStartTime < rhs.matchStartTime;});
            bot.log(dpp::loglevel::ll_info, "Finished PUBG polling function");
            // Create a message for each post in the list
            for (const PubgPost& pubgPost: pubgPosts) {
                co_await bot.co_message_create(getPubgPostMessage(*JBBChannel, pubgPost, pubgIdToDiscordUser));
            }
            long long timeNow = std::chrono::duration_cast<std::chrono::milliseconds>((myclock.now()).time_since_epoch()).count();
            // Update the aura for each pubgId in auraDelta
            for (const auto& [pubgId, auraDelta]: auraDelta) {
                std::string discordId {pubgIdToDiscordUser[pubgId].str()};
                sqlite3_exec(db, std::format("UPDATE Users SET Balance = Balance + {} WHERE UserId = {};", auraDelta, discordId).c_str(), NULL, NULL, &zErrMsg);
                constexpr std::string_view transactionInsert = "INSERT INTO Transactions (TransactionId,UserId,BalanceDelta,Time,TransactionDescription,TransactionType) VALUES ({}, {}, {}, {}, {}, {});";
                sqlite3_exec(db, std::format(transactionInsert, "NULL", discordId, auraDelta, timeNow, "\"PUBG Match\"", 2).c_str(), NULL, NULL, &zErrMsg);
            }
            sqlite3_close(db);
        }, 600);

        // ------------------- League polling timer
        bot.start_timer([&](const dpp::timer& timer) -> dpp::task<void> {
            co_await LoL::LeaguePollingMain(L_TOKEN, leaguelastKnownMatches, bot, *JBBChannel, myclock);
        }, 600);

        // ------------------- Registering commands
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("register", "Register for aura farming", bot.me.id));
            bot.global_command_create(dpp::slashcommand("balance", "Check your aura balance", bot.me.id));
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
            bot.global_command_create(dpp::slashcommand("inventory", "View your inventory", bot.me.id));
            bot.global_command_create(dpp::slashcommand("birthday", "Whose birthday is next", bot.me.id));
            bot.global_command_create(dpp::slashcommand("leaderboard", "Whose got the most aura", bot.me.id));
        }
    });

    // Keeping our presence map up to date
    bot.on_presence_update([&discordPresences](const dpp::presence_update_t& event){
        discordPresences[event.rich_presence.user_id] = event.rich_presence;
    });
    // Collecting our presence data when our bot starts up
    bot.on_guild_create([&discordPresences](const dpp::guild_create_t& event){
        for (const auto& [k,v]: event.presences) {
            discordPresences[k] = v;
        }
    });

    bot.start(dpp::st_wait);
    return 0;
}