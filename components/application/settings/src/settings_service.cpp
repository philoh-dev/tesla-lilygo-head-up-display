#include "tesla_headup/application/settings/settings_service.hpp"

namespace tesla_headup::application {

SettingsLoadOutcome SettingsService::load() noexcept {
    domain::Settings loaded;
    const auto status = store_.load(loaded);
    if (status == LoadSettingsStatus::Loaded) {
        return {loaded, status};
    }
    return {domain::Settings{}, status};
}

SaveSettingsStatus SettingsService::save(
    const domain::Settings& settings) noexcept {
    return store_.save(settings);
}

}  // namespace tesla_headup::application
