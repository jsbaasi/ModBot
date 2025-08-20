#ifndef DB_H
#define DB_H
#include <string_view>
#include <sqlite3.h>
void writeToTable(std::string_view nameOfTable, int d);

#endif DB_H