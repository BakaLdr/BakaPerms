# BakaPerms

**[中文](README_ZH.md) | English**

A DACL-based permission management mod for [LeviLamina](https://github.com/LiteLDev/LeviLamina) (Minecraft Bedrock Edition).

## Features

- **DACL (Discretionary Access Control List)** — Ordered ACEs with first-match-wins resolution
- **Hierarchical permission nodes** — Dot-separated nodes (e.g. `baka.perms.test`) with automatic parent fallback
- **Group inheritance** — Groups can have parent groups, forming an inheritance chain
- **Wildcard subjects** — Use `*` to match all players and groups
- **Per-player caching** — Generation-based cache with automatic invalidation
- **Trace diagnostics** — Step-by-step resolution trace for debugging permission issues
- **I18n** — Built-in English and Chinese localization

## Quick Start

1. Install BakaPerms via [Lip](https://lip.levimc.org): `lip install github.com/BakaLdr/BakaPerms`
2. Start the server — database and config are created automatically
3. Enjoy it

## Commands

All commands use the `/perms` prefix and require console permission level.

| Command                                                                  | Description               |
|--------------------------------------------------------------------------|---------------------------|
| `/perms reload`                                                          | Clear permission cache    |
| `/perms group create <name>`                                             | Create a group            |
| `/perms group delete <name>`                                             | Delete a group            |
| `/perms group setparent <name> <parent\|none>`                           | Set or clear parent group |
| `/perms group list`                                                      | List all groups           |
| `/perms group info <name>`                                               | Show group details        |
| `/perms group check <name> <node> [trace]`                               | Check group permission    |
| `/perms user addgroup <player> <group>`                                  | Add player to group       |
| `/perms user removegroup <player> <group>`                               | Remove player from group  |
| `/perms user info <player>`                                              | Show player details       |
| `/perms user check <player> <node> [trace]`                              | Check player permission   |
| `/perms acl add <node> <subject> <player\|group> <allow\|deny>`          | Append ACE                |
| `/perms acl insert <node> <pos> <subject> <player\|group> <allow\|deny>` | Insert ACE at position    |
| `/perms acl remove <node> <pos>`                                         | Remove ACE                |
| `/perms acl move <node> <from> <to>`                                     | Reorder ACE               |
| `/perms acl info <node>`                                                 | Show ACL for a node       |
| `/perms acl clear <node>`                                                | Clear all ACEs on a node  |

## For Developers

Other mods can link against BakaPerms and use the C++ API directly:

```cpp
#include "BakaPerms/Core/PermissionManager.hpp"

auto& mgr = BakaPerms::BakaPerms::getInstance().getPermissionManager();
auto result = mgr.checkPermission(playerUuid, "some.permission.node");
```

## Building

Requires C++23 and [xmake](https://xmake.io).

```bash
xmake f -y -p windows -a x64 -m release
xmake
```

## License

CC0-1.0
