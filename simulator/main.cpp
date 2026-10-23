#include "lvgl_sdl_display.hpp"

#include "tesla_headup/application/dashboard/blind_spot_warnings.hpp"
#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/dashboard.hpp"
#include "tesla_headup/application/dashboard/power_indicator.hpp"
#include "tesla_headup/application/dashboard/battery_charge_indicator.hpp"
#include "tesla_headup/application/dashboard/system_status_indicator.hpp"
#include "tesla_headup/application/dashboard/selected_gear_indicator.hpp"
#include "tesla_headup/application/dashboard/speed_limit_indicator.hpp"
#include "tesla_headup/application/dashboard/turn_signal_indicators.hpp"
#include "tesla_headup/application/dashboard/autopilot_indicator.hpp"
#include "tesla_headup/application/hud_runtime/dashboard_render_scheduler.hpp"
#include "tesla_headup/application/hud_runtime/system_status_presenter.hpp"
#include "tesla_headup/domain/blind_spots.hpp"
#include "tesla_headup/domain/turn_signals.hpp"
#include "tesla_headup/domain/speed_limit.hpp"
#include "tesla_headup/domain/vehicle_speed.hpp"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>

namespace {

constexpr int window_scale = 1;
constexpr std::uint64_t blink_interval_ms = 500;
constexpr std::uint64_t telemetry_interval_ms = 100;
constexpr std::uint64_t first_telemetry_delay_ms = 750;

class SdlSession final {
public:
    SdlSession() = default;
    ~SdlSession() {
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }

    SdlSession(const SdlSession&) = delete;
    SdlSession& operator=(const SdlSession&) = delete;

    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};
};

