#pragma once
#include "BakaPerms/Core/Types.hpp"

#include <ll/api/service/Service.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace BakaPerms::core {

class IPermissionManager : public ll::service::ServiceImpl<IPermissionManager, 1> {
public:
    ~IPermissionManager() override = default;

    // Permission checking
    virtual auto checkPermission(std::string_view playerUuid, std::string_view node) -> AccessMask = 0;
    virtual auto tracePermission(SubjectKind kind, std::string_view uuid, std::string_view node) const
        -> PermissionTrace = 0;

    // ACL management
    virtual void appendACE(std::string_view node, std::string_view subjectUuid, int subjectType, AccessMask mask) = 0;
    virtual void
    insertACE(std::string_view node, int position, std::string_view subjectUuid, int subjectType, AccessMask mask) = 0;
    virtual void removeACE(std::string_view node, int position)                                                    = 0;
    virtual void moveACE(std::string_view node, int from, int to)                                                  = 0;
    virtual auto getNodeACL(std::string_view node) const -> std::vector<ACE>                                       = 0;
    virtual void clearNodeACL(std::string_view node)                                                               = 0;

    // Group management
    virtual auto createGroup(std::string_view name, const std::optional<std::string_view>& parentUuid) const
        -> std::string                                                                                         = 0;
    virtual void deleteGroup(std::string_view groupUuid)                                                       = 0;
    virtual void setGroupParent(std::string_view groupUuid, const std::optional<std::string_view>& parentUuid) = 0;
    virtual auto getGroup(std::string_view uuid) const -> std::optional<GroupInfo>                             = 0;
    virtual auto getGroupByName(std::string_view name) const -> std::optional<GroupInfo>                       = 0;
    virtual auto getAllGroups() const -> std::vector<GroupInfo>                                                = 0;

    // Membership
    [[nodiscard]] virtual bool addPlayerToGroup(std::string_view playerUuid, std::string_view groupUuid)      = 0;
    [[nodiscard]] virtual bool removePlayerFromGroup(std::string_view playerUuid, std::string_view groupUuid) = 0;
    virtual auto               getPlayerGroups(std::string_view playerUuid) const -> std::vector<GroupInfo>   = 0;
    virtual auto               getGroupMembers(std::string_view groupUuid) const -> std::vector<std::string>  = 0;
};

} // namespace BakaPerms::core
