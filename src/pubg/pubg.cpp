

#include <dpp/dpp.h>
#include <sqlite3.h>
#include <cpr/cpr.h>
#include <string_view>

#include "pubg.h"
#include "misc-enum.h"
#include "sql.h"
#include "random.h"
#include "constants.h"

namespace PUBG {

time_t UTCStringToTimeT(std::string timestr, const char* fmtstr) {
	std::tm t = {}; // tm_isdst = 0, don't think about it please, this is UTC
	std::istringstream ss(timestr.c_str());
	ss.imbue(std::locale()); // "LANG=C", but local

	ss >> std::get_time(&t, fmtstr);
	if (ss.fail())
		return -1;
	// now fix up the day of week, day of year etc
	t.tm_isdst = 0; // no thinking!
	t.tm_wday = -1;
	if (mktime(&t) == -1 && t.tm_wday == -1) // "real error"
		return -1;

	return mktime(&t);
}

void populatePubgIdToMatches(IdToSet& pubgIdToMatches, std::string& P_TOKEN, IdToId& pubglastKnownMatches, IdToSnowflake& pubgIdToDiscordUser) {
    for (const auto& [pubgId, discordId]: pubgIdToDiscordUser) {
        cpr::Response playerResponse = cpr::Get(cpr::Url{"https://api.pubg.com/shards/steam/players?filter[playerIds]=" + pubgId},
                                                cpr::Header{{"accept", "application/vnd.api+json"}},
                                                cpr::Bearer{P_TOKEN});
        
        if (playerResponse.status_code!=200) {continue;}
        nlohmann::json playerJson {nlohmann::json::parse(playerResponse.text)};
        nlohmann::json& currMatches {playerJson["data"][0]["relationships"]["matches"]["data"]};

        if (pubglastKnownMatches.contains(pubgId)) {
            std::string& lkMatch {pubglastKnownMatches[pubgId]};
            if (currMatches[0]["id"].get<std::string>()==lkMatch) {continue;}
            for (int i{0}; i<currMatches.size(); i++) {
                if (currMatches[i]["id"].get<std::string>()!=lkMatch) {
                    pubgIdToMatches[pubgId].insert(currMatches[i]["id"].get<std::string>());
                } else {
                    break;
                }
            }
        }
        pubglastKnownMatches[pubgId] = currMatches[constants::PUBGRememberIndex]["id"].get<std::string>();
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
}

void populateMatchIdToPubgId(IdToSet& matchIdToPubgId, IdToSet& pubgIdToMatches){
    for (const auto& [pubgId, matchIdSet]: pubgIdToMatches) {
        for (std::string matchId: matchIdSet) {
            matchIdToPubgId[matchId].insert(pubgId);
        }
    }
}

constexpr std::string_view funIndexToApiKey(int i){
    switch (i){
        case 0:
            return "assists";
        case 1:
            return "damageDealt";
        case 2:
            return "kills";
        case 3:
            return "longestKill";
        case 4:
            return "revives";
        case 5:
            return "rideDistance";
        case 6:
            return "swimDistance";
        case 7:
            return "walkDistance";
        case 8:
            return "weaponsAcquired";
        default:
            return "kills";
    }
}

std::string apiGamemodeToPost(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    bool capitalizeNext = true;
    for (char c : input) {
        if (c == '-') {
            result.push_back(' ');
            capitalizeNext = true;
        } else if (capitalizeNext) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            capitalizeNext = false;
        } else {
            result.push_back(c);
        }
    }
    return result;
}

void populatePubgPost(IdToInt& auraDelta, nlohmann::json& matchJson, PubgPost& currPost, const IdSet& pubgIdSet, IdToSnowflake pubgIdToDiscordUser) {
    nlohmann::json& matchParticipants {matchJson["included"]};
    int funFactIndex {Random::get(0, constants::LastFunTypeIndex)};
    std::string_view funFactKey{funIndexToApiKey(funFactIndex)};
    for (const auto& entity: matchParticipants) {
        if (entity["type"].get<std::string>() != "participant") {continue;}
        const nlohmann::json& participantStats {entity["attributes"]["stats"]};
        // Get our fun fact data
        currPost.funFact.type = funFactIndex;
        if (participantStats[funFactKey]>currPost.funFact.data){
            currPost.funFact.data = participantStats[funFactKey];
            currPost.funFact.user = pubgIdSet.contains(participantStats["playerId"]) ? std::format("<@{}>", pubgIdToDiscordUser[participantStats["playerId"]].str()) : currPost.funFact.user=participantStats["name"];
        }
        // Get our target player data
        if (!pubgIdSet.contains(participantStats["playerId"])){continue;}
        PubgPlayerSummary currSummary {
            participantStats["name"],
            participantStats["kills"],
            participantStats["revives"],
            participantStats["assists"],
            participantStats["longestKill"],
            participantStats["damageDealt"]
        };
        currPost.duration                               = std::max(currPost.duration, participantStats["timeSurvived"].get<int>()/60.0);
        currPost.players[participantStats["playerId"]]  = currSummary;
        currPost.isWon                                  = participantStats["winPlace"]==1 ? true : false;
        currPost.winPlace                               = (currPost.winPlace < participantStats["winPlace"].get<int>()) ? participantStats["winPlace"].get<int>() : currPost.winPlace;
        auraDelta[participantStats["playerId"]]         += participantStats["revives"].get<int>()*3 + participantStats["kills"].get<int>();
    }
    currPost.matchStartTime = UTCStringToTimeT(matchJson["data"]["attributes"]["createdAt"].get<std::string>(), "%Y-%m-%dT%H:%M:%SZ");
    currPost.gameMode       = apiGamemodeToPost(matchJson["data"]["attributes"]["gameMode"].get<std::string>());
    currPost.mapName        = matchJson["data"]["attributes"]["mapName"].get<std::string>();
}

void populatePubgPostsAndAuraDelta(std::string& P_TOKEN, IdToSet& matchIdToPubgId, PPostVec& pubgPosts, IdToInt auraDelta, IdToSnowflake pubgIdToDiscordUser) {
    for (const auto& [matchId, pubgIdSet]: matchIdToPubgId) {
        cpr::Response matchResponse = cpr::Get(cpr::Url{"https://api.pubg.com/shards/steam/matches/"+matchId},
                                               cpr::Header{{"accept", "application/vnd.api+json"}});
        if (matchResponse.status_code!=200) {continue;}
        nlohmann::json matchJson {nlohmann::json::parse(matchResponse.text)};        
        PubgPost currPost {};
        populatePubgPost(auraDelta, matchJson, currPost, pubgIdSet, pubgIdToDiscordUser);
        pubgPosts.push_back(currPost);
    }
}

std::string getEmbedDescriptionString(const PubgPost& pubgPost) {
    std::string description {std::format("Match went on for {:.1f} minutes, started <t:{}:R>. Top ", pubgPost.duration, pubgPost.matchStartTime)};
    std::string funFact{};
    switch (pubgPost.funFact.type) {
    case 0:
        funFact = "assistant was {} with {:.0f} assists.";
        break;
    case 1:
        funFact = "damage dealer was {} with {:.1f} damage.";    
        break;
    case 2:
        funFact = "killer was {} with {:.0f} kills.";
        break;
    case 3:
        funFact = "distance kill was by {} with {:.1f}m.";
        break;
    case 4:
        funFact = "medic was {} with {:.0f} revives.";
        break;
    case 5:
        funFact = "driver was {} with {:.1f}m driven.";
        break;
    case 6:
        funFact = "swimmer was {} with {:.1f}m swam.";
        break;
    case 7:
        funFact = "car-less fucker was {} with {:.1f}m walked.";
        break;
    case 8:
        funFact = "loot goblin was {} with {:.0f} weapons looted.";
        break;
    }
    return description+std::vformat(funFact, std::make_format_args(pubgPost.funFact.user, pubgPost.funFact.data));
}

dpp::message getPubgPostMessage(dpp::snowflake channelId, const PubgPost& pubgPost, std::unordered_map<std::string, dpp::snowflake>& pubgIdToDiscordUser) {
    dpp::embed resEmbed{};
    dpp::message resMessage{channelId, ""};

    resMessage.add_file("pubg.png", dpp::utility::read_file("images/pubgicon.png"));
    resMessage.add_file("map.png", dpp::utility::read_file(std::format("images/maps/{}.png", pubgPost.mapName)));
    resMessage.add_file("jjbLogo.png", dpp::utility::read_file("images/jjb/jjbLogo.png"));

    if (pubgPost.isWon) {
        resEmbed.set_color(dpp::colors::bright_gold);
        resMessage.set_allowed_mentions(true);
    } else {
        resEmbed.set_color(dpp::colors::alien_gray);
    }

    std::string playerString {};
    std::string statsString {};
    for (const auto& [pubgId,pubgPlayerSummary]: pubgPost.players) {
        playerString+=std::format("<@{}>\n", pubgIdToDiscordUser[pubgId].str());
        statsString+=std::format("K:{}-R:{}-A:{}-LK:{:.1f}m-D:{:.1f}\n------\n", pubgPlayerSummary.kills, pubgPlayerSummary.revives, pubgPlayerSummary.assists, pubgPlayerSummary.longestKill, pubgPlayerSummary.damageDealt);
    }

    resEmbed.set_author("JJB", "https://stormblessed.fr/", "attachment://jjbLogo.png")
            .set_thumbnail("attachment://pubg.png")
            .set_image("attachment://map.png")
            .set_title(std::format("#{} Place", pubgPost.winPlace))
            .add_field(
            pubgPost.gameMode,
            getEmbedDescriptionString(pubgPost)
            )
            .add_field(
            "Players",
            playerString,
            true
            )
            .add_field(
                "Stats",
                statsString,
                true
            )
            .set_timestamp(pubgPost.matchStartTime);

    resMessage.add_embed(resEmbed);
    return resMessage;
}

dpp::task<void> PubgPollingMain(std::string& P_TOKEN, IdToId& pubglastKnownMatches, dpp::cluster& bot, const dpp::snowflake& JBBChannel, std::chrono::system_clock& myclock) {
    sqlite3* db{};
    char* zErrMsg{};
    sqlite3_open("data/thediscord", &db);

    // Populate pubgIdToDiscordUser with the PubgIdIds we need
    IdToSnowflake pubgIdToDiscordUser{};
    sqlite3_exec(db, sql::selectPubgIds, sql::fillPubgIdUserIdHashMapFromRecords, static_cast<void*>(&pubgIdToDiscordUser), &zErrMsg);
    
    // For each pubgId:
    // 1. we request /players
    // 2. compare to the one held in "lastKnownMatch"
    // 3. populate "pubgIdToMatches" string:set()
    IdToSet pubgIdToMatches{};
    populatePubgIdToMatches(pubgIdToMatches, P_TOKEN, pubglastKnownMatches, pubgIdToDiscordUser);
    
    // For each pubgId in "pubgIdToMatches" we iterate through
    // the matches and build out a "matchIdToPubgId" string:set()
    IdToSet matchIdToPubgId {};
    populateMatchIdToPubgId(matchIdToPubgId, pubgIdToMatches);
    
    // For each match in "matchIdToPubgId"
    // 1. Call the /matches endpoint
    // 2. Populate a PubgPost struct
    // 3. Populate PubgPosts, append the post structs to the list
    // 4. Calculate how much aura we add
    PPostVec pubgPosts {};
    IdToInt auraDelta{};
    populatePubgPostsAndAuraDelta(P_TOKEN, matchIdToPubgId, pubgPosts, auraDelta, pubgIdToDiscordUser);

    // Sort the posts list
    std::sort(pubgPosts.begin(), pubgPosts.end(),[](PubgPost const& lhs, PubgPost const& rhs){return lhs.matchStartTime < rhs.matchStartTime;});

    // Create a message for each post in the list
    for (const PubgPost& pubgPost: pubgPosts) {
        co_await bot.co_message_create(getPubgPostMessage(JBBChannel, pubgPost, pubgIdToDiscordUser));
    }

    // Update the aura for each leagueId in auraDelta
    auto timeNow = std::chrono::duration_cast<std::chrono::milliseconds>((myclock.now()).time_since_epoch()).count();
    for (const auto& [pId, balanceChange]: auraDelta) {
        std::string discordId {pubgIdToDiscordUser[pId].str()};
        sqlite3_exec(db, std::format(sql::updateBalanceByUser, balanceChange, discordId).c_str(), NULL, NULL, &zErrMsg);
        sqlite3_exec(db, std::format(sql::transactionInsert, "NULL", discordId, balanceChange, timeNow, "\"Pubg Match\"", 2).c_str(), NULL, NULL, &zErrMsg);
    }

    // Log completion of the pubg polling function
    bot.log(dpp::loglevel::ll_info, "Finished PUBG polling function");
    sqlite3_close(db);
}
}