#pragma once

#include "tesla_headup/ports/settings_store.hpp"

namespace tesla_headup::platform {

class EspNvsSettingsStore final : public application::SettingsStore {
public:
    [[nodiscard]] application::LoadSettingsStatus load(
        domain::Settings& destination) noexcept override;

    [[nodiscard]] application::SaveSettingsStatus save(
        const domain::Settings& settings) noexcept override;
};

}  // namespace tesla_headup::platform
