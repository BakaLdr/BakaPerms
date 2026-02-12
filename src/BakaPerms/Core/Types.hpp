#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace BakaPerms::core {

enum class AccessMask : int {
    Deny  = 0,
    Allow = 1,
};

enum class SubjectKind : int {
    Player = 0,
    Group  = 1,
};

enum class TokenEntryKind : int {
    Subject        = 0, // Primary identity (the player or group being checked)
    DirectGroup    = 1, // A group the player directly belongs to
    InheritedGroup = 2, // An ancestor group via parent chain
    Wildcard       = 3, // Matched via wildcard subject (*)
};

struct TokenEntry {
    std::string    uuid;
    TokenEntryKind kind;
};

class AccessToken {
public:
    void add(std::string uuid, const TokenEntryKind kind) {
        if (index_.contains(uuid)) return;
        index_[uuid] = entries_.size();
        entries_.push_back({std::move(uuid), kind});
    }

    [[nodiscard]] bool contains(const std::string_view uuid) const { return index_.contains(std::string(uuid)); }

    [[nodiscard]] auto find(const std::string_view uuid) const -> const TokenEntry* {
        const auto it = index_.find(std::string(uuid));
        if (it == index_.end()) return nullptr;
        return &entries_[it->second];
    }

    [[nodiscard]] auto size() const -> std::size_t { return entries_.size(); }

    [[nodiscard]] auto entries() const -> const std::vector<TokenEntry>& { return entries_; }

private:
    std::vector<TokenEntry>                      entries_;
    std::unordered_map<std::string, std::size_t> index_;
};

struct ACE {
    int         orderIndex;
    std::string subjectUuid;
    int         subjectType; // 0 = player, 1 = group
    AccessMask  mask;
};

struct GroupInfo {
    std::string                uuid;
    std::string                name;
    std::optional<std::string> parentUuid;
};

struct TraceStep {
    std::string      node;
    bool             aclFound{false};
    std::vector<ACE> acl;               // Full ACL at this node
    int              matchedAceIdx{-1}; // Index into acl vector, -1 = no match
    TokenEntryKind   matchedTokenKind{TokenEntryKind::Subject};
};

struct PermissionTrace {
    SubjectKind              subjectKind{SubjectKind::Player};
    std::string              subjectUuid;
    std::string              requestedNode;
    std::vector<std::string> nodePath;
    AccessToken              token;
    std::vector<TraceStep>   steps;
    AccessMask               finalResult{AccessMask::Deny};
};

struct NodeACE {
    std::string node;
    ACE         ace;
};

} // namespace BakaPerms::core
