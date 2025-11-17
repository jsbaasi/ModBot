#include "command_listener.h"
#include <dpp/dpp.h>
#include <sqlite3.h>
#include "message.h"
#include "sql.h"
#include "progress.pb.h"
#include <string>
#include <utility>

void CommandListener::on_slashcommand(const dpp::slashcommand_t &event) {
    if (event.command.get_command_name() == "ping") {
        event.reply("Pong!");
    }
    else if (event.command.get_command_name() == "balance") {
        sqlite3* db{};
        char *zErrMsg = 0;
        int balance{-1};
        int rc = sqlite3_open("data/thediscord", &db);
        if( rc ) {
            std::cout << stderr << "Can't open database: " << sqlite3_errmsg(db) << '\n';
        }
        rc = sqlite3_exec(db, std::format(sql::selectBalanceByUser, event.command.get_issuing_user().id.str()).c_str(), sql::fillBalanceFromRecords, static_cast<void*>(&balance), &zErrMsg);
        if (balance==-1) {
            event.reply("You are not registered in the bot's database");
        } else {
            event.reply(std::format("Your aura balance is {}", balance));
        }
        sqlite3_close(db);
    }
    else if (event.command.get_command_name() == "register") {
        sqlite3* db{};
        char *zErrMsg = 0;
        int rc = sqlite3_open("data/thediscord", &db);
        std::set<dpp::snowflake> users {};
        std::string sql = "SELECT UserId FROM Users;";
        rc = sqlite3_exec(db, sql.c_str(), sql::fillUserIdSetFromRecords, static_cast<void*>(&users), &zErrMsg);
        if (users.contains(event.command.get_issuing_user().id)) {
            event.reply("You are already registered in the bot");
        } else {
            event.reply("putting you in the table twin (i'm not)");
        }
        sqlite3_close(db);
    }
    else if (event.command.get_command_name() == "inventory") {
        sqlite3* db {};
        int rc = sqlite3_open("data/thediscord", &db);
        char *errmsg{};
        std::string sql {std::format("SELECT Progress FROM Users WHERE UserId = {};", event.command.get_issuing_user().id.str())};
        bot::Progress progress{};
        sqlite3_stmt* myStmt{};
        sqlite3_prepare(db, sql.c_str(), -1, &myStmt, NULL);
        rc = sqlite3_step(myStmt);
        if (!(rc==SQLITE_DONE || rc==SQLITE_ROW)) {
            event.reply("Error occured");
            return;
        }
        progress.ParseFromArray(sqlite3_column_blob(myStmt, 0), sqlite3_column_bytes(myStmt, 0));
        if (progress.has_inventory() and progress.inventory().items_size()!=0) {
            std::string reply {"Your inventory: "};
            for (const bot::Item& item : progress.inventory().items()) {
                reply+=item.id();
            }
            event.reply(reply);
        } else {
            event.reply("Inventory is empty");
        }
        sqlite3_finalize(myStmt);
        sqlite3_close(db);
    }
    else if (event.command.get_command_name() == "birthday") {
        sqlite3* db {};
        int rc = sqlite3_open("data/thediscord", &db);
        char *errmsg{};
        std::vector<std::string> birthdays{};
        rc = sqlite3_exec(db, sql::selectBirthdays, sql::fillBirthdayArrayFromRecords, static_cast<void*>(&birthdays), &errmsg);
        // Get current timestamp as std::tm struct, respect to local time
        const std::time_t nowTT{ std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
        std::tm nowTM {*localtime(&nowTT)};
        // Fill array of std::tm structs with the birthday timestamp strings. Initialise the std::tm struct
        // with local time
        std::vector<std::tm> birthdayTimeArray{};
        for (std::string birthday: birthdays) {
            std::chrono::duration<int> d{ std::stoi(birthday) };
            std::time_t tt{ std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{d})};
            std::tm tm {*localtime(&tt)};
            birthdayTimeArray.push_back(tm);
        }
        // Sort the birthday std::tm structs
        std::sort(birthdayTimeArray.begin(), birthdayTimeArray.end(), [](std::tm const& lhs, std::tm const& rhs){
            return lhs.tm_yday < rhs.tm_yday;
        });
        // Binary search for which 2 birthdays std::tm are either side of right now std::tm
        int length{static_cast<int>(birthdayTimeArray.size())};
        int l{0}, r{static_cast<int>(length-1)}, m{};
        int leftDate{}, rightDate{};
        bool startYear{false}, endYear{false};
        // left conditional, left date should be last year, right conditional right date should be next year
        if (nowTM.tm_yday<=birthdayTimeArray[0].tm_yday) {
            leftDate = length-1;
            rightDate = 0;
            startYear = true;
        } else if (nowTM.tm_yday>birthdayTimeArray[length-1].tm_yday) {
            leftDate = length-1;
            rightDate = 0;
            endYear = true;
        } else {
            while (l<=r) {
                int m{(l+r)/2};
                if (nowTM.tm_yday>birthdayTimeArray[m].tm_yday) {
                    leftDate = m;
                    l = m+1;
                } else {
                    r = m-1;
                }
            }
            rightDate = leftDate+1;
        }
        // Turn the left and right std:tm to timestamps, then we search up in the database
        std::time_t leftStamp{mktime(&birthdayTimeArray[leftDate])}, rightStamp{mktime(&birthdayTimeArray[rightDate])};
        // Search up for the users in the database
        std::string leftUserId{}, rightUserId{};
        sqlite3_exec(db, std::format(sql::selectUserByBirthday, leftStamp).c_str(), sql::fillUserIdFromBirthday, static_cast<void*>(&leftUserId), &errmsg);
        sqlite3_exec(db, std::format(sql::selectUserByBirthday, rightStamp).c_str(), sql::fillUserIdFromBirthday, static_cast<void*>(&rightUserId), &errmsg);
        // Then we set the appropriate years for the timestamps
        if (startYear) {
            birthdayTimeArray[leftDate].tm_year = nowTM.tm_year-1;
            birthdayTimeArray[rightDate].tm_year = nowTM.tm_year;
        } else if (endYear) {
            birthdayTimeArray[leftDate].tm_year = nowTM.tm_year;
            birthdayTimeArray[rightDate].tm_year = nowTM.tm_year+1;
        } else {
            birthdayTimeArray[leftDate].tm_year = nowTM.tm_year;
            birthdayTimeArray[rightDate].tm_year = nowTM.tm_year;
        }
        leftStamp = mktime(&birthdayTimeArray[leftDate]);
        rightStamp = mktime(&birthdayTimeArray[rightDate]);
        event.reply(std::format("Previous was <@{}> birthday <t:{}:R>. Next is <@{}> birthday <t:{}:R>", leftUserId, leftStamp, rightUserId, rightStamp));
        sqlite3_close(db);
    }
    else if (event.command.get_command_name() == "leaderboard") {
        sqlite3* db {};
        sqlite3_open("data/thediscord", &db);
        char *errmsg{};

        std::vector<std::pair<std::string, int>> userIdToBalance{};
        sqlite3_exec(db, sql::selectUserIdOrderByBalance, sql::fillUserIdBalanceHashMapFromRecords, static_cast<void*>(&userIdToBalance), &errmsg);
        std::string resString {};
        int count{1};
        for (const auto& [userId, balance]: userIdToBalance) {
            switch (count) {
                case 1:
                    resString += ":first_place: ";
                    break;
                case 2:
                    resString += ":second_place: ";
                    break;
                case 3:
                    resString += ":third_place: ";
                    break;
                default:
                    resString += std::to_string(count) + ". ";
                    break; 
            }
            resString += std::format("<@{}>", userId) + " - **" + std::to_string(balance) + "** aura\n";
            count+=1;
        }
        dpp::embed resEmbed{};
        dpp::message resMessage{event.command.channel_id, ""};
        
        resMessage.add_file("jjbLogo.png", dpp::utility::read_file("images/jjb/jjbLogo.png"));
        resMessage.add_file("trophy.png", dpp::utility::read_file("images/jjb/trophy.png"));
        resEmbed.set_author("JJB", "https://stormblessed.fr/", "attachment://jjbLogo.png")
                .set_title("**LEADERBOARD**")
                .set_thumbnail("attachment://trophy.png")
                .set_color(dpp::colors::bright_gold)
                .add_field(
                "**BALANCE**",
                resString
                );
        resMessage.add_embed(resEmbed);
        event.reply(resMessage);
        sqlite3_close(db);
    }
    else if (event.command.get_command_name() == "transactions") {
        dpp::command_interaction cmd_data = event.command.get_command_interaction();
        sqlite3* db{};
        char *zErrMsg {0};
        sqlite3_open("data/thediscord", &db);

        bool exists {false};
        std::string userId {cmd_data.options.empty() ? event.command.get_issuing_user().id.str() : cmd_data.get_value<dpp::snowflake>(0).str()};
        sqlite3_exec(db, std::format(sql::selectUser, userId).c_str(), sql::checkIfUserIdExists, static_cast<void*>(&exists), &zErrMsg);
        if (!exists) {
            event.reply(cmd_data.options.empty() ? "You are not registered in the bot's database" : "User specified is not registered in the bot's database");
            return;
        }
        
        
        sqlite3_close(db);
    }
}