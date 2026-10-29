#pragma once

#include <Includes/Includes.hpp>
#include <Core/SDK/Memory.hpp>
#include <Core/SDK/SDK.hpp>

#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <vector>

namespace Core::Features
{
    enum class PlayerType
    {
        Unknown,
        Player,
        Friend,
        NPC
    };

    struct PlayerInfo
    {
        std::string name;
        PlayerType type;
        int netId;
        bool shouldIgnore;

        PlayerInfo()
            : name("Unknown"),
            type(PlayerType::Unknown),
            netId(-1),
            shouldIgnore(false)
        {
        }

        PlayerInfo(const std::string& n, PlayerType t, int id, bool ignore = false)
            : name(n),
            type(t),
            netId(id),
            shouldIgnore(ignore)
        {
        }
    };

    class UpdateNames
    {
    private:
        static std::unordered_set<std::string> friendList;
        static std::unordered_set<std::string> ignoredNPCs;
        static std::unordered_map<int, PlayerInfo> playerCache;
        static std::unordered_map<int, std::chrono::steady_clock::time_point> unknownNameRetryMap;

        static std::mutex cacheMutex;
        static bool ignoreAllNPCs;

        static void TrimFriendName(std::string& name)
        {
            name.erase(name.begin(), std::find_if(name.begin(), name.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            name.erase(std::find_if(name.rbegin(), name.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), name.end());
        }

        static std::string NormalizeFriendName(std::string name)
        {
            TrimFriendName(name);

            std::string normalized;
            normalized.reserve(name.size());

            bool previousSpace = false;
            for (unsigned char ch : name)
            {
                if (std::isspace(ch))
                {
                    if (!previousSpace && !normalized.empty())
                        normalized += ' ';

                    previousSpace = true;
                    continue;
                }

                normalized += static_cast<char>(std::tolower(ch));
                previousSpace = false;
            }

            TrimFriendName(normalized);
            return normalized;
        }

    public:
        static std::string Utf16ToUtf8(const std::u16string& u16str)
        {
            if (u16str.empty())
                return "";

            int sizeNeeded = WideCharToMultiByte(
                CP_UTF8,
                0,
                reinterpret_cast<LPCWCH>(u16str.c_str()),
                static_cast<int>(u16str.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            );

            if (sizeNeeded <= 0)
                return "";

            std::string result(sizeNeeded, 0);

            WideCharToMultiByte(
                CP_UTF8,
                0,
                reinterpret_cast<LPCWCH>(u16str.c_str()),
                static_cast<int>(u16str.size()),
                result.data(),
                sizeNeeded,
                nullptr,
                nullptr
            );

            return result;
        }

        static std::string SanitizeName(const std::string& name)
        {
            if (name.empty())
                return "";

            std::string out;
            out.reserve(name.size());

            for (size_t i = 0; i < name.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(name[i]);

                if (c >= 32 && c <= 126)
                {
                    out += c;
                    continue;
                }

                if ((c & 0xE0) == 0xC0)
                {
                    if (i + 1 < name.size())
                    {
                        out += name[i];
                        out += name[i + 1];
                        ++i;
                    }
                    continue;
                }

                if ((c & 0xF0) == 0xE0)
                {
                    if (i + 2 < name.size())
                    {
                        out += name[i];
                        out += name[i + 1];
                        out += name[i + 2];
                        i += 2;
                    }
                    continue;
                }

                if ((c & 0xF8) == 0xF0)
                {
                    if (i + 3 < name.size())
                    {
                        out += name[i];
                        out += name[i + 1];
                        out += name[i + 2];
                        out += name[i + 3];
                        i += 3;
                    }
                    continue;
                }
            }

            if (out.empty() || out == "???")
                return "";

            return out;
        }

        static std::string ReadUtf16PlayerName(uintptr_t addr, size_t maxChars = 128)
        {
            if (!addr || maxChars == 0)
                return "";

            std::u16string u16name;
            u16name.reserve(maxChars);

            for (size_t i = 0; i < maxChars; ++i)
            {
                char16_t ch = Mem.Read<char16_t>(addr + (i * sizeof(char16_t)));

                if (ch == u'\0')
                    break;

                // invalid utf16
                if (ch == 0xFFFF || ch == 0xFFFE)
                    break;

                u16name.push_back(ch);
            }

            return SanitizeName(Utf16ToUtf8(u16name));
        }

        static std::string ReadPlayerNameSmart(uintptr_t addr)
        {
            if (!addr)
                return "";

            std::string result;

            result = Mem.ReadString(addr);
            result = SanitizeName(result);

            if (result.empty())
            {
                result = ReadUtf16PlayerName(addr);
            }

            if (result.empty())
            {
                result = Mem.ReadWString2(addr);
                result = SanitizeName(result);
            }

            if (result == "???")
                result.clear();

            return result;
        }

        static void AddFriend(const std::string& playerName)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            const std::string normalizedPlayerName = NormalizeFriendName(playerName);

            if (normalizedPlayerName.empty())
                return;

            friendList.insert(normalizedPlayerName);

            for (auto& [netId, info] : playerCache)
            {
                if (NormalizeFriendName(info.name) == normalizedPlayerName)
                    info.type = PlayerType::Friend;
            }
        }

        static void RemoveFriend(const std::string& playerName)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            const std::string normalizedPlayerName = NormalizeFriendName(playerName);

            if (normalizedPlayerName.empty())
                return;

            friendList.erase(normalizedPlayerName);

            for (auto& [netId, info] : playerCache)
            {
                if (NormalizeFriendName(info.name) == normalizedPlayerName && info.type == PlayerType::Friend)
                {
                    info.type = PlayerType::Player;
                }
            }
        }

        static void SetFriendList(const std::vector<std::string>& playerNames)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);

