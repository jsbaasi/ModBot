#include <fstream>
#include <iostream>
#include <dpp/dpp.h>
#include <string>
#include <sqlite3.h>

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

int main() {
    std::ifstream tokenStream{ "src/mytoken.txt" };
    std::string BOT_TOKEN{};
    tokenStream >> BOT_TOKEN;
    tokenStream.close();
    dpp::cluster bot(BOT_TOKEN);
    bot.on_log(dpp::utility::cout_logger());

    // This handles the commands that get passed in
    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ping") {
            event.reply("Pong!");
        }
    });

    // This runs when the bot starts
    bot.on_ready([&bot](const dpp::ready_t& event) {
        
        bot.start_timer([&bot](const dpp::timer& timer){
            // bot.request("https://dpp.dev/DPP-Logo.png", dpp::m_get, [&bot](const dpp::http_request_completion_t& callback) {
            //     bot.message_create(dpp::message(1407116920288710726, "").add_file("image.png", callback.body));
            // });
            
            bot.message_create(dpp::message(1407116920288710726, "hello"));



        }, 5);

        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
        }

    });







    bot.start(dpp::st_wait);
}