#pragma once
#include "BakaPerms/Core/Types.hpp"
#include "BakaPerms/Database/IDatabase.hpp"
#include "BakaPerms/Utils/Macros.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace BakaPerms::data {

class PermissionRepository {
public:
    explicit PermissionRepository(database::IDatabase& db);

    void initializeSchema() const;

    // Groups
    BAKA_PERMAPI void createGroup(
        std::string_view                       uuid,
        std::string_view                       name,
        const std::optional<std::string_view>& parentUuid
    ) const;
    BAKA_PERMAPI void deleteGroup(std::string_view uuid) const;
    BAKA_PERMAPI void setGroupParent(std::string_view uuid, const std::optional<std::string_view>& parentUuid) const;
    [[nodiscard]] BAKA_PERMAPI auto getGroup(std::string_view uuid) const -> std::optional<core::GroupInfo>;
    [[nodiscard]] BAKA_PERMAPI auto getGroupByName(std::string_view name) const -> std::optional<core::GroupInfo>;
    [[nodiscard]] BAKA_PERMAPI auto getAllGroups() const -> std::vector<core::GroupInfo>;

    // Membership
    [[nodiscard]] BAKA_PERMAPI bool addPlayerToGroup(std::string_view playerUuid, std::string_view groupUuid) const;
    [[nodiscard]] BAKA_PERMAPI bool removePlayerFromGroup(std::string_view playerUuid, std::string_view groupUuid) const;
    [[nodiscard]] BAKA_PERMAPI auto getPlayerGroups(std::string_view playerUuid) const
        -> std::vector<core::GroupInfo>;
    [[nodiscard]] BAKA_PERMAPI auto getGroupMembers(std::string_view groupUuid) const -> std::vector<std::string>;

    // Ancestry
    [[nodiscard]] BAKA_PERMAPI auto getGroupAncestry(std::string_view groupUuid) const
        -> std::vector<core::GroupInfo>;

    // ACL operations
    BAKA_PERMAPI void
    appendACE(std::string_view node, std::string_view subjectUuid, int subjectType, core::AccessMask mask) const;
    BAKA_PERMAPI void insertACE(
        std::string_view node,
        int              position,
        std::string_view subjectUuid,
        int              subjectType,
        core::AccessMask mask
    ) const;
    BAKA_PERMAPI void               removeACE(std::string_view node, int orderIndex) const;
    BAKA_PERMAPI void               moveACE(std::string_view node, int fromIndex, int toIndex) const;
    [[nodiscard]] BAKA_PERMAPI auto getNodeACL(std::string_view node) const -> std::vector<core::ACE>;
    BAKA_PERMAPI void               clearNodeACL(std::string_view node) const;

    // Query ACEs by subject (for info display)
    struct NodeACE {
        std::string node;
        core::ACE   ace;
    };
    [[nodiscard]] BAKA_PERMAPI auto getSubjectACEs(std::string_view subjectUuid) const -> std::vector<NodeACE>;

    // Batch ACL lookup for multiple nodes in a single query
    [[nodiscard]] BAKA_PERMAPI auto getNodeACLBatch(const std::vector<std::string>& nodes) const
        -> std::unordered_map<std::string, std::vector<core::ACE>>;

private:
    void reindexACL(std::string_view node) const;
    void writeACL(std::string_view node, const std::vector<core::ACE>& acl) const;

    database::IDatabase& db_;
};

} // namespace BakaPerms::data
