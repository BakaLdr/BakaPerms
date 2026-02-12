#pragma once
#include "BakaPerms/BakaPerms.hpp"

namespace BakaPerms {
constexpr auto         MOD_NAME = "BakaPerms";
inline ll::io::Logger& logger   = BakaPerms::getInstance().getSelf().getLogger();
} // namespace BakaPerms
