#include "BakaPerms/Data/PermissionRepository.hpp"

#include "BakaPerms/Database/DbTypes.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <unordered_set>

namespace BakaPerms::data {

static auto toAccessMask(const int value) -> core::AccessMask {
    if (value != static_cast<int>(core::AccessMask::Deny) && value != static_cast<int>(core::AccessMask::Allow)) {
        throw std::out_of_range(std::format("Invalid access_mask value in database: {}", value));
    }
    return static_cast<core::AccessMask>(value);
}

PermissionRepository::PermissionRepository(database::IDatabase& db) : db_(db) {}

void PermissionRepository::initializeSchema() const {
    db_.exec(R"(
        CREATE TABLE IF NOT EXISTS groups (
            uuid         TEXT PRIMARY KEY,
            name         TEXT NOT NULL UNIQUE,
            parent_uuid  TEXT DEFAULT NULL,
            created_at   TEXT NOT NULL DEFAULT (datetime('now')),
            FOREIGN KEY (parent_uuid) REFERENCES groups(uuid) ON DELETE SET NULL
        )
    )");

    db_.exec(R"(
        CREATE TABLE IF NOT EXISTS player_groups (
            player_uuid  TEXT NOT NULL,
            group_uuid   TEXT NOT NULL,
            added_at     TEXT NOT NULL DEFAULT (datetime('now')),
            PRIMARY KEY (player_uuid, group_uuid),
            FOREIGN KEY (group_uuid) REFERENCES groups(uuid) ON DELETE CASCADE
        )
    )");

    db_.exec(R"(
        CREATE TABLE IF NOT EXISTS permissions (
            node          TEXT NOT NULL,
            order_index   INTEGER NOT NULL,
            subject_uuid  TEXT NOT NULL,
            subject_type  INTEGER NOT NULL,
            access_mask   INTEGER NOT NULL,
            created_at    TEXT NOT NULL DEFAULT (datetime('now')),
            PRIMARY KEY (node, order_index)
        )
    )");

    db_.exec("CREATE INDEX IF NOT EXISTS idx_permissions_node ON permissions(node)");
    db_.exec("CREATE INDEX IF NOT EXISTS idx_permissions_subject ON permissions(subject_uuid)");
    db_.exec("CREATE INDEX IF NOT EXISTS idx_player_groups_player ON player_groups(player_uuid)");
}

// Groups
void PermissionRepository::createGroup(
    const std::string_view                 uuid,
    const std::string_view                 name,
    const std::optional<std::string_view>& parentUuid
) const {
    database::ParamList params = {std::string(uuid), std::string(name)};
    if (parentUuid) {
        params.emplace_back(std::string(*parentUuid));
    } else {
        params.emplace_back(database::DbNull{});
    }
    db_.execute("INSERT INTO groups (uuid, name, parent_uuid) VALUES (?, ?, ?)", params);
}

void PermissionRepository::deleteGroup(const std::string_view uuid) const {
    db_.withTransaction([&] {
        // Collect affected nodes before deletion so we can reindex them
        const auto affectedRows =
            db_.query("SELECT DISTINCT node FROM permissions WHERE subject_uuid = ?", {std::string(uuid)});
        db_.execute("DELETE FROM permissions WHERE subject_uuid = ?", {std::string(uuid)});
        db_.execute("DELETE FROM groups WHERE uuid = ?", {std::string(uuid)});
        // Reindex affected nodes to close gaps in order_index
        for (const auto& row : affectedRows) {
            reindexACL(row.getString(0));
        }
    });
}

void PermissionRepository::setGroupParent(
    const std::string_view                 uuid,
    const std::optional<std::string_view>& parentUuid
) const {
    database::ParamList params;
    if (parentUuid) {
        params.emplace_back(std::string(*parentUuid));
    } else {
        params.emplace_back(database::DbNull{});
    }
    params.emplace_back(std::string(uuid));
    db_.execute("UPDATE groups SET parent_uuid = ? WHERE uuid = ?", params);
}

static auto rowToGroupInfo(const database::Row& row) -> core::GroupInfo {
    core::GroupInfo info;
    info.uuid       = row.getString(0);
    info.name       = row.getString(1);
    info.parentUuid = row.isNull(2) ? std::nullopt : std::optional(row.getString(2));
    return info;
}

auto PermissionRepository::getGroup(const std::string_view uuid) const -> std::optional<core::GroupInfo> {
    const auto row =
        db_.queryOne("SELECT uuid, name, parent_uuid FROM groups WHERE uuid = ?", {std::string(uuid)});
    if (!row) return std::nullopt;
    return rowToGroupInfo(*row);
}

auto PermissionRepository::getGroupByName(const std::string_view name) const -> std::optional<core::GroupInfo> {
    const auto row =
        db_.queryOne("SELECT uuid, name, parent_uuid FROM groups WHERE name = ?", {std::string(name)});
    if (!row) return std::nullopt;
    return rowToGroupInfo(*row);
}

auto PermissionRepository::getAllGroups() const -> std::vector<core::GroupInfo> {
    const auto rows = db_.query("SELECT uuid, name, parent_uuid FROM groups ORDER BY name");
    std::vector<core::GroupInfo> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(rowToGroupInfo(row));
    }
    return result;
}

