# BakaPerms

**中文 | [English](README.md)**

基于 DACL 的权限管理模组，适用于 [LeviLamina](https://github.com/LiteLDev/LeviLamina)（Minecraft 基岩版）。

## 快速开始

1. 通过 [Lip](https://lip.levimc.org) 安装：`lip install github.com/BakaLdr/BakaPerms`
2. 启动服务器 — 数据库和配置文件会自动创建
3. 体验

## 命令列表

所有命令以 `/perms` 为前缀，需要控制台权限。

| 命令                                                               | 说明          |
|------------------------------------------------------------------|-------------|
| `/perms reload`                                                  | 清除权限缓存      |
| `/perms group create <名称>`                                       | 创建用户组       |
| `/perms group delete <名称>`                                       | 删除用户组       |
| `/perms group setparent <名称> <父组\|none>`                         | 设置或清除父组     |
| `/perms group list`                                              | 列出所有用户组     |
| `/perms group info <名称>`                                         | 查看用户组详情     |
| `/perms group check <名称> <节点> [trace]`                           | 检查用户组权限     |
| `/perms user addgroup <玩家> <用户组>`                                | 将玩家添加到用户组   |
| `/perms user removegroup <玩家> <用户组>`                             | 将玩家从用户组移除   |
| `/perms user info <玩家>`                                          | 查看玩家详情      |
| `/perms user check <玩家> <节点> [trace]`                            | 检查玩家权限      |
| `/perms acl add <节点> <主体> <player\|group> <allow\|deny>`         | 追加 ACE      |
| `/perms acl insert <节点> <位置> <主体> <player\|group> <allow\|deny>` | 在指定位置插入 ACE |
| `/perms acl remove <节点> <位置>`                                    | 移除 ACE      |
| `/perms acl move <节点> <原位置> <新位置>`                               | 调整 ACE 顺序   |
| `/perms acl info <节点>`                                           | 查看节点的 ACL   |
| `/perms acl clear <节点>`                                          | 清除节点的所有 ACE |

## 开发者接入

其他模组可以链接 BakaPerms，直接使用 C++ API：

```cpp
#include "BakaPerms/Core/PermissionManager.hpp"

auto& mgr = BakaPerms::BakaPerms::getInstance().getPermissionManager();
auto result = mgr.checkPermission(playerUuid, "some.permission.node");
```

## 构建

需要 C++23 和 [xmake](https://xmake.io)。

```bash
xmake f -y -p windows -a x64 -m release
xmake
```

## 许可证

MIT