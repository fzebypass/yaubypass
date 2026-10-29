#include "Friends.hpp"
#include "../SDK/SDK.hpp"
#include <Core/Lifecycle.hpp>
#include "../Offsets.hpp"
#include "../Threads/UpdateNames.hpp"
#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <unordered_set>

namespace Core::Features
{
    static bool IsValidFriendName(const std::string& name)
    {
        return !name.empty() &&
            name != "Unknown" &&
            name != "NPC" &&
            name != "npc" &&
            name != "unknown player";
    }

    static std::string GetFriendName(CPed* ped)
    {
        if (!ped)
            return "";

        std::string name = UpdateNames::GetName(ped);
        return IsValidFriendName(name) ? name : "";
    }

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

    static std::vector<std::string> ParseFriendNames(const std::string& str)
    {
        std::vector<std::string> names;
        if (str.empty())
            return names;

        std::unordered_set<std::string> seen;
        auto addName = [&names](std::string name) {
            TrimFriendName(name);
            if (!IsValidFriendName(name))
                return;

            names.push_back(name);
        };
        auto addUniqueName = [&names, &seen](std::string name) {
            TrimFriendName(name);
            if (!IsValidFriendName(name))
                return;

            std::string key = NormalizeFriendName(name);
            if (!key.empty() && seen.insert(key).second)
                names.push_back(name);
        };

        const bool hasNewline = str.find('\n') != std::string::npos;
        const bool hasCarriageReturn = str.find('\r') != std::string::npos;
        const bool hasComma = str.find(',') != std::string::npos;
        if (hasNewline || hasCarriageReturn || hasComma)
        {
            std::string normalizedSeparators = str;
            std::replace(normalizedSeparators.begin(), normalizedSeparators.end(), '\r', '\n');
            std::replace(normalizedSeparators.begin(), normalizedSeparators.end(), ',', '\n');

            std::stringstream ss(normalizedSeparators);
            std::string token;
            while (std::getline(ss, token, '\n'))
                addUniqueName(token);

            return names;
        }

        std::string trimmed = str;
        TrimFriendName(trimmed);
        if (trimmed.empty())
            return names;

        std::stringstream words(trimmed);
        std::vector<std::string> parts;
        std::string word;
        while (words >> word)
            parts.push_back(word);

        const bool isRepeatedSingleName = parts.size() > 1 &&
            std::all_of(parts.begin() + 1, parts.end(), [&parts](const std::string& part) {
                return part == parts.front();
            });

        if (isRepeatedSingleName)
            addName(parts.front());
        else
            addName(trimmed);

        return names;
    }

    std::string cFriends::FriendsToString()
    {
        std::lock_guard<std::mutex> lk(FriendMutex);

        if (FriendNames.empty())
            return "";

        std::string result;
        for (auto it = FriendNames.begin(); it != FriendNames.end(); ++it)
        {
            if (it != FriendNames.begin())
                result += "\n";
            result += *it;
        }
        return result;
    }

    void cFriends::StringToFriends(const std::string& str)
    {
        std::vector<std::string> names = ParseFriendNames(str);

        {
            std::lock_guard<std::mutex> lk(FriendMutex);
            FriendNames.clear();
            FriendNameKeys.clear();

            for (const auto& name : names)
            {
                FriendNames.insert(name);
                FriendNameKeys.insert(NormalizeFriendName(name));
            }
        }

        UpdateNames::SetFriendList(names);
    }

    bool cFriends::IsFriend(CPed* ped)
    {
        std::string name = GetFriendName(ped);
        if (name.empty()) return false;

        std::lock_guard<std::mutex> lk(FriendMutex);
        return FriendNameKeys.contains(NormalizeFriendName(name));
    }

    void cFriends::AddFriend(CPed* ped)
    {
        std::string name = GetFriendName(ped);
        if (name.empty()) return;

        {
            std::lock_guard<std::mutex> lk(FriendMutex);
            FriendNames.insert(name);
            FriendNameKeys.insert(NormalizeFriendName(name));
        }

        UpdateNames::AddFriend(name);
    }

    void cFriends::RemoveFriend(CPed* ped)
    {
        std::string name = GetFriendName(ped);
        if (name.empty()) return;

        {
            std::lock_guard<std::mutex> lk(FriendMutex);
            const std::string key = NormalizeFriendName(name);
            for (auto it = FriendNames.begin(); it != FriendNames.end();)
            {
                if (NormalizeFriendName(*it) == key)
                    it = FriendNames.erase(it);
                else
                    ++it;
            }
            FriendNameKeys.erase(key);
        }

        UpdateNames::RemoveFriend(name);
    }

