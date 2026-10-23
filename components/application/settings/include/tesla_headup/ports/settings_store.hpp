#pragma once

#include "tesla_headup/domain/settings.hpp"

#include <cstdint>

namespace tesla_headup::application {

enum class LoadSettingsStatus : std::uint8_t {
    Loaded,
    NotFound,
    InvalidData,
    StorageError,
};

enum class SaveSettingsStatus : std::uint8_t {
    Saved,
    StorageError,
};

class SettingsReader {
public:
    virtual ~SettingsReader() = default;

    [[nodiscard]] virtual LoadSettingsStatus load(
        domain::Settings& destination) noexcept = 0;
};

class SettingsWriter {
public:
    virtual ~SettingsWriter() = default;

    [[nodiscard]] virtual SaveSettingsStatus save(
        const domain::Settings& settings) noexcept = 0;
};

class SettingsStore : public SettingsReader, public SettingsWriter {
public:
    ~SettingsStore() override = default;
};

}  // namespace tesla_headup::application
