#pragma once
#include "BakaPerms/Core/Types.hpp"
#include "BakaPerms/Database/IDatabase.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace BakaPerms::data {

class PermissionRepository {
public:
    explicit PermissionRepository(database::IDatabase& db);

    void initializeSchema() const;

    // Groups
    void
    createGroup(std::string_view uuid, std::string_view name, const std::optional<std::string_view>& parentUuid) const;
    void               deleteGroup(std::string_view uuid) const;
    void               setGroupParent(std::string_view uuid, const std::optional<std::string_view>& parentUuid) const;
    [[nodiscard]] auto getGroup(std::string_view uuid) const -> std::optional<core::GroupInfo>;
    [[nodiscard]] auto getGroupByName(std::string_view name) const -> std::optional<core::GroupInfo>;
    [[nodiscard]] auto getAllGroups() const -> std::vector<core::GroupInfo>;

    // Membership
    [[nodiscard]] bool addPlayerToGroup(std::string_view playerUuid, std::string_view groupUuid) const;
    [[nodiscard]] bool removePlayerFromGroup(std::string_view playerUuid, std::string_view groupUuid) const;
    [[nodiscard]] auto getPlayerGroups(std::string_view playerUuid) const -> std::vector<core::GroupInfo>;
    [[nodiscard]] auto getGroupMembers(std::string_view groupUuid) const -> std::vector<std::string>;

    // Ancestry
    [[nodiscard]] auto getGroupAncestry(std::string_view groupUuid) const -> std::vector<core::GroupInfo>;

    // ACL operations
    void appendACE(std::string_view node, std::string_view subjectUuid, int subjectType, core::AccessMask mask) const;
    void insertACE(
        std::string_view node,
        int              position,
        std::string_view subjectUuid,
        int              subjectType,
        core::AccessMask mask
    ) const;
    void               removeACE(std::string_view node, int orderIndex) const;
    void               moveACE(std::string_view node, int fromIndex, int toIndex) const;
    [[nodiscard]] auto getNodeACL(std::string_view node) const -> std::vector<core::ACE>;
    void               clearNodeACL(std::string_view node) const;

    // Query ACEs by subject
    [[nodiscard]] auto getSubjectACEs(std::string_view subjectUuid) const -> std::vector<core::NodeACE>;

    // Batch ACL lookup for multiple nodes in a single query
    [[nodiscard]] auto getNodeACLBatch(const std::vector<std::string>& nodes) const
        -> std::unordered_map<std::string, std::vector<core::ACE>>;

private:
    void reindexACL(std::string_view node) const;
    void writeACL(std::string_view node, const std::vector<core::ACE>& acl) const;

    database::IDatabase& db_;
};

} // namespace BakaPerms::data
