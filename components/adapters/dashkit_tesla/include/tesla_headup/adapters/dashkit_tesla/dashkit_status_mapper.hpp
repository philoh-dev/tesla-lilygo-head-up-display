#pragma once

#include "tesla_headup/adapters/dashkit_ble/dashkit_connection.hpp"
#include "tesla_headup/application/hud_runtime/system_status_presenter.hpp"

namespace tesla_headup::dashkit {

[[nodiscard]] application::ConnectivityState map_connectivity(
    ConnectionSnapshot snapshot) noexcept;

}  // namespace tesla_headup::dashkit