            friendList.clear();
            for (const auto& playerName : playerNames)
            {
                const std::string normalizedPlayerName = NormalizeFriendName(playerName);
                if (!normalizedPlayerName.empty())
                    friendList.insert(normalizedPlayerName);
            }

            for (auto& [netId, info] : playerCache)
            {
                if (friendList.find(NormalizeFriendName(info.name)) != friendList.end())
                    info.type = PlayerType::Friend;
                else if (info.type == PlayerType::Friend)
                    info.type = PlayerType::Player;
            }
        }

        static bool IsFriend(const std::string& playerName)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            return friendList.find(NormalizeFriendName(playerName)) != friendList.end();
        }

        static const std::unordered_set<std::string>& GetFriendList()
        {
            return friendList;
        }

        static void SetIgnoreAllNPCs(bool ignore)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);

            ignoreAllNPCs = ignore;

            for (auto& [netId, info] : playerCache)
            {
                if (info.type == PlayerType::NPC)
                    info.shouldIgnore = ignore;
            }
        }

        static void AddIgnoredNPC(const std::string& npcName)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);

            ignoredNPCs.insert(npcName);

            for (auto& [netId, info] : playerCache)
            {
                if (info.name == npcName && info.type == PlayerType::NPC)
                {
                    info.shouldIgnore = true;
                }
            }
        }

        static void RemoveIgnoredNPC(const std::string& npcName)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);

            ignoredNPCs.erase(npcName);

            for (auto& [netId, info] : playerCache)
            {
                if (info.name == npcName && info.type == PlayerType::NPC)
                {
                    info.shouldIgnore = ignoreAllNPCs;
                }
            }
        }

        static bool ShouldIgnoreNPC(const std::string& npcName)
        {
            std::lock_guard<std::mutex> lock(cacheMutex);

            return ignoreAllNPCs ||
                (ignoredNPCs.find(npcName) != ignoredNPCs.end());
        }

        static const std::unordered_set<std::string>& GetIgnoredNPCs()
        {
            return ignoredNPCs;
        }

        static bool GetIgnoreAllNPCs()
        {
            return ignoreAllNPCs;
        }

        static std::string getPlayerNameByNetId(int netid)
        {
            try
            {
                if (netid <= 0 ||
                    g_Variables.ProcIdFiveM == 0 ||
                    !Mem.IsProcessAlive())
                {
                    return "";
                }

                uintptr_t ModSize = 0;

                uintptr_t playernames = Mem.GetModuleBaseAddr(
                    g_Variables.ProcIdFiveM,
                    "citizen-playernames-five.dll",
                    &ModSize
                );

                if (!playernames)
                    return "";

                static uintptr_t cachedPlayerNamesOffset = 0;

                if (cachedPlayerNamesOffset == 0)
                {
                    std::vector<uint8_t> pattern =
                    {
                        0x48, 0x8B, 0x15,
                        0x00, 0x00, 0x00, 0x00,
                        0x48, 0xC1, 0xE1
                    };

                    uintptr_t patternAddr = Mem.FindSignatureBypass(
                        pattern,
                        playernames,
                        ModSize
                    );

                    if (patternAddr)
                    {
                        int32_t relativeOffset =
                            Mem.Read<int32_t>(patternAddr + 3);

                        uintptr_t resolved =
                            patternAddr + 7 + relativeOffset;

                        cachedPlayerNamesOffset =
                            resolved - playernames;
                    }
                }

                uintptr_t PlayerNamesArray = Mem.Read<uintptr_t>(
                    playernames + cachedPlayerNamesOffset
                );

                if (!PlayerNamesArray)
                    return "";

                int LastPlayer = Mem.Read<int>(
                    playernames + cachedPlayerNamesOffset + 0x8
                );

                if (LastPlayer <= 0 || LastPlayer > 2048)
                    return "";

                uintptr_t list = Mem.Read<uintptr_t>(PlayerNamesArray + 0x8);

                if (!list)
                    return "";

                for (int i = 0; i < LastPlayer; i++)
                {
                    try
                    {
                        if (!list)
                            break;

                        int id = Mem.Read<int>(list + 0x10);

                        if (id == netid)
                        {
                            std::string foundName;

                            uintptr_t namePtr =
                                Mem.Read<uintptr_t>(list + 0x18);

                            if (namePtr)
                            {
                                foundName = ReadPlayerNameSmart(namePtr);
                            }

                            if (foundName.empty())
                            {
                                foundName =
                                    ReadPlayerNameSmart(list + 0x18);
                            }

                            if (foundName.empty())
                            {
                                std::vector<uintptr_t> offsets =
                                {
                                    0x20,
                                    0x28,
                                    0x30,
                                    0x38,
                                    0x40
                                };

                                for (uintptr_t off : offsets)
                                {
                                    uintptr_t altPtr =
                                        Mem.Read<uintptr_t>(list + off);

                                    if (!altPtr)
                                        continue;

                                    foundName =
                                        ReadPlayerNameSmart(altPtr);

                                    if (!foundName.empty())
                                        break;
                                }
                            }

                            foundName = SanitizeName(foundName);

                            if (!foundName.empty())
                                return foundName;

                            break;
                        }

                        list = Mem.Read<uintptr_t>(list + 0x8);
                    }
                    catch (...)
                    {
                        break;
                    }
                }

                return "";
            }
            catch (...)
            {
                return "";
            }
        }

        static std::string GetName(CPed* ped)
        {
            try
            {
                if (!ped)
                    return "Unknown";

                PlayerInfo info = GetPlayerInfo(ped);

                if (info.name == "NPC")
                    info.name = "npc";

                if ((info.name.empty() || info.name == "NPC") &&
                    ped == Core::SDK::Pointers::pLocalPlayer)
                {
                    if (!g_Variables.UserName.empty())
                        return g_Variables.UserName;
                }

                return info.name.empty() ? "NPC" : info.name;
            }
            catch (...)
            {
                return "Unknown";
            }
        }

        static PlayerInfo GetPlayerInfo(CPed* ped)
        {
            try
            {
                if (!ped)
                    return PlayerInfo();

                int netId = ped->GetID();

                auto now = std::chrono::steady_clock::now();

                if (netId <= 0)
                {
                    std::lock_guard<std::mutex> lock(cacheMutex);

                    return PlayerInfo(
                        "NPC",
                        PlayerType::NPC,
                        netId,
                        ignoreAllNPCs
                    );
                }

                {
                    std::lock_guard<std::mutex> lock(cacheMutex);

                    auto it = playerCache.find(netId);

                    if (it != playerCache.end())
                    {
                        if (!it->second.name.empty() &&
                            it->second.name != "Unknown")
                        {
                            return it->second;
                        }

                        auto retryIt =
                            unknownNameRetryMap.find(netId);

                        if (retryIt != unknownNameRetryMap.end())
                        {
                            auto elapsed =
                                std::chrono::duration_cast<
                                std::chrono::milliseconds>(
                                    now - retryIt->second
                                );

                            if (elapsed.count() < 500)
                                return it->second;
                        }
                    }
                }

                std::string name = getPlayerNameByNetId(netId);

                if (name.empty())
                {
                    std::lock_guard<std::mutex> lock(cacheMutex);
                    unknownNameRetryMap[netId] = now;
                }

                PlayerType type = PlayerType::Unknown;
                bool shouldIgnore = false;

                {
                    std::lock_guard<std::mutex> lock(cacheMutex);

                    if (!name.empty())
                    {
                        type =
                            friendList.find(NormalizeFriendName(name)) != friendList.end()
                            ? PlayerType::Friend
                            : PlayerType::Player;
                    }
                }

                PlayerInfo info(
                    name.empty() ? "Unknown" : name,
                    type,
                    netId,
                    shouldIgnore
                );

                {
                    std::lock_guard<std::mutex> lock(cacheMutex);
                    playerCache[netId] = info;
                }

                return info;
            }
            catch (...)
            {
                return PlayerInfo();
            }
        }

        static bool ShouldIgnorePlayer(CPed* ped)
        {
            return GetPlayerInfo(ped).shouldIgnore;
        }

        static PlayerType GetPlayerType(CPed* ped)
        {
            return GetPlayerInfo(ped).type;
        }

        static void ClearCache()
        {
            std::lock_guard<std::mutex> lock(cacheMutex);

            playerCache.clear();
            unknownNameRetryMap.clear();
        }

        static size_t GetCacheSize()
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            return playerCache.size();
        }

        static void ForceResync()
        {
            ClearCache();
        }

        static void ForceResync(CPed* ped)
        {
            if (!ped)
                return;

            std::lock_guard<std::mutex> lock(cacheMutex);

            int netId = ped->GetID();

            playerCache.erase(netId);
            unknownNameRetryMap.erase(netId);
        }

    private:
        static std::string GetNameFromPlayerInfo(CPed* ped)
        {
            return GetName(ped);
        }
    };
}
