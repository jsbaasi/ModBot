#include <cstdlib>
#include <dpp/dispatcher.h>
#include <dpp/intents.h>
#include <dpp/snowflake.h>
#include <fstream>
#include <iostream>
#include <dpp/dpp.h>
#include <dpp/presence.h>
#include <set>
#include <string>
#include <sqlite3.h>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <format>
#include "colors.h"
#include "daily.h"
#include "guild.h"
#include "nlohmann/json_fwd.hpp"
#include "user.h"
#include <stdlib.h>
#include <cpr/cpr.h>
#include "cpr/response.h"
#include "cpr/api.h"
#include "cpr/cprtypes.h"
#include "json.h"

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

int getUserIdArrayFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames){
    for(int i{0}; i<numberOfColumns; i++){
        if (strcmp(columnNames[i],"UserId")==0) {
            static_cast<std::vector<dpp::snowflake>*>(users)->push_back(static_cast<dpp::snowflake>(std::strtoull(recordValues[i], NULL, 10)));
            break;
        }
    }
    return 0;
}

int getPubgIdUserIdHashMapFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames){
    // I'm casting the void pointer to map, then dereferencing it to use the operator[]
    static_cast<std::unordered_map<std::string, dpp::snowflake>*>(users)->operator[](recordValues[1]) = recordValues[0];
    return 0;
}

int getUserIdSetFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames){
    static_cast<std::set<dpp::snowflake>*>(users)->insert(static_cast<dpp::snowflake>(std::strtoull(recordValues[0], NULL, 10)));
    return 0;
}

int getBalanceFromRecords(void* balance, int numberOfColumns, char **recordValues, char **columnNames){
    *static_cast<int*>(balance) = atoi(recordValues[0]);
    return 0;
}

struct PubgPlayerSummary{
    std::string name{};
    int kills{};
    int revives{};
    // int longestKill{};
};

struct PubgPost{
    // TODO I need to add time started member variable, carry on with visual studio experiments on mktime() and tm structs and all that bologna
    int duration{};
    bool isWon{};
    std::unordered_map<std::string, PubgPlayerSummary>players{}; // PubgId as key not discord snowflake
};

int main() {
    std::ifstream dTokenStream{ "src/dtoken.txt" };
    std::string D_TOKEN{};
    dTokenStream >> D_TOKEN;
    dTokenStream.close();
    std::ifstream pTokenStream{ "src/ptoken.txt" };
    std::string P_TOKEN{};
    pTokenStream >> P_TOKEN;
    pTokenStream.close();
    constexpr dpp::snowflake mossadChannel {1407116920288710726};
    dpp::cluster bot(D_TOKEN, dpp::intents::i_default_intents | dpp::intents::i_guild_presences);
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
    std::unordered_map<std::string, std::string> lastKnownMatches{};

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
            constexpr std::string_view balanceSelect = "SELECT Balance FROM Users WHERE UserId={};";
            std::string sql = std::format(balanceSelect, static_cast<long long>(event.command.get_issuing_user().id));
            rc = sqlite3_exec(db, sql.c_str(), getBalanceFromRecords, static_cast<void*>(&balance), &zErrMsg);
            if (balance==-1) {
                event.reply("You are not registered in the bot's database");
            } else {
                event.reply(std::format("Your aura balance is {}", balance));
            }
        }
        if (event.command.get_command_name() == "register") {
            sqlite3* db{};
            char *zErrMsg = 0;
            int rc = sqlite3_open("data/thediscord", &db);
            std::set<dpp::snowflake> users {};
            std::string sql = "SELECT UserId FROM Users;";
            rc = sqlite3_exec(db, sql.c_str(), getUserIdSetFromRecords, static_cast<void*>(&users), &zErrMsg);
            if (users.contains(event.command.get_issuing_user().id)) {
                event.reply("You are already registered in the bot");
            } else {
                event.reply("putting you in the table twin");
            }
        }
    });

    // This runs when the bot starts and reigsters our timers:
    // 1. User polling
    // 2. Pubg polling
    // And also registers the bot commands
    bot.on_ready([&bot, &discordPresences, &presenceStatusToString, &myclock, &userDailies, &lastKnownMatches, &P_TOKEN, &mossadChannel](const dpp::ready_t& event) {
        // how do i get images from local into here
        dpp::message msg(1407116920288710726, "");
        msg.add_file("pubg.png", dpp::utility::read_file("images/pubgicon.png"));
        msg.add_file("deston.png", dpp::utility::read_file("images/destonmap.png"));
        msg.add_file("jjbLogo.png", dpp::utility::read_file("images/jjb/jjbLogo.png"));
        dpp::embed lossEmbed = dpp::embed()
                .set_color(dpp::colors::alien_gray)
                .set_title("{}th Place")
                .set_author("JJB", "https://jsbaasi.github.io/", "attachment://jjbLogo.png")
                .set_thumbnail("attachment://pubg.png")
                .add_field(
                    "<t:1758203940>",
                    "Some value here"
                )
                .add_field(
                    "Players",
                    "jsbaasi\n{}",
                    true
                )
                .add_field(
                    "Kills - Revives - Assists",
                    "6 - 7 - 8\n{} - {} - {}\n",
                    true
                )
                .set_image("attachment://deston.png")
                .set_timestamp(time(0));
    
        msg.add_embed(lossEmbed);
        bot.message_create(msg);
        
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("register", "Register for aura farming", bot.me.id));
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
            bot.global_command_create(dpp::slashcommand("balance", "Check your aura balance", bot.me.id));
        }
    });

    bot.start(dpp::st_wait);
    return 0;
}