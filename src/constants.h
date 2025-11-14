#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "snowflake.h"
namespace constants
{
    inline constexpr int LastFunTypeIndex { 8 };
    inline constexpr dpp::snowflake JBBChannelDev {1407116920288710726};
    inline constexpr dpp::snowflake JBBChannelProd {1424851501498630276};
    inline constexpr int LeaguePollingFrequency { 600 };
    inline constexpr int PUBGPollingFrequency { 600 };
    inline constexpr int PUBGRememberIndex { 0 };
}
#endif