#pragma once

#include "tesla_headup/ports/settings_store.hpp"

#include <optional>

namespace tesla_headup::adapters {

class InMemorySettingsStore final : public application::SettingsStore {
public:
    InMemorySettingsStore() = default;

    explicit constexpr InMemorySettingsStore(
        domain::Settings settings) noexcept
        : settings_(settings) {}

    [[nodiscard]] application::LoadSettingsStatus load(
        domain::Settings& destination) noexcept override {
        if (fail_load_) {
            return application::LoadSettingsStatus::StorageError;
        }
        if (!settings_.has_value()) {
            return application::LoadSettingsStatus::NotFound;
        }
        destination = *settings_;
        return application::LoadSettingsStatus::Loaded;
    }

    [[nodiscard]] application::SaveSettingsStatus save(
        const domain::Settings& settings) noexcept override {
        if (fail_save_) {
            return application::SaveSettingsStatus::StorageError;
        }
        settings_ = settings;
        return application::SaveSettingsStatus::Saved;
    }

    void fail_load(bool fail) noexcept { fail_load_ = fail; }
    void fail_save(bool fail) noexcept { fail_save_ = fail; }

    [[nodiscard]] constexpr bool has_value() const noexcept {
        return settings_.has_value();
    }

private:
    std::optional<domain::Settings> settings_{};
    bool fail_load_{false};
    bool fail_save_{false};
};

}  // namespace tesla_headup::adapters