// Membership
bool PermissionRepository::addPlayerToGroup(const std::string_view playerUuid, const std::string_view groupUuid) const {
    return db_.execute(
               "INSERT OR IGNORE INTO player_groups (player_uuid, group_uuid) VALUES (?, ?)",
               {std::string(playerUuid), std::string(groupUuid)}
           )
        > 0;
}

bool PermissionRepository::removePlayerFromGroup(
    const std::string_view playerUuid,
    const std::string_view groupUuid
) const {
    return db_.execute(
               "DELETE FROM player_groups WHERE player_uuid = ? AND group_uuid = ?",
               {std::string(playerUuid), std::string(groupUuid)}
           )
        > 0;
}

auto PermissionRepository::getPlayerGroups(const std::string_view playerUuid) const -> std::vector<core::GroupInfo> {
    const auto rows = db_.query(
        "SELECT g.uuid, g.name, g.parent_uuid "
        "FROM player_groups pg JOIN groups g ON pg.group_uuid = g.uuid "
        "WHERE pg.player_uuid = ? ORDER BY g.name",
        {std::string(playerUuid)}
    );
    std::vector<core::GroupInfo> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(rowToGroupInfo(row));
    }
    return result;
}

auto PermissionRepository::getGroupMembers(const std::string_view groupUuid) const -> std::vector<std::string> {
    const auto rows = db_.query("SELECT player_uuid FROM player_groups WHERE group_uuid = ?", {std::string(groupUuid)});
    std::vector<std::string> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(row.getString(0));
    }
    return result;
}

// Ancestry
auto PermissionRepository::getGroupAncestry(const std::string_view groupUuid) const -> std::vector<core::GroupInfo> {
    const auto rows = db_.query(
        R"(
            WITH RECURSIVE ancestry(uuid, name, parent_uuid, depth) AS (
                SELECT uuid, name, parent_uuid, 0
                FROM groups WHERE uuid = ?
                UNION ALL
                SELECT g.uuid, g.name, g.parent_uuid, a.depth + 1
                FROM groups g
                JOIN ancestry a ON g.uuid = a.parent_uuid
                WHERE a.depth < 32
            )
            SELECT uuid, name, parent_uuid FROM ancestry ORDER BY depth
        )",
        {std::string(groupUuid)}
    );

    std::vector<core::GroupInfo>    chain;
    std::unordered_set<std::string> visited;
    for (const auto& row : rows) {
        auto info = rowToGroupInfo(row);
        if (visited.contains(info.uuid)) break; // cycle detection
        visited.insert(info.uuid);
        chain.push_back(std::move(info));
    }
    return chain;
}

// ACL operations
static auto rowToACE(const database::Row& row) -> core::ACE {
    return {
        .orderIndex  = row.getInt(0),
        .subjectUuid = row.getString(1),
        .subjectType = row.getInt(2),
        .mask        = toAccessMask(row.getInt(3)),
    };
}

void PermissionRepository::appendACE(
    const std::string_view node,
    const std::string_view subjectUuid,
    int                    subjectType,
    core::AccessMask       mask
) const {
    db_.withTransaction([&] {
        const auto row = db_.queryOne(
            "SELECT COALESCE(MAX(order_index), -1) + 1 FROM permissions WHERE node = ?",
            {std::string(node)}
        );
        int nextIndex = row ? row->getInt(0) : 0;
        db_.execute(
            "INSERT INTO permissions (node, order_index, subject_uuid, subject_type, access_mask) VALUES (?, ?, ?, ?, "
            "?)",
            {std::string(node), nextIndex, std::string(subjectUuid), subjectType, static_cast<int>(mask)}
        );
    });
}

void PermissionRepository::insertACE(
    const std::string_view node,
    int                    position,
    const std::string_view subjectUuid,
    int                    subjectType,
    core::AccessMask       mask
) const {
    db_.withTransaction([&] {
        // Validate position bounds
        const auto countRow = db_.queryOne("SELECT COUNT(*) FROM permissions WHERE node = ?", {std::string(node)});
        if (const int count = countRow ? countRow->getInt(0) : 0; position < 0 || position > count) {
            throw std::out_of_range(std::format("ACE position {} is out of range [0, {}]", position, count));
        }
        // Shift existing ACEs using negative offset to avoid PRIMARY KEY conflicts.
        // -(order_index + 1) produces unique negative values; then -order_index restores them shifted +1.
        db_.execute(
            "UPDATE permissions SET order_index = -(order_index + 1) WHERE node = ? AND order_index >= ?",
            {std::string(node), position}
        );
        db_.execute(
            "UPDATE permissions SET order_index = -order_index WHERE node = ? AND order_index < 0",
            {std::string(node)}
        );
        // Insert new ACE at position
        db_.execute(
            "INSERT INTO permissions (node, order_index, subject_uuid, subject_type, access_mask) VALUES (?, ?, ?, ?, "
            "?)",
            {std::string(node), position, std::string(subjectUuid), subjectType, static_cast<int>(mask)}
        );
    });
}

