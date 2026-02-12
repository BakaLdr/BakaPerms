#include "BakaPerms/Core/PermissionManager.hpp"

#include "BakaPerms/Core/PermissionResolver.hpp"
#include "BakaPerms/Utils/Exception/Exceptions.hpp"
#include "BakaPerms/Utils/I18n/I18n.hpp"

#include <mc/platform/UUID.h>

namespace BakaPerms::core {

PermissionManager::PermissionManager(std::unique_ptr<database::IDatabase> db) : db_(std::move(db)), repo_(*db_) {
    repo_.initializeSchema();
}

// Permission checking
auto PermissionManager::checkPermission(const std::string_view playerUuid, const std::string_view node) -> AccessMask {
    uint64_t gen;
    {
        std::shared_lock lock(cacheMutex_);
        gen = cacheGeneration_;
        if (const auto playerIt = cache_.find(std::string(playerUuid)); playerIt != cache_.end()) {
            if (const auto nodeIt = playerIt->second.find(std::string(node)); nodeIt != playerIt->second.end()) {
                return nodeIt->second;
            }
        }
    }

    const auto result = resolvePermission(playerUuid, node);

    {
        std::unique_lock lock(cacheMutex_);
        // Only write to cache if no invalidation occurred during resolution.
        // This prevents stale results from being written into a freshly cleared cache.
        if (cacheGeneration_ == gen) {
            cache_[std::string(playerUuid)][std::string(node)] = result;
        }
    }

    return result;
}

// Trace
auto PermissionManager::tracePermission(
    const SubjectKind      kind,
    const std::string_view uuid,
    const std::string_view node
) const -> PermissionTrace {
    const auto token    = buildToken(kind, uuid);
    const auto nodePath = PermissionResolver::buildNodePath(node);
    const auto aclMap   = repo_.getNodeACLBatch(nodePath);
    auto       trace =
        PermissionResolver::resolveWithTrace(node, token, [&aclMap](const std::string_view n) -> std::vector<ACE> {
            if (const auto it = aclMap.find(std::string(n)); it != aclMap.end()) return it->second;
            return {};
        });
    trace.subjectKind = kind;
    trace.subjectUuid = uuid;
    return trace;
}

// ACL management
void PermissionManager::appendACE(
    const std::string_view node,
    const std::string_view subjectUuid,
    const int              subjectType,
    const AccessMask       mask
) {
    repo_.appendACE(node, subjectUuid, subjectType, mask);
    invalidateAll();
}

void PermissionManager::insertACE(
    const std::string_view node,
    const int              position,
    const std::string_view subjectUuid,
    const int              subjectType,
    const AccessMask       mask
) {
    repo_.insertACE(node, position, subjectUuid, subjectType, mask);
    invalidateAll();
}

void PermissionManager::removeACE(const std::string_view node, const int position) {
    repo_.removeACE(node, position);
    invalidateAll();
}

void PermissionManager::moveACE(const std::string_view node, const int from, const int to) {
    repo_.moveACE(node, from, to);
    invalidateAll();
}

auto PermissionManager::getNodeACL(const std::string_view node) const -> std::vector<ACE> {
    return repo_.getNodeACL(node);
}

void PermissionManager::clearNodeACL(const std::string_view node) {
    repo_.clearNodeACL(node);
    invalidateAll();
}

auto PermissionManager::getSubjectACEs(const std::string_view subjectUuid) const
    -> std::vector<data::PermissionRepository::NodeACE> {
    return repo_.getSubjectACEs(subjectUuid);
}

// Group management
auto PermissionManager::createGroup(
    const std::string_view                 name,
    const std::optional<std::string_view>& parentUuid
) const -> std::string {
    if (repo_.getGroupByName(name)) {
        throw utils::exception::OperationFailedException("bakaperms.exception.detail.group_exists"_tr(name));
    }
    auto uuid = mce::UUID::random().asString();
    repo_.createGroup(uuid, name, parentUuid);
    return uuid;
}

void PermissionManager::deleteGroup(const std::string_view groupUuid) {
    repo_.deleteGroup(groupUuid);
    invalidateAll();
}

void PermissionManager::setGroupParent(
    const std::string_view                 groupUuid,
    const std::optional<std::string_view>& parentUuid
) {
    if (parentUuid && wouldCreateCycle(groupUuid, *parentUuid)) {
        throw utils::exception::OperationFailedException("bakaperms.exception.detail.group_cycle"_tr());
    }
    repo_.setGroupParent(groupUuid, parentUuid);
    invalidateAll();
}

auto PermissionManager::getGroup(const std::string_view uuid) const -> std::optional<GroupInfo> {
    return repo_.getGroup(uuid);
}

auto PermissionManager::getGroupByName(const std::string_view name) const -> std::optional<GroupInfo> {
    return repo_.getGroupByName(name);
}

auto PermissionManager::getAllGroups() const -> std::vector<GroupInfo> { return repo_.getAllGroups(); }

// Membership
bool PermissionManager::addPlayerToGroup(const std::string_view playerUuid, const std::string_view groupUuid) {
    if (!repo_.addPlayerToGroup(playerUuid, groupUuid)) return false;
    invalidatePlayer(playerUuid);
    return true;
}

bool PermissionManager::removePlayerFromGroup(const std::string_view playerUuid, const std::string_view groupUuid) {
    if (!repo_.removePlayerFromGroup(playerUuid, groupUuid)) return false;
    invalidatePlayer(playerUuid);
    return true;
}

auto PermissionManager::getPlayerGroups(const std::string_view playerUuid) const -> std::vector<GroupInfo> {
    return repo_.getPlayerGroups(playerUuid);
}

auto PermissionManager::getGroupMembers(const std::string_view groupUuid) const -> std::vector<std::string> {
    return repo_.getGroupMembers(groupUuid);
}

// Cache
void PermissionManager::invalidatePlayer(const std::string_view uuid) {
    std::unique_lock lock(cacheMutex_);
    ++cacheGeneration_;
    cache_.erase(std::string(uuid));
}

void PermissionManager::invalidateAll() {
    std::unique_lock lock(cacheMutex_);
    ++cacheGeneration_;
    cache_.clear();
}

auto PermissionManager::buildToken(const SubjectKind kind, const std::string_view uuid) const -> AccessToken {
    AccessToken token;

    if (kind == SubjectKind::Player) {
        token.add(std::string(uuid), TokenEntryKind::Subject);
        const auto groups = repo_.getPlayerGroups(uuid);
        // First pass: add all direct groups so they are never mislabeled as InheritedGroup
        for (const auto& group : groups) {
            token.add(group.uuid, TokenEntryKind::DirectGroup);
        }
        // Second pass: add ancestor groups (skip index 0 which is the direct group itself)
        for (const auto& group : groups) {
            auto ancestry = repo_.getGroupAncestry(group.uuid);
            for (std::size_t i = 1; i < ancestry.size(); ++i) {
                token.add(ancestry[i].uuid, TokenEntryKind::InheritedGroup);
            }
        }
    } else {
        // Group: the group itself + its ancestry
        const auto ancestry = repo_.getGroupAncestry(uuid);
        for (std::size_t i = 0; i < ancestry.size(); ++i) {
            token.add(ancestry[i].uuid, i == 0 ? TokenEntryKind::Subject : TokenEntryKind::InheritedGroup);
        }
    }

    return token;
}

auto PermissionManager::resolvePermission(const std::string_view playerUuid, const std::string_view node) const
    -> AccessMask {
    const auto token    = buildToken(SubjectKind::Player, playerUuid);
    const auto nodePath = PermissionResolver::buildNodePath(node);
    const auto aclMap   = repo_.getNodeACLBatch(nodePath);

    return PermissionResolver::resolve(node, token, [&aclMap](const std::string_view n) -> std::vector<ACE> {
        if (const auto it = aclMap.find(std::string(n)); it != aclMap.end()) return it->second;
        return {};
    });
}

// Private helpers
bool PermissionManager::wouldCreateCycle(const std::string_view groupUuid, const std::string_view parentUuid) const {
    const auto ancestry = repo_.getGroupAncestry(parentUuid);
    return std::ranges::any_of(ancestry, [&](const auto& ancestor) { return ancestor.uuid == groupUuid; });
}

} // namespace BakaPerms::core
