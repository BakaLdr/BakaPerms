#pragma once
#include "BakaPerms/Core/Types.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace BakaPerms::core {

class PermissionResolver {
public:
    using ACLProvider = std::function<std::vector<ACE>(std::string_view)>;

    /// DACL resolution: walk up node hierarchy, find first node with an ACL,
    /// iterate ACEs in order, first matching trustee in token wins.
    /// If ACL exists but no ACE matches token → Deny (implicit deny).
    /// If no node in hierarchy has an ACL → Deny (default deny).
    static auto resolve(std::string_view requestedNode, const AccessToken& token, const ACLProvider& getNodeACL)
        -> AccessMask;

    /// Build the node lookup path: exact node → parent levels → "*" root.
    /// e.g., "baka.perms.test" → ["baka.perms.test", "baka.perms", "baka", "*"]
    static auto buildNodePath(std::string_view node) -> std::vector<std::string>;

    /// Same as resolve(), but returns a detailed trace of each step for debugging.
    static auto
    resolveWithTrace(std::string_view requestedNode, const AccessToken& token, const ACLProvider& getNodeACL)
        -> PermissionTrace;
};

} // namespace BakaPerms::core
