#pragma once
#include "BakaPerms/Core/IPermissionManager.hpp"
#include "BakaPerms/Core/Types.hpp"
#include "BakaPerms/Data/PermissionRepository.hpp"
#include "BakaPerms/Database/IDatabase.hpp"

#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace BakaPerms::core {

class PermissionManager final : public IPermissionManager {
public:
    explicit PermissionManager(std::unique_ptr<database::IDatabase> db);

    // ll::service lifecycle
    void invalidate() override;

    // Permission checking
    auto checkPermission(std::string_view playerUuid, std::string_view node) -> AccessMask override;

    // Trace
    auto tracePermission(SubjectKind kind, std::string_view uuid, std::string_view node) const
        -> PermissionTrace override;

    // ACL management
    void appendACE(std::string_view node, std::string_view subjectUuid, int subjectType, AccessMask mask) override;
    void insertACE(
        std::string_view node,
        int              position,
        std::string_view subjectUuid,
        int              subjectType,
        AccessMask       mask
    ) override;
    void removeACE(std::string_view node, int position) override;
    void moveACE(std::string_view node, int from, int to) override;
    auto getNodeACL(std::string_view node) const -> std::vector<ACE> override;
    void clearNodeACL(std::string_view node) override;

    // Group management
    auto createGroup(std::string_view name, const std::optional<std::string_view>& parentUuid) const
        -> std::string override;
    void deleteGroup(std::string_view groupUuid) override;
    void setGroupParent(std::string_view groupUuid, const std::optional<std::string_view>& parentUuid) override;
    auto getGroup(std::string_view uuid) const -> std::optional<GroupInfo> override;
    auto getGroupByName(std::string_view name) const -> std::optional<GroupInfo> override;
    auto getAllGroups() const -> std::vector<GroupInfo> override;

    // Membership
    [[nodiscard]] bool addPlayerToGroup(std::string_view playerUuid, std::string_view groupUuid) override;
    [[nodiscard]] bool removePlayerFromGroup(std::string_view playerUuid, std::string_view groupUuid) override;
    auto               getPlayerGroups(std::string_view playerUuid) const -> std::vector<GroupInfo> override;
    auto               getGroupMembers(std::string_view groupUuid) const -> std::vector<std::string> override;

    // Internal: query ACEs by subject (for display)
    auto getSubjectACEs(std::string_view subjectUuid) const -> std::vector<NodeACE>;

    // Internal: cache management
    void invalidatePlayer(std::string_view uuid);
    void invalidateAll();

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