bool initialize(SdlSession& session, const tesla_headup::simulator::PreviewOptions& options) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }
    session.window = SDL_CreateWindow(
        options.rgb ? "Tesla Head-Up Display Newhaven RGB Simulator" : "Tesla Head-Up Display LVGL/RM67162 Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        options.output_width() * window_scale,
        options.output_height() * window_scale,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (session.window == nullptr) {
        std::fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        return false;
    }

    session.renderer = SDL_CreateRenderer(
        session.window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC |
            SDL_RENDERER_TARGETTEXTURE);
    if (session.renderer == nullptr) {
        session.renderer =
            SDL_CreateRenderer(session.window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (session.renderer == nullptr) {
        std::fprintf(stderr, "SDL renderer creation failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

tesla_headup::domain::VehicleSpeed make_speed(int value) {
    const auto speed = tesla_headup::domain::VehicleSpeed::from_kilometres_per_hour(
        static_cast<std::uint16_t>(value));
    return speed.value_or(tesla_headup::domain::VehicleSpeed::zero());
}

const char* signal_name(tesla_headup::domain::TurnSignalState state) {
    switch (state) {
    case tesla_headup::domain::TurnSignalState::Off:
        return "off";
    case tesla_headup::domain::TurnSignalState::Left:
        return "left";
    case tesla_headup::domain::TurnSignalState::Right:
        return "right";
    case tesla_headup::domain::TurnSignalState::Hazard:
        return "hazard";
    }
    return "unknown";
}

const char* blind_spot_name(tesla_headup::domain::BlindSpots blind_spots) {
    if (blind_spots.left_occupied() && blind_spots.right_occupied()) {
        return "both";
    }
    if (blind_spots.left_occupied()) {
        return "left";
    }
    if (blind_spots.right_occupied()) {
        return "right";
    }
    return "clear";
}

const char* status_name(tesla_headup::application::SystemStatus status) {
    using tesla_headup::application::SystemStatus;
    switch (status) {
    case SystemStatus::Hidden:
        return "live";
    case SystemStatus::Disconnected:
        return "disconnected";
    case SystemStatus::Connecting:
        return "connecting";
    case SystemStatus::Reconnecting:
        return "reconnecting";
    case SystemStatus::PairingRequired:
        return "pairing required";
    case SystemStatus::ConnectionFailed:
        return "connection failed";
    case SystemStatus::WaitingForData:
        return "waiting for data";
    case SystemStatus::NoData:
        return "no data";
    }
    return "unknown";
}

tesla_headup::application::ConnectivityState next_connectivity(
    tesla_headup::application::ConnectivityState current) {
    using tesla_headup::application::ConnectivityState;
    switch (current) {
    case ConnectivityState::Disconnected:
        return ConnectivityState::Connecting;
    case ConnectivityState::Connecting:
        return ConnectivityState::Ready;
    case ConnectivityState::Ready:
        return ConnectivityState::Reconnecting;
    case ConnectivityState::Reconnecting:
        return ConnectivityState::PairingRequired;
    case ConnectivityState::PairingRequired:
        return ConnectivityState::Failed;
    case ConnectivityState::Failed:
        return ConnectivityState::Disconnected;
    }
    return ConnectivityState::Disconnected;
}

tesla_headup::application::TelemetryTime simulator_now() {
    return tesla_headup::application::TelemetryTime{
        static_cast<tesla_headup::application::TelemetryTime::rep>(SDL_GetTicks64())};
}

tesla_headup::domain::BlindSpots next_blind_spot_state(
    tesla_headup::domain::BlindSpots current) {
    if (!current.left_occupied() && !current.right_occupied()) {
        return tesla_headup::domain::BlindSpots::from_occupancy(true, false);
    }
    if (current.left_occupied() && !current.right_occupied()) {
        return tesla_headup::domain::BlindSpots::from_occupancy(false, true);
    }
    if (!current.left_occupied() && current.right_occupied()) {
        return tesla_headup::domain::BlindSpots::from_occupancy(true, true);
    }
    return tesla_headup::domain::BlindSpots::clear();
}

void update_title(
    SDL_Window& window,
    int speed,
    bool enabled,
    tesla_headup::domain::TurnSignals signals,
    tesla_headup::domain::BlindSpots blind_spots,
    tesla_headup::application::SystemStatus status,
    bool telemetry_streaming,
    const tesla_headup::simulator::LvglSdlDisplay& display) {
    char title[480]{};
    char placement[100]{};
    if (display.rgb_output()) {
        const auto origin = display.dashboard_position();
        std::snprintf(placement, sizeof(placement),
                      " RGB 800x480 (%d,%d)",
                      origin.x, origin.y);
    }
    std::snprintf(
        title,
        sizeof(title),
        "Tesla Head-Up Display%s — %d km/h — signal: %s — arrows: speed, A/D/H/S: signals, "
        "B: blind spot (%s), C: connection (%s), "
        "N: telemetry (%s), M: mirror (%s), Space: speed, Esc: quit%s%s",
        placement,
        speed,
        signal_name(signals.state()),
        blind_spot_name(blind_spots),
        status_name(status),
        telemetry_streaming ? "live" : "paused",
        display.projection_mirrored() ? "HUD" : "direct",
        enabled ? "" : " — HIDDEN",
        display.rgb_output() ? " — I/J/K/L: move, Shift: 10px" : "");
    SDL_SetWindowTitle(&window, title);
}

bool has_argument(
    int argument_count,
    char** arguments,
    std::string_view expected) {
    for (int index = 1; index < argument_count; ++index) {
        if (std::string_view{arguments[index]} == expected) {
            return true;
        }
    }
    return false;
}

const char* screenshot_path(int argument_count, char** arguments) {
    for (int index = 1; index + 1 < argument_count; ++index) {
        if (std::string_view{arguments[index]} == "--screenshot") {
            return arguments[index + 1];
        }
    }
    return nullptr;
}

int run_rgb_diagnostics(SdlSession& session, tesla_headup::simulator::LvglSdlDisplay& display,
    const tesla_headup::simulator::PreviewOptions& options, int count, char** args) {
    using tesla_headup::platform::RgbDiagnostics;
    tesla_headup::platform::RgbDiagnosticSequence sequence;
    sequence.select(options.diagnostic_pattern, SDL_GetTicks());
    sequence.set_automatic(!options.explicit_pattern, SDL_GetTicks());
    (void)display.set_diagnostic_pattern(sequence.pattern());
    if (const char* path = screenshot_path(count, args)) {
        if (!display.save_screenshot(path)) {
            std::fprintf(stderr, "Diagnostic screenshot failed: %s\n", SDL_GetError());
            return 1;
        }
        return 0;
    }
    if (has_argument(count, args, "--smoke-test")) return 0;
    bool running = true;
    while (running) {
        const auto origin = display.dashboard_position();
        const auto name = RgbDiagnostics::name(sequence.pattern());
        char title[320]{};
        std::snprintf(title, sizeof(title),
            "RGB %.*s (%d,%d) %s %s | Space: next, Backspace: previous, A: auto, M: mirror, IJKL: move, Esc: quit",
            static_cast<int>(name.size()), name.data(), origin.x, origin.y,
            sequence.automatic() ? "AUTO" : "PAUSED", display.projection_mirrored() ? "MIRRORED" : "DIRECT");
        SDL_SetWindowTitle(session.window, title);
        SDL_Event event{};
        if (SDL_WaitEventTimeout(&event, 16)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                const auto now = SDL_GetTicks();
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_SPACE:
                case SDLK_BACKSPACE:
                    sequence.select(event.key.keysym.sym == SDLK_SPACE
                        ? RgbDiagnostics::next(sequence.pattern()) : RgbDiagnostics::previous(sequence.pattern()), now);
                    sequence.set_automatic(false, now);
                    (void)display.set_diagnostic_pattern(sequence.pattern());
                    break;
                case SDLK_a: sequence.set_automatic(!sequence.automatic(), now); break;
                case SDLK_m: display.set_projection_mirrored(!display.projection_mirrored()); break;
                case SDLK_i: case SDLK_j: case SDLK_k: case SDLK_l: {
                    const int step = event.key.keysym.mod & KMOD_SHIFT ? 10 : 1;
                    display.move_dashboard(
                        event.key.keysym.sym == SDLK_j ? -step : event.key.keysym.sym == SDLK_l ? step : 0,
                        event.key.keysym.sym == SDLK_i ? -step : event.key.keysym.sym == SDLK_k ? step : 0);
                    break;
                }
                default: break;
                }
            } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                display.refresh();
            }
        }
        if (sequence.tick(SDL_GetTicks())) (void)display.set_diagnostic_pattern(sequence.pattern());
        display.process();
    }
    return 0;
}

}  // namespace

int main(int argument_count, char** arguments) {
    const auto options = tesla_headup::simulator::PreviewOptions::parse(argument_count, arguments);
    if (!options.error.empty()) {
        std::fprintf(stderr, "Preview options: %.*s\n",
                     static_cast<int>(options.error.size()), options.error.data());
        return 1;
    }
    if (has_argument(argument_count, arguments, "--help")) {
        std::puts("Usage: tesla_headup_simulator [--rgb | --rgb-viewport [--origin-x 0..264] [--origin-y 0..240]]\n"
                  "  [--direct-view] [--screenshot output.bmp] [--smoke-test]\n"
                  "  [--preview-{left-turn-signal,right-turn-signal,no-turn-signal}]\n"
                  "  [--preview-dim-turn-signal] [--preview-right-blind-spot | --preview-both-blind-spots]\n"
                  "  [--preview-speed 0..400] [--preview-speed-limit 1..150]\n"
                  "  [--preview-regeneration] [--preview-park] [--preview-connected] [--preview-no-autopilot]\n"
                  "  [--rgb-diagnostics [--diagnostic-pattern NAME]]\n"
                  "Diagnostic patterns: red green blue white black checkerboard corners viewport.\n"
                  "Diagnostics: Space next, Backspace previous, A auto; M and IJKL still work.\n"
                  "RGB road_scene fills 800x480. --rgb-viewport retains movable 536x240 output.\n"
                  "RGB viewport controls: I/J/K/L move by 1px; Shift moves 10px.\n"
                  "Coordinates are before mirroring. M mirrors the entire output.\n"
                  "Other controls: arrows speed, A/D/H/S signals, B blind spot, Q Autopilot,\n"
                  "C connection, N telemetry, P power, G gear, Space speed visibility, Esc quit.");
        return 0;
    }
    SdlSession session;
    if (!initialize(session, options)) {
        return 1;
    }

    tesla_headup::simulator::LvglSdlDisplay display{*session.renderer, options};
    if (!display.ready()) {
        std::fprintf(
            stderr,
            "Could not initialize the LVGL display simulator: %s\n",
            SDL_GetError());
        return 1;
    }
    display.set_projection_mirrored(
        !has_argument(argument_count, arguments, "--direct-view"));
    if (options.diagnostics) return run_rgb_diagnostics(session, display, options, argument_count, arguments);
    tesla_headup::application::BlindSpotWarnings blind_spot_feature;
    tesla_headup::application::CenteredVehicleSpeed speed_feature;
    tesla_headup::application::TurnSignalIndicators turn_signal_feature;
    tesla_headup::application::AutopilotIndicator autopilot_feature;
    tesla_headup::application::SystemStatusIndicator status_feature;
    tesla_headup::application::SelectedGearIndicator gear_feature;
    tesla_headup::application::SpeedLimitIndicator speed_limit_feature;
    tesla_headup::application::PowerIndicator power_feature;
    tesla_headup::application::BatteryChargeIndicator battery_charge_feature;
    tesla_headup::application::Dashboard dashboard{display.canvas()};
    if (!dashboard.add_feature(blind_spot_feature) ||
        !dashboard.add_feature(speed_feature, false) ||
        !dashboard.add_feature(turn_signal_feature) ||
        !dashboard.add_feature(autopilot_feature) ||
        !dashboard.add_feature(status_feature) ||
        !dashboard.add_feature(gear_feature) ||
        !dashboard.add_feature(speed_limit_feature) ||
        !dashboard.add_feature(power_feature) ||
        !dashboard.add_feature(battery_charge_feature)) {
        std::fprintf(stderr, "Could not register dashboard features\n");
        return 1;
    }
    tesla_headup::application::DashboardRenderScheduler render_scheduler{dashboard};
    tesla_headup::application::SystemStatusPresenter status_presenter{
        status_feature,
        render_scheduler};

    int speed = 0;
    std::int16_t simulated_current_deciamps = 2000;
    bool enabled = true;
    bool telemetry_streaming = true;
    auto connectivity = tesla_headup::application::ConnectivityState::Connecting;
    auto signals = tesla_headup::domain::TurnSignals::off();
    auto blind_spots = tesla_headup::domain::BlindSpots::clear();
    auto speed_limit = *tesla_headup::domain::SpeedLimit::from_display_value(50);
    speed_limit_feature.set_speed_limit(speed_limit);
    std::uint64_t next_blink_at = SDL_GetTicks64() + blink_interval_ms;
    std::uint64_t next_telemetry_at =
        SDL_GetTicks64() + first_telemetry_delay_ms;
    std::optional<tesla_headup::application::TelemetryTime> last_telemetry_at;
    const char* requested_screenshot =
        screenshot_path(argument_count, arguments);
    const bool deterministic_preview =
        has_argument(argument_count, arguments, "--smoke-test") ||
        requested_screenshot != nullptr;
    if (!deterministic_preview) {
        (void)status_presenter.set_connectivity(
            connectivity,
            simulator_now());
        if (render_scheduler.tick(simulator_now())) {
            display.refresh();
        }
        update_title(
            *session.window,
            speed,
            enabled,
            signals,
            blind_spots,
            status_feature.status(),
            telemetry_streaming,
            display);
    }

    if (deterministic_preview) {
        battery_charge_feature.set_charge(tesla_headup::domain::BatteryCharge::from_percent(51));
        if (has_argument(argument_count, arguments, "--preview-regeneration")) {
            simulated_current_deciamps = -1000;
        }
        power_feature.set_power(tesla_headup::domain::PowerFlow::from_battery(
            {40000, simulated_current_deciamps}, {32000, 8000}));
        speed = options.preview_speed;
        speed_limit = *tesla_headup::domain::SpeedLimit::from_display_value(
            static_cast<std::uint16_t>(options.preview_speed_limit));
        signals = tesla_headup::domain::TurnSignals::hazard();
        if (has_argument(
                argument_count,
                arguments,
                "--preview-no-turn-signal")) {
            signals = tesla_headup::domain::TurnSignals::off();
        } else if (has_argument(
                       argument_count,
                       arguments,
                       "--preview-left-turn-signal")) {
            signals = tesla_headup::domain::TurnSignals::left();
        } else if (has_argument(
                       argument_count,
                       arguments,
                       "--preview-right-turn-signal")) {
            signals = tesla_headup::domain::TurnSignals::right();
        }
        speed_feature.set_speed(make_speed(speed));
        (void)dashboard.set_enabled(
            tesla_headup::application::FeatureId::VehicleSpeed,
            true);
        turn_signal_feature.set_signals(signals);
        turn_signal_feature.set_illuminated(!has_argument(
            argument_count, arguments, "--preview-dim-turn-signal"));
        autopilot_feature.set_illuminated(!has_argument(
            argument_count, arguments, "--preview-no-autopilot"));
        const bool parked_preview = has_argument(argument_count, arguments, "--preview-park");
        gear_feature.set_gear(parked_preview ? tesla_headup::domain::SelectedGear::Park
                                           : tesla_headup::domain::SelectedGear::Drive);
        if (parked_preview) {
            speed_feature.set_speed(make_speed(0));
            turn_signal_feature.set_signals(tesla_headup::domain::TurnSignals::off());
            autopilot_feature.set_illuminated(false);
        }
        speed_limit_feature.set_speed_limit(speed_limit);
        blind_spots =
            tesla_headup::domain::BlindSpots::from_occupancy(true, false);
        if (has_argument(
                argument_count,
                arguments,
                "--preview-right-blind-spot")) {
            blind_spots =
                tesla_headup::domain::BlindSpots::from_occupancy(false, true);
        }
        if (has_argument(argument_count, arguments, "--preview-both-blind-spots"))
            blind_spots = tesla_headup::domain::BlindSpots::from_occupancy(true, true);
        if (parked_preview) blind_spots = tesla_headup::domain::BlindSpots::from_occupancy(false, false);
        blind_spot_feature.set_blind_spots(blind_spots);
        const bool connected_preview = has_argument(argument_count, arguments, "--preview-connected");
        connectivity = connected_preview ? tesla_headup::application::ConnectivityState::Ready
                                        : tesla_headup::application::ConnectivityState::Reconnecting;
        (void)status_presenter.set_connectivity(
            connectivity,
            simulator_now());
        if (connected_preview) (void)status_presenter.on_telemetry_received(simulator_now());
        dashboard.render();
        display.refresh();
        if (requested_screenshot != nullptr &&
            !display.save_screenshot(requested_screenshot)) {
            std::fprintf(
                stderr,
                "Could not save screenshot '%s': %s\n",
                requested_screenshot,
                SDL_GetError());
            return 1;
        }
        return 0;
    }

    bool running = true;
    while (running) {
        SDL_Event event{};
        bool changed = false;
        bool status_changed = false;
        const bool received_event = SDL_WaitEventTimeout(&event, 16) != 0;
        if (received_event && event.type == SDL_QUIT) {
            running = false;
        } else if (received_event && event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
            case SDLK_i:
            case SDLK_j:
            case SDLK_k:
            case SDLK_l: {
                const int step = event.key.keysym.mod & KMOD_SHIFT ? 10 : 1;
                display.move_dashboard(
                    event.key.keysym.sym == SDLK_j ? -step :
                        event.key.keysym.sym == SDLK_l ? step : 0,
                    event.key.keysym.sym == SDLK_i ? -step :
                        event.key.keysym.sym == SDLK_k ? step : 0);
                changed = true;
                break;
            }
            case SDLK_ESCAPE:
                running = false;
                break;
            case SDLK_UP:
            case SDLK_RIGHT:
                speed = std::min(
                    speed + (event.key.keysym.mod & KMOD_SHIFT ? 10 : 1),
                    static_cast<int>(
                        tesla_headup::domain::VehicleSpeed::
                            maximum_kilometres_per_hour));
                changed = true;
                break;
            case SDLK_DOWN:
            case SDLK_LEFT:
                speed = std::max(
                    speed - (event.key.keysym.mod & KMOD_SHIFT ? 10 : 1),
                    0);
                changed = true;
                break;
            case SDLK_r:
                speed = 0;
                changed = true;
                break;
            case SDLK_a:
                signals = signals.state() == tesla_headup::domain::TurnSignalState::Left
                              ? tesla_headup::domain::TurnSignals::off()
                              : tesla_headup::domain::TurnSignals::left();
                turn_signal_feature.set_signals(signals);
                turn_signal_feature.set_illuminated(true);
                next_blink_at = SDL_GetTicks64() + blink_interval_ms;
                changed = true;
                break;
            case SDLK_d:
                signals = signals.state() == tesla_headup::domain::TurnSignalState::Right
                              ? tesla_headup::domain::TurnSignals::off()
                              : tesla_headup::domain::TurnSignals::right();
                turn_signal_feature.set_signals(signals);
                turn_signal_feature.set_illuminated(true);
                next_blink_at = SDL_GetTicks64() + blink_interval_ms;
                changed = true;
                break;
            case SDLK_h:
                signals = signals.state() ==
                                  tesla_headup::domain::TurnSignalState::Hazard
                              ? tesla_headup::domain::TurnSignals::off()
                              : tesla_headup::domain::TurnSignals::hazard();
                turn_signal_feature.set_signals(signals);
                turn_signal_feature.set_illuminated(true);
                next_blink_at = SDL_GetTicks64() + blink_interval_ms;
                changed = true;
                break;
            case SDLK_s:
                signals = tesla_headup::domain::TurnSignals::off();
                turn_signal_feature.set_signals(signals);
                changed = true;
                break;
            case SDLK_b:
                blind_spots = next_blind_spot_state(blind_spots);
                blind_spot_feature.set_blind_spots(blind_spots);
                changed = true;
                break;
            case SDLK_p:
                simulated_current_deciamps = simulated_current_deciamps > 0
                    ? static_cast<std::int16_t>(-1000)
                    : (simulated_current_deciamps < 0
                        ? static_cast<std::int16_t>(0)
                        : static_cast<std::int16_t>(2000));
                changed = true;
                break;
            case SDLK_c:
                connectivity = next_connectivity(connectivity);
                status_changed = status_presenter.set_connectivity(
                    connectivity,
                    simulator_now());
                if (connectivity ==
                    tesla_headup::application::ConnectivityState::Ready) {
                    next_telemetry_at =
                        SDL_GetTicks64() + first_telemetry_delay_ms;
                }
                changed = true;
                break;
            case SDLK_n:
                telemetry_streaming = !telemetry_streaming;
                if (telemetry_streaming &&
                    connectivity ==
                        tesla_headup::application::ConnectivityState::Ready) {
                    const auto received_at = simulator_now();
                    status_changed = status_presenter.on_telemetry_received(
                                         received_at) ||
                                     status_changed;
                    last_telemetry_at = received_at;
                    speed_feature.set_speed(make_speed(speed));
                    next_telemetry_at =
                        SDL_GetTicks64() + telemetry_interval_ms;
                }
                changed = true;
                break;
            case SDLK_SPACE:
                enabled = !enabled;
                (void)dashboard.set_enabled(
                    tesla_headup::application::FeatureId::VehicleSpeed,
                    enabled);
                changed = true;
                break;
            case SDLK_q:
                autopilot_feature.set_illuminated(
                    !autopilot_feature.illuminated());
                changed = true;
                break;
            case SDLK_m:
                display.set_projection_mirrored(
                    !display.projection_mirrored());
                changed = true;
                break;
            case SDLK_g: {
                using tesla_headup::domain::SelectedGear;
                const auto next = [gear = gear_feature.gear()] {
                    switch (gear) {
                    case SelectedGear::Park:
                        return SelectedGear::Reverse;
                    case SelectedGear::Reverse:
                        return SelectedGear::Neutral;
                    case SelectedGear::Neutral:
                        return SelectedGear::Drive;
                    case SelectedGear::Drive:
                        return SelectedGear::Park;
                    }
                    return SelectedGear::Park;
                }();
                gear_feature.set_gear(next);
                changed = true;
                break;
            }
            default:
                break;
            }
        } else if (received_event && event.type == SDL_WINDOWEVENT &&
                   event.window.event == SDL_WINDOWEVENT_EXPOSED) {
            changed = true;
        }

        const std::uint64_t current_time = SDL_GetTicks64();
        const auto current_telemetry_time =
            tesla_headup::application::TelemetryTime{
                static_cast<tesla_headup::application::TelemetryTime::rep>(
                    current_time)};
        if (connectivity == tesla_headup::application::ConnectivityState::Ready &&
            telemetry_streaming && current_time >= next_telemetry_at) {
            status_changed = status_presenter.on_telemetry_received(
                                 current_telemetry_time) ||
                             status_changed;
            last_telemetry_at = current_telemetry_time;
            speed_feature.set_speed(make_speed(speed));
            next_telemetry_at = current_time + telemetry_interval_ms;
        }
        status_changed = status_presenter.refresh(current_telemetry_time) ||
                         status_changed;
        const auto telemetry_timeout =
            tesla_headup::application::SystemStatusPolicy{}.telemetry_timeout;
        const bool speed_data_fresh =
            last_telemetry_at.has_value() &&
            current_telemetry_time >= *last_telemetry_at &&
            (current_telemetry_time - *last_telemetry_at) <=
                telemetry_timeout;
        const auto simulated_power = speed_data_fresh
            ? tesla_headup::domain::PowerFlow::from_battery(
                {40000, simulated_current_deciamps}, {32000, 8000})
            : std::nullopt;
        const auto simulated_charge = speed_data_fresh
            ? tesla_headup::domain::BatteryCharge::from_percent(51) : std::nullopt;
        if (battery_charge_feature.charge() != simulated_charge) {
            battery_charge_feature.set_charge(simulated_charge);
            changed = true;
        }
        if (power_feature.power() != simulated_power) {
            power_feature.set_power(simulated_power);
            changed = true;
        }
        if (dashboard.is_enabled(
                tesla_headup::application::FeatureId::VehicleSpeed) !=
            (enabled && speed_data_fresh)) {
            (void)dashboard.set_enabled(
                tesla_headup::application::FeatureId::VehicleSpeed,
                enabled && speed_data_fresh);
            changed = true;
        }
        if (signals.state() != tesla_headup::domain::TurnSignalState::Off &&
            current_time >= next_blink_at) {
            turn_signal_feature.set_illuminated(
                !turn_signal_feature.illuminated());
            next_blink_at = current_time + blink_interval_ms;
            changed = true;
        }

        if (changed) {
            render_scheduler.request_render();
        }
        if (render_scheduler.tick(current_telemetry_time)) {
            display.refresh();
        }
        if (changed || status_changed) {
            update_title(
                *session.window,
                speed,
                enabled,
                signals,
                blind_spots,
                status_feature.status(),
                telemetry_streaming,
                display);
        }
        display.process();
    }
    return 0;
}
