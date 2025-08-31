#include <dpp/dispatcher.h>
#include <dpp/intents.h>
#include <dpp/snowflake.h>
#include <fstream>
#include <iostream>
#include <dpp/dpp.h>
#include <dpp/presence.h>
#include <string>
#include <sqlite3.h>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <format>
#include "daily.h"
#include <stdlib.h>

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

int getUserIdFromRecord(void* users, int numberOfColumns, char **recordValues, char **columnNames){
    for(int i{0}; i<numberOfColumns; i++){
        if (strcmp(columnNames[i],"UserId")==0) {
            static_cast<std::vector<dpp::snowflake>*>(users)->push_back(static_cast<dpp::snowflake>(std::strtoull(recordValues[i], NULL, 10)));
            break;
        }
    }
    return 0;
}

int getBalanceFromRecord(void* balance, int numberOfColumns, char **recordValues, char **columnNames){
    *static_cast<int*>(balance) = atoi(recordValues[0]);
    return 0;
}

int main() {
    std::ifstream tokenStream{ "src/mytoken.txt" };
    std::string BOT_TOKEN{};
    tokenStream >> BOT_TOKEN;
    tokenStream.close();
    dpp::cluster bot(BOT_TOKEN, dpp::intents::i_default_intents | dpp::intents::i_guild_presences);
    bot.on_log(dpp::utility::cout_logger());
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

    // This handles the commands that get passed in
    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ping") {
            event.reply("Pong!");
        }
        if (event.command.get_command_name() == "balance") {
            sqlite3* db{};
            char *zErrMsg = 0;
            int balance{-1};
            int rc = sqlite3_open("data/thediscord", &db);
            if( rc ) {
                std::cout << stderr << "Can't open database: " << sqlite3_errmsg(db) << '\n';
            }
            std::string_view balanceSelect = "SELECT Balance FROM Users WHERE UserId={};";
            std::string sql = std::vformat(balanceSelect, std::make_format_args(static_cast<long long>(event.command.get_issuing_user().id)));
            rc = sqlite3_exec(db, sql.c_str(), getBalanceFromRecord, static_cast<void*>(&balance), &zErrMsg);
            if (balance==-1) {
                event.reply("You are not registered in the bot's database");
            } else {
                event.reply(std::format("Your aura balance is {}", balance));
            }
        }
    });

    // This runs when the bot starts
    bot.on_ready([&bot, &discordPresences, &presenceStatusToString, &myclock, &userDailies](const dpp::ready_t& event) {
        
        bot.start_timer([&bot, &discordPresences, &presenceStatusToString, &myclock, &userDailies](const dpp::timer& timer){
            // bot.request("https://dpp.dev/DPP-Logo.png", dpp::m_get, [&bot](const dpp::http_request_completion_t& callback) {
            //     bot.message_create(dpp::message(1407116920288710726, "").add_file("image.png", callback.body));
            // });
            // bot.message_create(dpp::message(1407116920288710726, "hello"));s

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
            std::string_view transactionInsert = "INSERT INTO Transactions (TransactionId,UserId,BalanceDelta,Time,TransactionDescription,TransactionType) VALUES ({}, {}, {}, {}, {}, {});";
            rc = sqlite3_exec(db, sql.c_str(), getUserIdFromRecord, static_cast<void*>(&users), &zErrMsg);
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
                    rc = sqlite3_exec(db, std::vformat(transactionInsert, std::make_format_args("NULL", static_cast<long long>(userId), static_cast<int>(daily::delta::d_wasOnline), timeNow, "\"Daily: User was online\"", 1)).c_str(), NULL, 0, &zErrMsg);
                    if( rc != SQLITE_OK ){
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                    }
                }
                // ------------------- Checking in-call

                // ------------------- Finally update what we collected from dailies
                if (userBalanceChange>0) {
                    std::string_view balanceUpdate = "UPDATE Users SET Balance = Balance + {} WHERE UserId = {};";
                    std::string sql = std::vformat(balanceUpdate, std::make_format_args(userBalanceChange, static_cast<long long>(userId)));
                    rc = sqlite3_exec(db, sql.c_str(), NULL, 0, &zErrMsg);
                }

                // if (((userDailies[userId] & daily::check::c_wasOnline) != daily::check::c_wasOnline) and
                // (discordPresences[userId].status()==dpp::presence_status::ps_online)) {
                //     userDailies[userId] |= daily::check::c_wasOnline;
                //     userBalanceChange += daily::delta::d_wasOnline;
                //     rc = sqlite3_exec(db, std::vformat(transactionInsert, std::make_format_args("NULL", static_cast<long long>(userId), 0, timeNow, "\"Daily: User was online\"", 1)).c_str(), NULL, 0, &zErrMsg);
                //     if( rc != SQLITE_OK ){
                //         fprintf(stderr, "SQL error: %s\n", zErrMsg);
                //         sqlite3_free(zErrMsg);
                //     }
                // }

                // ------------------- Update stored information
                // Some kind of insert sql statement to the transactions table
                std::string sql = std::vformat(transactionInsert,std::make_format_args("NULL", static_cast<long long>(userId), 0, timeNow, "\"User is Online\"", 0));
                rc = sqlite3_exec(db, sql.c_str(), NULL, 0, &zErrMsg);
                if( rc != SQLITE_OK ){
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                }

            }

            // ------------------- Close out sql stuff DONE
            sqlite3_close(db);
            // for (const auto& [k,v]: discordPresences) {
            //     std::cout << "user id " << k << " presence " << presenceStatusToString[v.status()] << '\n';
            // }
            return 0;
        }, 5);

        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
            bot.global_command_create(dpp::slashcommand("balance", "Check your aura balance", bot.me.id));
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