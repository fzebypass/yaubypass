#include "UpdateNames.hpp"

namespace Core::Features {
    std::unordered_set<std::string> UpdateNames::friendList;
    std::unordered_set<std::string> UpdateNames::ignoredNPCs;
    std::unordered_map<int, PlayerInfo> UpdateNames::playerCache;
    std::mutex UpdateNames::cacheMutex;
    bool UpdateNames::ignoreAllNPCs = false;
    std::unordered_map<int, std::chrono::steady_clock::time_point> UpdateNames::unknownNameRetryMap;
}