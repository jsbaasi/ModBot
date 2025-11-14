#include <bits/chrono.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <sqlite3.h>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <format>
#include <stdlib.h>

#include "json.h"
#include <cpr/cpr.h>
#include <dpp/dpp.h>
#include <cpptrace/from_current.hpp>

#include "daily.h"
#include "pubg/pubg.h"
#include "constants.h"
#include "league.h"
#include "sql.h"
#include "command_listener.h"

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
        bot.start_timer([&P_TOKEN, &pubglastKnownMatches, &bot, &JBBChannel, &myclock](const dpp::timer& timer) -> dpp::task<void>{
            co_await PUBG::PubgPollingMain(P_TOKEN, pubglastKnownMatches, bot, *JBBChannel, myclock);
        }, constants::PUBGPollingFrequency);

        // ------------------- League polling timer
        bot.start_timer([&L_TOKEN, &leaguelastKnownMatches, &bot, &JBBChannel, &myclock](const dpp::timer& timer) -> dpp::task<void> {
            co_await LoL::LeaguePollingMain(L_TOKEN, leaguelastKnownMatches, bot, *JBBChannel, myclock);
        }, constants::LeaguePollingFrequency);

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