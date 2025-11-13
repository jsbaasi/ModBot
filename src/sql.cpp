#include <cstdlib>
#include <sqlite3.h>
#include <dpp/dpp.h>
#include <vector>
#include <string>
#include <set>
#include <unordered_map>

namespace sql {
    int fillBalanceFromRecords(void* balance, int numberOfColumns, char **recordValues, char **columnNames){
        *static_cast<int*>(balance) = atoi(recordValues[0]);
        return 0;
    }

    int fillUserIdArrayFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames){
        for(int i{0}; i<numberOfColumns; i++){
            if (strcmp(columnNames[i],"UserId")==0) {
                static_cast<std::vector<dpp::snowflake>*>(users)->push_back(static_cast<dpp::snowflake>(std::strtoull(recordValues[i], NULL, 10)));
                break;
            }
        }
        return 0;
    }

    int fillBirthdayArrayFromRecords(void* birthdays, int numberOfColumns, char **recordValues, char **columnNames) {
        static_cast<std::vector<std::string>*>(birthdays)->push_back(recordValues[0]);
        return 0;
    }

    int fillPubgIdUserIdHashMapFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames){
        // I'm casting the void pointer to map, then dereferencing it to use the operator[]
        static_cast<std::unordered_map<std::string, dpp::snowflake>*>(users)->operator[](recordValues[1]) = recordValues[0];
        return 0;
    }

    int fillUserIdSetFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames){
        static_cast<std::set<dpp::snowflake>*>(users)->insert(static_cast<dpp::snowflake>(std::strtoull(recordValues[0], NULL, 10)));
        return 0;
    }

    int fillUserIdFromBirthday(void* userId, int numberOfColumns, char **recordValues, char **columnNames){
        *static_cast<std::string*>(userId) = recordValues[0];
        return 0;
    }

    int fillLeagueIdUserIdHashMapFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames){
        // I'm casting the void pointer to map, then dereferencing it to use the operator[]
        static_cast<std::unordered_map<std::string, dpp::snowflake>*>(users)->operator[](recordValues[1]) = recordValues[0];
        return 0;
    }
}