#include "BakaPerms/Utils/I18n/I18n.hpp"
#include "BakaPerms/BakaPerms.hpp"

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/network/LoopbackPacketSender.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/packet/AvailableCommandsPacket.h>
#include <mc/server/ServerPlayer.h>
#include <mc/world/actor/player/Player.h>

#include <nlohmann/json.hpp>

#include <Windows.h>

namespace BakaPerms::utils::i18n {

std::string I18nManager::loadResourceJson(const int resId) {
    const auto& handle = static_cast<HMODULE>(BakaPerms::getInstance().getSelf().getHandle());

    HRSRC hRes = FindResourceW(handle, MAKEINTRESOURCE(resId), L"JSON");
    if (!hRes) return {};

    const DWORD   size  = SizeofResource(handle, hRes);
    const HGLOBAL hData = LoadResource(handle, hRes);
    if (!hData) return {};

    const auto data = static_cast<const char*>(LockResource(hData));
    return {data, size};
}

void I18nManager::loadAndAddTranslations(const int resId, const std::string& localeCode) const {
    if (auto jsonStr = loadResourceJson(resId); !jsonStr.empty()) {
        try {
            const auto json = nlohmann::json::parse(jsonStr);
            injectTranslations(json, localeCode);
        } catch (const nlohmann::json::parse_error&) {
            // Malformed embedded resource — skip this locale silently.
        }
    }
}

void I18nManager::processLangFile(
    const nlohmann::json&                                              obj,
    const std::string&                                                 prefix,
    const std::function<void(const std::string&, const std::string&)>& setter
) {
    for (auto& [key, val] : obj.items()) {
        std::string fullKey;
        if (!prefix.empty()) {
            fullKey.reserve(prefix.size() + 1 + key.size());
            fullKey += prefix;
            fullKey += '.';
        } else {
            fullKey.reserve(key.size());
        }
        fullKey += key;

        if (val.is_object()) {
            processLangFile(val, fullKey, setter);
        } else if (val.is_string()) {
            setter(fullKey, val.get<std::string>());
        }
    }
}

I18nManager& I18nManager::getInstance() {
    static I18nManager instance;
    return instance;
}

std::string_view I18nManager::get(const std::string_view key, const std::string_view langCode) const {
    return i18n.get(key, langCode);
}

void I18nManager::init() const {
    for (auto [resId, locale] : resources) {
        loadAndAddTranslations(resId, locale);
    }
}

void I18nManager::injectTranslations(const nlohmann::json& json, const std::string& localeCode) const {
    processLangFile(json, "", [&](const std::string& key, const std::string& value) {
        i18n.set(localeCode, key, value);
    });
}

LL_AUTO_TYPE_INSTANCE_HOOK(
    SendToClientHook,
    ll::memory::HookPriority::Normal,
    LoopbackPacketSender,
    &LoopbackPacketSender::$sendToClient,
    void,
    ::NetworkIdentifier const& id,
    ::Packet const&            packet,
    ::SubClientId              recipientSubId
) {
    if (packet.getId() == MinecraftPacketIds::AvailableCommands) {
        if (const auto handler = ll::service::getServerNetworkHandler()) {
            const auto* player = handler->_getServerPlayer(id, packet.mSenderSubId);

            auto& commandInfoPkt =
                const_cast<AvailableCommandsPacket&>(static_cast<AvailableCommandsPacket const&>(packet));

            for (auto& it : commandInfoPkt.mCommands.get()) {
                it.description = I18nManager::getInstance().get(it.description.get(), getPlayerLangCode(player));
            }
        }
    }
    origin(id, packet, recipientSubId);
}

} // namespace BakaPerms::utils::i18n
