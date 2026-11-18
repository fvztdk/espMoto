#pragma once

#include <cstddef>
#include <cstdint>
#include "nav_state.h"

enum class ObdProtocol : uint8_t {
    AUTO                = 0, // ATSP0 - Auto-detect (Euro 4/5+ CAN)
    J1850_VPW           = 2, // ATSP2 - Pre-2014 Harley-Davidson
    ISO_9141_2          = 3, // ATSP3 - Legacy K-Line
    ISO_14230_KWP_FAST  = 5, // ATSP5 - Honda / Yamaha K-Line
    ISO_15765_11_500    = 6, // ATSP6 - Standard 11-bit 500kbps CAN (Euro 4/5+)
    ISO_15765_29_500    = 7  // ATSP7 - 29-bit 500kbps CAN (HDLAN / Euro 5+)
};

constexpr size_t MAX_OBD_PARAMS = 12;

struct ObdParamConfig {
    MetricId id = MetricId::NONE;
    bool enabled = false;
    char label[8] = "";
    char unit[6] = "";
    char header[8] = "";     // Optional ATSH CAN/UDS header, e.g. "7E0"
    char requestHex[12] = ""; // e.g. "010C", "221004"
    uint8_t byteOffset = 0;  // Byte index into response data payload (after echo)
    uint8_t byteLength = 1;  // 1 or 2 bytes
    bool isSigned = false;
    uint16_t bitMask = 0;    // 0 = no mask
    uint8_t bitShift = 0;    // Right-shift after masking
    int32_t mul = 1;
    int32_t div = 1;
    int32_t add = 0;
};

struct ObdDisplayConfig {
    uint16_t maxRpm = 7000;
    uint16_t redlineRpm = 5500;
    MetricId centerSlot = MetricId::GEAR;
    MetricId bottomLeftSlot = MetricId::COOLANT_TEMP;
    MetricId bottomRightSlot = MetricId::VOLTAGE;
    MetricId bottomCenterSlot = MetricId::NONE;
};

struct ObdConfig {
    uint8_t version = 1;
    ObdProtocol protocol = ObdProtocol::AUTO;
    char elmName[24] = "";
    ObdDisplayConfig display;
    ObdParamConfig params[MAX_OBD_PARAMS];
    uint8_t paramCount = 0;
};

ObdConfig makeDefaultObdConfig();
bool parseObdConfigJson(const char* json, ObdConfig& out);
bool loadObdConfigFromNvs(ObdConfig& out);
bool saveObdConfigToNvs(const char* json);

// Evaluate raw payload bytes (after service+PID echo) using param formula
float evalObdFormula(const ObdParamConfig& param, const uint8_t* data, size_t len, bool* ok = nullptr);
void applyObdMetricValue(BikeTelemetry& bike, const ObdParamConfig& param, float val);
const char* defaultMetricLabel(MetricId id);
const char* defaultMetricUnit(MetricId id);
