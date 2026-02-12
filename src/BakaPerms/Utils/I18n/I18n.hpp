#pragma once
#include <ll/api/i18n/I18n.h>

#include <mc/world/actor/player/Player.h>
#include <nlohmann/json.hpp>

using namespace ll::i18n_literals;

namespace BakaPerms::utils::i18n {

inline std::string_view getDefaultLangCode() { return ll::i18n::getDefaultLocaleCode(); }

inline std::string getPlayerLangCode(const Player* player) {
    return player ? player->getLocaleCode() : std::string(getDefaultLangCode());
}

class I18nManager {
public:
    I18nManager(const I18nManager&)            = delete;
    I18nManager& operator=(const I18nManager&) = delete;

    static I18nManager& getInstance();

    [[nodiscard]] std::string_view get(std::string_view key, std::string_view langCode = "") const;

    void init() const;

    void injectTranslations(const nlohmann::json& json, const std::string& localeCode) const;

private:
    I18nManager()  = default;
    ~I18nManager() = default;

    ll::i18n::I18n& i18n = ll::i18n::getInstance();

    static constexpr std::array<std::pair<int, const char*>, 2> resources = {
        {
         {101, "en-US"},
         {102, "zh-CN"},
         }
    };

    static std::string loadResourceJson(int resId);

    void loadAndAddTranslations(int resId, const std::string& localeCode) const;

    static void processLangFile(
        const nlohmann::json&                                              obj,
        const std::string&                                                 prefix,
        const std::function<void(const std::string&, const std::string&)>& setter
    );
};

} // namespace BakaPerms::utils::i18n