    // ==================== Friend Adder Thread ====================
    static inline std::vector<uint16_t> GetSelectedBoneGroups2()
    {
        std::vector<uint16_t> bones;

            bones.push_back(SKEL_Head);

        return bones;
    }
    struct sClosestPedInfo {
        CPed* ped;
        uint16_t boneId;
        D3DXVECTOR3 bonePosition;
        float screenDistance;
    };

    sClosestPedInfo GetClosestPedInFOV(int fov = 50)
    {
        sClosestPedInfo result{ nullptr, 0, D3DXVECTOR3(0,0,0), FLT_MAX };

        CPed* local = Core::SDK::Pointers::pLocalPlayer;
        if (!local) return result;

        CPed* ped = Core::SDK::Game::GetClosestPed(300.0f, false, false, true, false,false);
        if (!ped) return result;

        uintptr_t frag = Mem.Read<uintptr_t>((uintptr_t)ped + g_Offsets.m_FragInst);
        if (!frag) return result;

        uintptr_t v9 = Mem.Read<uintptr_t>(frag + 0x68);
        if (!v9) return result;

        uintptr_t skeletonPtr = Mem.Read<uintptr_t>(v9 + 0x178);
        if (!skeletonPtr) return result;

        Core::SDK::Game::cSkeleton_t skel{};
        skel.m_pSkeleton = skeletonPtr;

        skel.crSkeletonData.Ptr = Mem.Read<uintptr_t>(skel.m_pSkeleton);
        skel.crSkeletonData.m_Used = Mem.Read<unsigned int>(skel.crSkeletonData.Ptr + 0x1A);
        skel.crSkeletonData.m_NumBones = Mem.Read<unsigned int>(skel.crSkeletonData.Ptr + 0x5E);
        skel.crSkeletonData.m_BoneIdTable_Slots = Mem.Read<unsigned short>(skel.crSkeletonData.Ptr + 0x18);
        skel.crSkeletonData.m_BoneIdTable = Mem.Read<uintptr_t>(skel.crSkeletonData.Ptr + 0x10);

        skel.Arg1 = Mem.Read<D3DXMATRIX>(Mem.Read<uintptr_t>(skel.m_pSkeleton + 0x8));
        skel.Arg2 = Mem.Read<uintptr_t>(skel.m_pSkeleton + 0x18);

        auto bones = GetSelectedBoneGroups2();

        float bestDist = FLT_MAX;
        uint16_t bestBone = 0;
        D3DXVECTOR3 bestPos{};

        for (auto id : bones)
        {
            D3DXVECTOR3 p3 = Core::SDK::Game::GetBonePosComplex(ped, id, skel);
            D3DXVECTOR2 p2 = Core::SDK::Game::WorldToScreen(p3);

            if (!Core::SDK::Game::IsOnScreen(p2)) continue;

            float dx = p2.x - g_Variables.g_vGameWindowCenter.x;
            float dy = p2.y - g_Variables.g_vGameWindowCenter.y;
            float dist = std::hypot(dx, dy);

            if (dist < bestDist && dist <= fov)
            {
                bestDist = dist;
                bestBone = id;
                bestPos = p3;
            }
        }

        if (bestBone != 0)
        {
            result.ped = ped;
            result.boneId = bestBone;
            result.bonePosition = bestPos;
            result.screenDistance = bestDist;
        }

        return result;
    }

    void cFriends::Start()
    {
        static bool wasPressed = false;

        while (!Core::IsShutDownRequested())
        {
            // Je bestaande GetClosestPedInFOV functie gebruiken
            auto closest = GetClosestPedInFOV(50);   // Zorg dat deze functie zichtbaar is

            if (!closest.ped)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                continue;
            }

            bool alt = (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
            bool x = (GetAsyncKeyState('X') & 0x8000) != 0;

            if (alt && x && !wasPressed)
            {
                std::string name = GetFriendName(closest.ped);
                if (!name.empty())
                {
                    bool shouldAdd = false;

                    {
                        std::lock_guard<std::mutex> lk(FriendMutex);
                        const std::string key = NormalizeFriendName(name);

                        if (FriendNameKeys.contains(key))
                        {
                            for (auto it = FriendNames.begin(); it != FriendNames.end();)
                            {
                                if (NormalizeFriendName(*it) == key)
                                    it = FriendNames.erase(it);
                                else
                                    ++it;
                            }
                            FriendNameKeys.erase(key);
                        }
                        else
                        {
                            FriendNames.insert(name);
                            FriendNameKeys.insert(key);
                            shouldAdd = true;
                        }
                    }

                    if (shouldAdd)
                        UpdateNames::AddFriend(name);
                    else
                        UpdateNames::RemoveFriend(name);
                }
                wasPressed = true;
            }
            else if (!(alt && x))
            {
                wasPressed = false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}
