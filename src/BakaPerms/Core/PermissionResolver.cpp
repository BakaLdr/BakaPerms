#include "BakaPerms/Core/PermissionResolver.hpp"

#include "BakaPerms/Utils/Exception/Exceptions.hpp"

namespace BakaPerms::core {

auto PermissionResolver::resolve(
    const std::string_view requestedNode,
    const AccessToken&     token,
    const ACLProvider&     getNodeACL
) -> AccessMask {
    for (const auto nodePath = buildNodePath(requestedNode); const auto& node : nodePath) {
        auto acl = getNodeACL(node);
        if (acl.empty()) continue; // No ACL at this level, go up

        // ACL exists，iterate in order, first matching trustee wins
        for (const auto& ace : acl) {
            if (ace.subjectUuid == "*" || token.contains(ace.subjectUuid)) {
                return ace.mask;
            }
        }

        // ACL exists but no ACE matched the token, implicit deny
        return AccessMask::Deny;
    }

    // No node in hierarchy has any ACL, default deny
    return AccessMask::Deny;
}

auto PermissionResolver::buildNodePath(std::string_view node) -> std::vector<std::string> {
    if (node.empty()) {
        throw utils::exception::InvalidArgumentException("Permission node must not be empty");
    }

    std::vector<std::string> path;
    path.emplace_back(node);

    std::string current{node};
    while (true) {
        const auto pos = current.rfind('.');
        if (pos == std::string::npos) break;
        current = current.substr(0, pos);
        path.push_back(current);
    }

    // Add root wildcard if we haven't already added it
    if (path.back() != "*") {
        path.emplace_back("*");
    }

    return path;
}

auto PermissionResolver::resolveWithTrace(
    const std::string_view requestedNode,
    const AccessToken&     token,
    const ACLProvider&     getNodeACL
) -> PermissionTrace {
    PermissionTrace trace;
    trace.requestedNode = requestedNode;
    trace.token         = token;

    const auto nodePath = buildNodePath(requestedNode);
    trace.nodePath      = nodePath;

    for (const auto& node : nodePath) {
        TraceStep step;
        step.node = node;

        auto acl = getNodeACL(node);
        if (acl.empty()) {
            step.aclFound = false;
            trace.steps.push_back(step);
            continue;
        }

        step.aclFound = true;
        step.acl      = acl;

        for (std::size_t i = 0; i < acl.size(); ++i) {
            if (acl[i].subjectUuid == "*") {
                step.matchedAceIdx    = static_cast<int>(i);
                step.matchedTokenKind = TokenEntryKind::Wildcard;
                trace.steps.push_back(step);
                trace.finalResult = acl[i].mask;
                return trace;
            }
            if (auto* entry = token.find(acl[i].subjectUuid)) {
                step.matchedAceIdx    = static_cast<int>(i);
                step.matchedTokenKind = entry->kind;
                trace.steps.push_back(step);
                trace.finalResult = acl[i].mask;
                return trace;
            }
        }

        // ACL exists but no ACE matched → implicit deny
        trace.steps.push_back(step);
        trace.finalResult = AccessMask::Deny;
        return trace;
    }

    // No ACL anywhere → default deny
    trace.finalResult = AccessMask::Deny;
    return trace;
}

} // namespace BakaPerms::core
