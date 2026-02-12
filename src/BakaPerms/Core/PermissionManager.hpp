#pragma once
#include "BakaPerms/Core/Types.hpp"
#include "BakaPerms/Data/PermissionRepository.hpp"
#include "BakaPerms/Database/IDatabase.hpp"
#include "BakaPerms/Utils/Macros.h"

#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace BakaPerms::core {

class PermissionManager {
public:
    explicit PermissionManager(std::unique_ptr<database::IDatabase> db);

    // Permission checking
    BAKA_PERMAPI auto checkPermission(std::string_view playerUuid, std::string_view node) -> AccessMask;

    // Trace
    BAKA_PERMAPI auto tracePermission(SubjectKind kind, std::string_view uuid, std::string_view node) const
        -> PermissionTrace;

    // ACL management
    BAKA_PERMAPI void appendACE(std::string_view node, std::string_view subjectUuid, int subjectType, AccessMask mask);
    BAKA_PERMAPI void
    insertACE(std::string_view node, int position, std::string_view subjectUuid, int subjectType, AccessMask mask);
    BAKA_PERMAPI void removeACE(std::string_view node, int position);
    BAKA_PERMAPI void moveACE(std::string_view node, int from, int to);
    BAKA_PERMAPI auto getNodeACL(std::string_view node) const -> std::vector<ACE>;
    BAKA_PERMAPI void clearNodeACL(std::string_view node);

    // Query ACEs by subject (for display)
    BAKA_PERMAPI auto getSubjectACEs(std::string_view subjectUuid) const
        -> std::vector<data::PermissionRepository::NodeACE>;

    // Group management
    BAKA_PERMAPI auto createGroup(std::string_view name, const std::optional<std::string_view>& parentUuid) const
        -> std::string;
    BAKA_PERMAPI void deleteGroup(std::string_view groupUuid);
    BAKA_PERMAPI void setGroupParent(std::string_view groupUuid, const std::optional<std::string_view>& parentUuid);

    BAKA_PERMAPI auto getGroup(std::string_view uuid) const -> std::optional<GroupInfo>;
    BAKA_PERMAPI auto getGroupByName(std::string_view name) const -> std::optional<GroupInfo>;
    BAKA_PERMAPI auto getAllGroups() const -> std::vector<GroupInfo>;

    // Membership
    [[nodiscard]] BAKA_PERMAPI bool addPlayerToGroup(std::string_view playerUuid, std::string_view groupUuid);
    [[nodiscard]] BAKA_PERMAPI bool removePlayerFromGroup(std::string_view playerUuid, std::string_view groupUuid);
    BAKA_PERMAPI auto getPlayerGroups(std::string_view playerUuid) const -> std::vector<GroupInfo>;
    BAKA_PERMAPI auto getGroupMembers(std::string_view groupUuid) const -> std::vector<std::string>;

    // Cache
    BAKA_PERMAPI void invalidatePlayer(std::string_view uuid);
    BAKA_PERMAPI void invalidateAll();

private:
    auto buildToken(SubjectKind kind, std::string_view uuid) const -> AccessToken;
    auto resolvePermission(std::string_view playerUuid, std::string_view node) const -> AccessMask;
    bool wouldCreateCycle(std::string_view groupUuid, std::string_view parentUuid) const;

    std::unique_ptr<database::IDatabase> db_;
    data::PermissionRepository           repo_;

    mutable std::shared_mutex                                                    cacheMutex_;
    uint64_t                                                                     cacheGeneration_{0};
    std::unordered_map<std::string, std::unordered_map<std::string, AccessMask>> cache_;
};

} // namespace BakaPerms::core
