#include "BakaPerms/Commands/PermsCommand.hpp"

#include "BakaPerms/BakaPerms.hpp"
#include "BakaPerms/Core/Types.hpp"
#include "BakaPerms/Utils/I18n/I18n.hpp"

#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/Optional.h>
#include <ll/api/service/PlayerInfo.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <format>
#include <string>
#include <unordered_map>

using namespace ll::i18n_literals;

namespace BakaPerms::commands {

// Command parameter structs
struct ReloadParams {};

struct GroupCreateParams {
    std::string name;
};

struct GroupNameParams {
    std::string name;
};

struct GroupSetParentParams {
    std::string name;
    std::string parentName;
};

struct GroupCheckParams {
    std::string                 name;
    std::string                 node;
    ll::command::Optional<bool> trace;
};

struct UserGroupParams {
    std::string playerName;
    std::string groupName;
};

struct UserCheckParams {
    std::string                 playerName;
    std::string                 node;
    ll::command::Optional<bool> trace;
};

struct UserInfoParams {
    std::string playerName;
};

struct AclAddParams {
    std::string node;
    std::string subjectName;
    enum { player, group } subjectType{};
    enum { allow, deny } access{};
};

struct AclInsertParams {
    std::string node;
    int         position{};
    std::string subjectName;
    enum { player, group } subjectType{};
    enum { allow, deny } access{};
};

struct AclRemoveParams {
    std::string node;
    int         position{};
};

struct AclMoveParams {
    std::string node;
    int         from{};
    int         to{};
};

struct AclInfoParams {
    std::string node;
};

struct AclClearParams {
    std::string node;
};

// Helpers
static auto resolvePlayerUuid(const std::string& playerName, CommandOutput& output) -> std::string {
    const auto info = ll::service::PlayerInfo::getInstance().fromName(playerName);
    if (!info) {
        output.error("bakaperms.error.player_not_found"_tr(playerName));
        return {};
    }
    return info->uuid.asString();
}

static auto accessMaskToString(const core::AccessMask mask) -> std::string {
    switch (mask) {
    case core::AccessMask::Allow:
        return "bakaperms.label.allow"_tr();
    case core::AccessMask::Deny:
        return "bakaperms.label.deny"_tr();
    }
    return "bakaperms.label.unknown"_tr();
}

static auto resolveSubjectUuid(const std::string& subjectName, const bool isGroup, CommandOutput& output)
    -> std::string {
    if (subjectName == "*") return "*";
    if (isGroup) {
        const auto& mgr   = BakaPerms::getInstance().getPermissionManager();
        auto        group = mgr.getGroupByName(subjectName);
        if (!group) {
            output.error("bakaperms.error.group_not_found"_tr(subjectName));
            return {};
        }
        return group->uuid;
    }
    return resolvePlayerUuid(subjectName, output);
}

static auto resolveUuidLabel(const std::string& uuid, const core::IPermissionManager& mgr) -> std::string {
    if (uuid == "*") return "bakaperms.label.everyone"_tr();
    if (auto group = mgr.getGroup(uuid)) {
        return std::format("{}:{}", "bakaperms.label.group"_tr(), group->name);
    }
    const auto info = ll::service::PlayerInfo::getInstance().fromUuid(mce::UUID(uuid));
    return info ? std::format("{}:{}", "bakaperms.label.player"_tr(), info->name) : uuid;
}

static auto tokenEntryKindToString(const core::TokenEntryKind kind) -> std::string {
    switch (kind) {
    case core::TokenEntryKind::Subject:
        return "bakaperms.label.subject"_tr();
    case core::TokenEntryKind::DirectGroup:
        return "bakaperms.label.direct_group"_tr();
    case core::TokenEntryKind::InheritedGroup:
        return "bakaperms.label.inherited_group"_tr();
    case core::TokenEntryKind::Wildcard:
        return "bakaperms.label.wildcard"_tr();
    }
    return "bakaperms.label.unknown"_tr();
}

static auto formatTrace(const core::PermissionTrace& trace, core::IPermissionManager& mgr) -> std::string {
    // Resolve subject display name
    std::string kindStr =
        trace.subjectKind == core::SubjectKind::Player ? "bakaperms.label.player"_tr() : "bakaperms.label.group"_tr();
    std::string subjectName;
    if (trace.subjectKind == core::SubjectKind::Player) {
        auto info   = ll::service::PlayerInfo::getInstance().fromUuid(mce::UUID(trace.subjectUuid));
        subjectName = info ? info->name : trace.subjectUuid;
    } else {
        auto group  = mgr.getGroup(trace.subjectUuid);
        subjectName = group ? group->name : trace.subjectUuid;
    }

    std::string msg = "bakaperms.trace.header"_tr(trace.requestedNode, kindStr, subjectName);

    // Token summary
    msg += std::format("\n    {}", "bakaperms.trace.token_header"_tr(trace.token.size()));
    for (const auto& [uuid, kind] : trace.token.entries()) {
        msg += std::format("\n        [{}] {}", tokenEntryKindToString(kind), resolveUuidLabel(uuid, mgr));
    }

    // Walk through each resolution step
    bool anyAclFound = false;
    bool matched     = false;

    for (const auto& [node, aclFound, acl, matchedAceIdx, matchedTokenKind] : trace.steps) {
        msg += std::format("\n    {}", "bakaperms.trace.searching"_tr(node));

        if (!aclFound) {
            msg += std::format("\n        {}", "bakaperms.trace.no_acl"_tr(node));
            continue;
        }

        anyAclFound  = true;
        msg         += std::format("\n        {}", "bakaperms.trace.found_node"_tr(node));

        for (int i = 0; i < static_cast<int>(acl.size()); ++i) {
            const auto& ace      = acl[i];
            std::string aceLabel = resolveUuidLabel(ace.subjectUuid, mgr);
            std::string maskStr  = accessMaskToString(ace.mask);

            if (i == matchedAceIdx) {
                msg += std::format(
                    "\n            {}",
                    "bakaperms.trace.ace_matched"_tr(
                        node,
                        ace.orderIndex,
                        maskStr,
                        aceLabel,
                        tokenEntryKindToString(matchedTokenKind)
                    )
                );
                matched = true;
            } else {
                msg += std::format(
                    "\n            {}",
                    "bakaperms.trace.ace_skipped"_tr(node, ace.orderIndex, maskStr, aceLabel)
                );
            }
        }

        if (matchedAceIdx >= 0) {
            const auto& matchedAce  = acl[matchedAceIdx];
            msg                    += std::format(
                "\n        {}",
                "bakaperms.trace.match_detail"_tr(
                    matchedAce.orderIndex,
                    resolveUuidLabel(matchedAce.subjectUuid, mgr),
                    tokenEntryKindToString(matchedTokenKind)
                )
            );
            msg += std::format("\n    {}", "bakaperms.trace.resolved"_tr(node, accessMaskToString(matchedAce.mask)));
        } else {
            msg += std::format("\n        {}", "bakaperms.trace.no_match"_tr(kindStr, subjectName));
            msg += std::format("\n    {}", "bakaperms.trace.implicit_deny"_tr(node));
        }
    }

    // Final summary
    if (!matched && !anyAclFound) {
        msg += std::format("\n{}", "bakaperms.trace.no_acl_for_subject"_tr(kindStr, subjectName));
    }

    std::string resultStr = accessMaskToString(trace.finalResult);
    if (!matched) {
        resultStr += anyAclFound ? "bakaperms.trace.suffix_implicit"_tr() : "bakaperms.trace.suffix_default"_tr();
    }

    msg += std::format("\n{}", "bakaperms.trace.complete"_tr(kindStr, subjectName, trace.requestedNode, resultStr));

    return msg;
}

// Registration
void registerCommands() {
    auto& command = ll::command::CommandRegistrar::getInstance(false)
                        .getOrCreateCommand("perms", "bakaperms.command.description", CommandPermissionLevel::Admin);

    // /perms reload
    command.overload<ReloadParams>().text("reload").execute([](CommandOrigin const&, CommandOutput& output) {
        auto& mgr = BakaPerms::getInstance().getPermissionManager();
        mgr.invalidateAll();
        output.success("bakaperms.reload.success"_tr());
    });

    // Group management
    // /perms group create <name>
    command.overload<GroupCreateParams>()
        .text("group")
        .text("create")
        .required("name")
        .execute([](CommandOrigin const&, CommandOutput& output, const GroupCreateParams& params) {
            const auto& mgr = BakaPerms::getInstance().getPermissionManager();
            try {
                auto uuid = mgr.createGroup(params.name, std::nullopt);
                output.success("bakaperms.group.created"_tr(params.name, uuid));
            } catch (const std::exception& e) {
                output.error("bakaperms.error.create_group_failed"_tr(e.what()));
            }
        });

    // /perms group delete <name>
    command.overload<GroupNameParams>().text("group").text("delete").required("name").execute(
        [](CommandOrigin const&, CommandOutput& output, const GroupNameParams& params) {
            auto&      mgr   = BakaPerms::getInstance().getPermissionManager();
            const auto group = mgr.getGroupByName(params.name);
            if (!group) {
                output.error("bakaperms.error.group_not_found"_tr(params.name));
                return;
            }
            mgr.deleteGroup(group->uuid);
            output.success("bakaperms.group.deleted"_tr(params.name));
        }
    );

    // /perms group setparent <name> <parentName|none>
    command.overload<GroupSetParentParams>()
        .text("group")
        .text("setparent")
        .required("name")
        .required("parentName")
        .execute([](CommandOrigin const&, CommandOutput& output, const GroupSetParentParams& params) {
            auto&      mgr   = BakaPerms::getInstance().getPermissionManager();
            const auto group = mgr.getGroupByName(params.name);
            if (!group) {
                output.error("bakaperms.error.group_not_found"_tr(params.name));
                return;
            }
            try {
                if (params.parentName == "none") {
                    mgr.setGroupParent(group->uuid, std::nullopt);
                    output.success("bakaperms.group.parent_cleared"_tr(params.name));
                } else {
                    auto parent = mgr.getGroupByName(params.parentName);
                    if (!parent) {
                        output.error("bakaperms.error.parent_group_not_found"_tr(params.parentName));
                        return;
                    }
                    mgr.setGroupParent(group->uuid, parent->uuid);
                    output.success("bakaperms.group.parent_set"_tr(params.name, params.parentName));
                }
            } catch (const std::exception& e) {
                output.error("bakaperms.error.operation_failed"_tr(e.what()));
            }
        });

    // /perms group list
    command.overload().text("group").text("list").execute([](CommandOrigin const&, CommandOutput& output) {
        const auto& mgr    = BakaPerms::getInstance().getPermissionManager();
        const auto  groups = mgr.getAllGroups();
        if (groups.empty()) {
            output.success("bakaperms.group.list_empty"_tr());
            return;
        }
        // Build UUID→name map for parent lookups without additional DB queries
        std::unordered_map<std::string, std::string> groupNames;
        for (const auto& g : groups) {
            groupNames[g.uuid] = g.name;
        }
        std::string msg = "bakaperms.group.list_header"_tr(groups.size());
        for (const auto& [uuid, name, parentUuid] : groups) {
            msg += std::format("\n  {}", "bakaperms.group.list_entry"_tr(name, uuid));
            if (parentUuid) {
                if (const auto it = groupNames.find(*parentUuid); it != groupNames.end())
                    msg += std::format(" -> {}", "bakaperms.group.info_parent"_tr(it->second));
            }
        }
        output.success(msg);
    });

    // /perms group info <name>
    command.overload<GroupNameParams>().text("group").text("info").required("name").execute(
        [](CommandOrigin const&, CommandOutput& output, const GroupNameParams& params) {
            auto& mgr   = BakaPerms::getInstance().getPermissionManager();
            auto  group = mgr.getGroupByName(params.name);
            if (!group) {
                output.error("bakaperms.error.group_not_found"_tr(params.name));
                return;
            }
            std::string msg = "bakaperms.group.info_header"_tr(group->name, group->uuid);
            if (group->parentUuid) {
                if (auto parent = mgr.getGroup(*group->parentUuid))
                    msg += std::format("\n{}", "bakaperms.group.info_parent"_tr(parent->name));
            }
            if (auto aces = mgr.getSubjectACEs(group->uuid); !aces.empty()) {
                msg += std::format("\n{}", "bakaperms.group.info_aces"_tr());
                for (const auto& [node, ace] : aces) {
                    msg += std::format("\n  [{}] #{} {}", node, ace.orderIndex, accessMaskToString(ace.mask));
                }
            }
            if (auto members = mgr.getGroupMembers(group->uuid); !members.empty()) {
                msg += std::format("\n{}", "bakaperms.group.info_members"_tr(members.size()));
                for (const auto& m : members) {
                    auto info  = ll::service::PlayerInfo::getInstance().fromUuid(mce::UUID(m));
                    msg       += std::format("\n  {}", info ? info->name : m);
                }
            }
            output.success(msg);
        }
    );

    // /perms group check <name> <node> [trace]
    command.overload<GroupCheckParams>()
        .text("group")
        .text("check")
        .required("name")
        .required("node")
        .optional("trace")
        .execute([](CommandOrigin const&, CommandOutput& output, const GroupCheckParams& params) {
            auto&      mgr   = BakaPerms::getInstance().getPermissionManager();
            const auto group = mgr.getGroupByName(params.name);
            if (!group) {
                output.error("bakaperms.error.group_not_found"_tr(params.name));
                return;
            }
            const auto trace = mgr.tracePermission(core::SubjectKind::Group, group->uuid, params.node);
            if (params.trace.has_value() && params.trace.value()) {
                output.success(formatTrace(trace, mgr));
            } else {
                output.success(
                    "bakaperms.check.result"_tr(
                        params.node,
                        "bakaperms.label.group"_tr(),
                        params.name,
                        accessMaskToString(trace.finalResult)
                    )
                );
            }
        });

    // User management
    // /perms user addgroup <player> <group>
    command.overload<UserGroupParams>()
        .text("user")
        .text("addgroup")
        .required("playerName")
        .required("groupName")
        .execute([](CommandOrigin const&, CommandOutput& output, const UserGroupParams& params) {
            const auto playerUuid = resolvePlayerUuid(params.playerName, output);
            if (playerUuid.empty()) return;
            auto&      mgr   = BakaPerms::getInstance().getPermissionManager();
            const auto group = mgr.getGroupByName(params.groupName);
            if (!group) {
                output.error("bakaperms.error.group_not_found"_tr(params.groupName));
                return;
            }
            if (mgr.addPlayerToGroup(playerUuid, group->uuid)) {
                output.success("bakaperms.user.added_to_group"_tr(params.playerName, params.groupName));
            } else {
                output.error("bakaperms.user.already_in_group"_tr(params.playerName, params.groupName));
            }
        });

    // /perms user removegroup <player> <group>
    command.overload<UserGroupParams>()
        .text("user")
        .text("removegroup")
        .required("playerName")
        .required("groupName")
        .execute([](CommandOrigin const&, CommandOutput& output, const UserGroupParams& params) {
            const auto playerUuid = resolvePlayerUuid(params.playerName, output);
            if (playerUuid.empty()) return;
            auto&      mgr   = BakaPerms::getInstance().getPermissionManager();
            const auto group = mgr.getGroupByName(params.groupName);
            if (!group) {
                output.error("bakaperms.error.group_not_found"_tr(params.groupName));
                return;
            }
            if (mgr.removePlayerFromGroup(playerUuid, group->uuid)) {
                output.success("bakaperms.user.removed_from_group"_tr(params.playerName, params.groupName));
            } else {
                output.error("bakaperms.user.not_in_group"_tr(params.playerName, params.groupName));
            }
        });

    // /perms user check <player> <node> [trace]
    command.overload<UserCheckParams>()
        .text("user")
        .text("check")
        .required("playerName")
        .required("node")
        .optional("trace")
        .execute([](CommandOrigin const&, CommandOutput& output, const UserCheckParams& params) {
            const auto playerUuid = resolvePlayerUuid(params.playerName, output);
            if (playerUuid.empty()) return;
            auto& mgr = BakaPerms::getInstance().getPermissionManager();
            if (params.trace.has_value() && params.trace.value()) {
                const auto trace = mgr.tracePermission(core::SubjectKind::Player, playerUuid, params.node);
                output.success(formatTrace(trace, mgr));
            } else {
                const auto result = mgr.checkPermission(playerUuid, params.node);
                output.success(
                    "bakaperms.check.result"_tr(
                        params.node,
                        "bakaperms.label.player"_tr(),
                        params.playerName,
                        accessMaskToString(result)
                    )
                );
            }
        });

    // /perms user info <player>
    command.overload<UserInfoParams>()
        .text("user")
        .text("info")
        .required("playerName")
        .execute([](CommandOrigin const&, CommandOutput& output, const UserInfoParams& params) {
            auto playerUuid = resolvePlayerUuid(params.playerName, output);
            if (playerUuid.empty()) return;
            const auto& mgr = BakaPerms::getInstance().getPermissionManager();
            std::string msg = "bakaperms.user.info_header"_tr(params.playerName, playerUuid);

            if (const auto groups = mgr.getPlayerGroups(playerUuid); !groups.empty()) {
                msg += std::format("\n{}", "bakaperms.user.info_groups"_tr(groups.size()));
                for (const auto& g : groups) {
                    msg += std::format("\n  {}", "bakaperms.user.info_group_entry"_tr(g.name));
                }
            } else {
                msg += std::format("\n{}", "bakaperms.user.info_no_groups"_tr());
            }

            if (auto aces = mgr.getSubjectACEs(playerUuid); !aces.empty()) {
                msg += std::format("\n{}", "bakaperms.user.info_aces"_tr());
                for (const auto& [node, ace] : aces) {
                    msg += std::format("\n  [{}] #{} {}", node, ace.orderIndex, accessMaskToString(ace.mask));
                }
            } else {
                msg += std::format("\n{}", "bakaperms.user.info_no_aces"_tr());
            }

            output.success(msg);
        });

    // ACL management
    // /perms acl add <node> <subjectName> <player|group> <allow|deny>
    command.overload<AclAddParams>()
        .text("acl")
        .text("add")
        .required("node")
        .required("subjectName")
        .required("subjectType")
        .required("access")
        .execute([](CommandOrigin const&, CommandOutput& output, const AclAddParams& params) {
            try {
                const bool isGroup     = params.subjectType == AclAddParams::group;
                const auto subjectUuid = resolveSubjectUuid(params.subjectName, isGroup, output);
                if (subjectUuid.empty()) return;

                auto&      mgr = BakaPerms::getInstance().getPermissionManager();
                const auto mask =
                    params.access == AclAddParams::allow ? core::AccessMask::Allow : core::AccessMask::Deny;
                const int type = isGroup ? 1 : 0;
                mgr.appendACE(params.node, subjectUuid, type, mask);
                output.success(
                    "bakaperms.acl.appended"_tr(
                        params.node,
                        isGroup ? "bakaperms.label.group"_tr() : "bakaperms.label.player"_tr(),
                        params.subjectName,
                        accessMaskToString(mask)
                    )
                );
            } catch (const std::exception& e) {
                output.error("bakaperms.error.operation_failed"_tr(e.what()));
            }
        });

    // /perms acl insert <node> <position> <subjectName> <player|group> <allow|deny>
    command.overload<AclInsertParams>()
        .text("acl")
        .text("insert")
        .required("node")
        .required("position")
        .required("subjectName")
        .required("subjectType")
        .required("access")
        .execute([](CommandOrigin const&, CommandOutput& output, const AclInsertParams& params) {
            try {
                const bool isGroup     = params.subjectType == AclInsertParams::group;
                const auto subjectUuid = resolveSubjectUuid(params.subjectName, isGroup, output);
                if (subjectUuid.empty()) return;

                auto&      mgr = BakaPerms::getInstance().getPermissionManager();
                const auto mask =
                    params.access == AclInsertParams::allow ? core::AccessMask::Allow : core::AccessMask::Deny;
                const int type = isGroup ? 1 : 0;
                mgr.insertACE(params.node, params.position, subjectUuid, type, mask);
                output.success(
                    "bakaperms.acl.inserted"_tr(
                        params.position,
                        params.node,
                        isGroup ? "bakaperms.label.group"_tr() : "bakaperms.label.player"_tr(),
                        params.subjectName,
                        accessMaskToString(mask)
                    )
                );
            } catch (const std::exception& e) {
                output.error("bakaperms.error.operation_failed"_tr(e.what()));
            }
        });

    // /perms acl remove <node> <position>
    command.overload<AclRemoveParams>()
        .text("acl")
        .text("remove")
        .required("node")
        .required("position")
        .execute([](CommandOrigin const&, CommandOutput& output, const AclRemoveParams& params) {
            try {
                auto& mgr = BakaPerms::getInstance().getPermissionManager();
                mgr.removeACE(params.node, params.position);
                output.success("bakaperms.acl.removed"_tr(params.position, params.node));
            } catch (const std::exception& e) {
                output.error("bakaperms.error.operation_failed"_tr(e.what()));
            }
        });

    // /perms acl move <node> <from> <to>
    command.overload<AclMoveParams>().text("acl").text("move").required("node").required("from").required("to").execute(
        [](CommandOrigin const&, CommandOutput& output, const AclMoveParams& params) {
            try {
                auto& mgr = BakaPerms::getInstance().getPermissionManager();
                mgr.moveACE(params.node, params.from, params.to);
                output.success("bakaperms.acl.moved"_tr(params.from, params.to, params.node));
            } catch (const std::exception& e) {
                output.error("bakaperms.error.operation_failed"_tr(e.what()));
            }
        }
    );

    // /perms acl info <node>
    command.overload<AclInfoParams>().text("acl").text("info").required("node").execute(
        [](CommandOrigin const&, CommandOutput& output, const AclInfoParams& params) {
            const auto& mgr = BakaPerms::getInstance().getPermissionManager();
            const auto  acl = mgr.getNodeACL(params.node);
            if (acl.empty()) {
                output.success("bakaperms.acl.empty"_tr(params.node));
                return;
            }
            std::string msg = "bakaperms.acl.info_header"_tr(params.node, acl.size());
            for (const auto& [orderIndex, subjectUuid, subjectType, mask] : acl) {
                std::string subjectLabel;
                if (subjectUuid == "*") {
                    subjectLabel = "bakaperms.label.everyone"_tr();
                } else if (subjectType == 1) {
                    const auto group = mgr.getGroup(subjectUuid);
                    subjectLabel =
                        std::format("{}:{}", "bakaperms.label.group"_tr(), group ? group->name : subjectUuid);
                } else {
                    const auto info = ll::service::PlayerInfo::getInstance().fromUuid(mce::UUID(subjectUuid));
                    subjectLabel = std::format("{}:{}", "bakaperms.label.player"_tr(), info ? info->name : subjectUuid);
                }
                msg += std::format("\n  #{} {} {}", orderIndex, subjectLabel, accessMaskToString(mask));
            }
            output.success(msg);
        }
    );

    // /perms acl clear <node>
    command.overload<AclClearParams>().text("acl").text("clear").required("node").execute(
        [](CommandOrigin const&, CommandOutput& output, const AclClearParams& params) {
            auto& mgr = BakaPerms::getInstance().getPermissionManager();
            mgr.clearNodeACL(params.node);
            output.success("bakaperms.acl.cleared"_tr(params.node));
        }
    );
}

} // namespace BakaPerms::commands
