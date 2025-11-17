#ifndef SQL_H
#define SQL_H

namespace sql
{
    inline constexpr char transactionInsert[] {"INSERT INTO Transactions (TransactionId,UserId,BalanceDelta,Time,TransactionDescription,TransactionType) VALUES ({}, {}, {}, {}, {}, {});"};
    inline constexpr char selectUserIdOrderByBalance[] {"SELECT UserId, Balance FROM Users ORDER BY Balance DESC;"};
    inline constexpr char updateBalanceByUser[] {"UPDATE Users SET Balance = Balance + {} WHERE UserId = {};"};
    inline constexpr char selectLeagueIds[] {"SELECT UserId, LeagueId FROM Users WHERE LeagueId IS NOT NULL;"};
    inline constexpr char selectPubgIds[] {"SELECT UserId, PubgId FROM Users WHERE PubgId IS NOT NULL;"};
    inline constexpr char selectUserByBirthday[] {"SELECT UserId FROM Users WHERE Birthday = {};"};
    inline constexpr char selectBalanceByUser[] {"SELECT Balance FROM Users WHERE UserId={};"};
    inline constexpr char selectUser[] {"SELECT 1 FROM Users WHERE UserId={};"};
    inline constexpr char selectBirthdays[] {"SELECT Birthday FROM Users;"};

    int fillLeagueIdUserIdHashMapFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames);
    int fillUserIdBalanceHashMapFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames);
    int fillPubgIdUserIdHashMapFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames);
    int fillBirthdayArrayFromRecords(void* birthdays, int numberOfColumns, char **recordValues, char **columnNames);
    int fillUserIdArrayFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames);
    int fillBalanceFromRecords(void* balance, int numberOfColumns, char **recordValues, char **columnNames);
    int fillUserIdSetFromRecords(void* users, int numberOfColumns, char **recordValues, char **columnNames);
    int fillUserIdFromBirthday(void* userId, int numberOfColumns, char **recordValues, char **columnNames);
    int checkIfUserIdExists(void* exists, int numberOfColumns, char **recordValues, char **columnNames);
}

#endif