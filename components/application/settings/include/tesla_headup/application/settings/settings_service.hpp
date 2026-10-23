#pragma once

#include "tesla_headup/ports/settings_store.hpp"

namespace tesla_headup::application {

struct SettingsLoadOutcome {
    domain::Settings settings{};
    LoadSettingsStatus status{LoadSettingsStatus::NotFound};
};

class SettingsService final {
public:
    explicit constexpr SettingsService(SettingsStore& store) noexcept
        : store_(store) {}

    [[nodiscard]] SettingsLoadOutcome load() noexcept;
    [[nodiscard]] SaveSettingsStatus save(
        const domain::Settings& settings) noexcept;

private:
    SettingsStore& store_;
};

}  // namespace tesla_headup::application
