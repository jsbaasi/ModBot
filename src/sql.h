#ifndef SQL_H
#define SQL_H

namespace sql
{
    inline constexpr char getLeagueIds[] {"SELECT UserId, LeagueId FROM Users WHERE LeagueId IS NOT NULL;"};
    inline constexpr char updateBalance[] {"UPDATE Users SET Balance = Balance + {} WHERE UserId = {};"};
    inline constexpr char transactionInsert[] {"INSERT INTO Transactions (TransactionId,UserId,BalanceDelta,Time,TransactionDescription,TransactionType) VALUES ({}, {}, {}, {}, {}, {});"};
}
#endif