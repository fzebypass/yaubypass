#pragma once
#include <unordered_set>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>

class CPed;

namespace Core::Features
{
    class cFriends
    {
    public:
        static inline std::unordered_set<std::string> FriendNames;
        static inline std::unordered_set<std::string> FriendNameKeys;
        static inline std::mutex FriendMutex;

        static inline bool BoneGroup_Head = true;
        static inline bool BoneGroup_Neck = true;
        static inline bool BoneGroup_Chest = true;
        static inline bool BoneGroup_Arms = true;
        static inline bool BoneGroup_Legs = true;

        static std::string FriendsToString();
        static void StringToFriends(const std::string& str);

        static bool IsFriend(CPed* ped);
        void AddFriend(CPed* ped);
        void RemoveFriend(CPed* ped);

        void Start();
    };

    inline cFriends g_Friends;
}
