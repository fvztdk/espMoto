#pragma once

// Shared BLE GATT UUIDs for Phone (Android) -> M5Stack StopWatch Navigation
namespace BleProtocol {
    constexpr const char* DEVICE_NAME          = "espMoto";
    constexpr const char* NAV_SERVICE_UUID     = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    constexpr const char* NAV_CHAR_UUID        = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
    constexpr const char* OBD_CONFIG_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9";
    constexpr const char* PAIR_CHAR_UUID       = "beb5483e-36e1-4688-b7f5-ea07361b26aa";
}

// Maneuver IDs mirrored from Google Maps / Drive Assistant style
enum class ManeuverType : uint8_t {
    NONE            = 0,
    STRAIGHT        = 1,
    TURN_RIGHT      = 2,
    TURN_LEFT       = 3,
    SLIGHT_RIGHT    = 4,
    SLIGHT_LEFT     = 5,
    SHARP_RIGHT     = 6,
    SHARP_LEFT      = 7,
    UTURN_LEFT      = 8,
    UTURN_RIGHT     = 9,
    ROUNDABOUT        = 10,
    MERGE             = 11,
    DESTINATION       = 12,
    ROUNDABOUT_EXIT_1 = 13,
    ROUNDABOUT_EXIT_2 = 14,
    ROUNDABOUT_EXIT_3 = 15,
    ROUNDABOUT_EXIT_4 = 16,
    ROUNDABOUT_EXIT_5 = 17
};