void PermissionRepository::removeACE(std::string_view node, int orderIndex) const {
    db_.withTransaction([&] {
        const int affected =
            db_.execute("DELETE FROM permissions WHERE node = ? AND order_index = ?", {std::string(node), orderIndex});
        if (affected == 0) {
            throw std::out_of_range(std::format("No ACE found at position {} on node '{}'", orderIndex, node));
        }
        // Shift entries after the removed one down by 1.
        // Use negative offset to avoid PRIMARY KEY conflicts regardless of row processing order.
        db_.execute(
            "UPDATE permissions SET order_index = -order_index WHERE node = ? AND order_index > ?",
            {std::string(node), orderIndex}
        );
        db_.execute(
            "UPDATE permissions SET order_index = (-order_index) - 1 WHERE node = ? AND order_index < 0",
            {std::string(node)}
        );
    });
}

void PermissionRepository::moveACE(const std::string_view node, const int fromIndex, const int toIndex) const {
    if (fromIndex == toIndex) return;

    db_.withTransaction([&] {
        auto acl = getNodeACL(node);
        if (fromIndex < 0 || fromIndex >= static_cast<int>(acl.size()) || toIndex < 0
            || toIndex >= static_cast<int>(acl.size())) {
            throw std::out_of_range(
                std::format("ACE move position out of range: from={}, to={}, size={}", fromIndex, toIndex, acl.size())
            );
        }

        const auto ace = acl[static_cast<size_t>(fromIndex)];
        acl.erase(acl.begin() + fromIndex);
        acl.insert(acl.begin() + toIndex, ace);

        writeACL(node, acl);
    });
}

auto PermissionRepository::getNodeACL(const std::string_view node) const -> std::vector<core::ACE> {
    const auto rows = db_.query(
        "SELECT order_index, subject_uuid, subject_type, access_mask "
        "FROM permissions WHERE node = ? ORDER BY order_index ASC",
        {std::string(node)}
    );
    std::vector<core::ACE> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(rowToACE(row));
    }
    return result;
}

void PermissionRepository::clearNodeACL(const std::string_view node) const {
    db_.execute("DELETE FROM permissions WHERE node = ?", {std::string(node)});
}

auto PermissionRepository::getSubjectACEs(const std::string_view subjectUuid) const -> std::vector<NodeACE> {
    const auto rows = db_.query(
        "SELECT node, order_index, subject_uuid, subject_type, access_mask "
        "FROM permissions WHERE subject_uuid = ? ORDER BY node, order_index",
        {std::string(subjectUuid)}
    );
    std::vector<NodeACE> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back({
            .node = row.getString(0),
            .ace  = {
                     .orderIndex  = row.getInt(1),
                     .subjectUuid = row.getString(2),
                     .subjectType = row.getInt(3),
                     .mask        = toAccessMask(row.getInt(4)),
                     },
        });
    }
    return result;
}

void PermissionRepository::reindexACL(const std::string_view node) const {
    const auto acl = getNodeACL(node);
    writeACL(node, acl);
}

void PermissionRepository::writeACL(const std::string_view node, const std::vector<core::ACE>& acl) const {
    // Move all to negative temporary indices to avoid PRIMARY KEY conflicts during reorder.
    // -(order_index + 1) maps 0→-1, 1→-2, etc. — all unique and negative.
    db_.execute(
        "UPDATE permissions SET order_index = -(order_index + 1) WHERE node = ?",
        {std::string(node)}
    );
    // Update each ACE to its new position, matching by its original (now negated) order_index.
    // This preserves created_at and other columns.
    for (int i = 0; i < static_cast<int>(acl.size()); ++i) {
        db_.execute(
            "UPDATE permissions SET order_index = ? WHERE node = ? AND order_index = ?",
            {i, std::string(node), -(acl[static_cast<size_t>(i)].orderIndex + 1)}
        );
    }
}

auto PermissionRepository::getNodeACLBatch(const std::vector<std::string>& nodes) const
    -> std::unordered_map<std::string, std::vector<core::ACE>> {
    if (nodes.empty()) return {};

    // Build parameterized IN clause
    std::string         sql = "SELECT node, order_index, subject_uuid, subject_type, access_mask "
                              "FROM permissions WHERE node IN (";
    database::ParamList params;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) sql += ", ";
        sql += "?";
        params.emplace_back(nodes[i]);
    }
    sql += ") ORDER BY node, order_index ASC";

    const auto                                              rows = db_.query(sql, params);
    std::unordered_map<std::string, std::vector<core::ACE>> result;
    for (const auto& row : rows) {
        result[row.getString(0)].push_back({
            .orderIndex  = row.getInt(1),
            .subjectUuid = row.getString(2),
            .subjectType = row.getInt(3),
            .mask        = toAccessMask(row.getInt(4)),
        });
    }
    return result;
}

} // namespace BakaPerms::data
