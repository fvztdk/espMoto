#pragma once

#include <Arduino.h>
#include "ble_protocol.h"

enum class MetricId : uint8_t {
    NONE             = 0,
    RPM              = 1,
    SPEED            = 2,
    COOLANT_TEMP     = 3,
    VOLTAGE          = 4,
    THROTTLE         = 5,
    INTAKE_TEMP      = 6,
    GEAR             = 7,
    OIL_PRESSURE     = 8,
    LEAN_ANGLE       = 9,
    CLUTCH_KICKSTAND = 10,
    ABS_TC           = 11
};

struct NavState {
    bool active = false;
    ManeuverType maneuver = ManeuverType::NONE;
    String distance = "--";
    String street = "Waiting for phone...";
    String eta = "--:--";
    uint8_t roundaboutExit = 0; // 0 = auto-detect from maneuver / street text
    uint32_t lastUpdateMs = 0;
};

struct BikeTelemetry {
    bool obdConnected = false;
    uint16_t rpm = 0;
    uint16_t speedKmh = 0;
    int16_t engineTempC = 0;
    float batteryVolts = 0.0f;
    uint8_t throttlePct = 0;
    int16_t intakeTempC = 0;
    int8_t gear = 0; // 0 = N, 1..6 = gears
    float oilPressureBar = 0.0f;
    int16_t leanAngleDeg = 0;
    bool clutchEngaged = false;
    bool kickstandDown = false;
    bool absActive = false;
    uint32_t validMask = 0;
    uint32_t lastUpdateMs = 0;

    bool isValid(MetricId id) const {
        uint8_t bit = static_cast<uint8_t>(id);
        if (bit == 0 || bit >= 32) return false;
        return (validMask & (1UL << bit)) != 0;
    }

    void setValid(MetricId id, bool valid) {
        uint8_t bit = static_cast<uint8_t>(id);
        if (bit == 0 || bit >= 32) return;
        if (valid) {
            validMask |= (1UL << bit);
        } else {
            validMask &= ~(1UL << bit);
        }
    }
};
