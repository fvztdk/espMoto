#include "obd_config.h"

#include <Preferences.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* NVS_NAMESPACE = "espmoto_obd";
constexpr const char* NVS_KEY_JSON  = "cfg_json";

void safeCopy(char* dst, size_t dstSize, const char* src) {
    if (!dst || dstSize == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

ObdParamConfig makeParam(MetricId id,
                         bool enabled,
                         const char* reqHex,
                         uint8_t byteLen,
                         int32_t mul,
                         int32_t div,
                         int32_t add) {
    ObdParamConfig p;
    p.id = id;
    p.enabled = enabled;
    safeCopy(p.label, sizeof(p.label), defaultMetricLabel(id));
    safeCopy(p.unit, sizeof(p.unit), defaultMetricUnit(id));
    safeCopy(p.requestHex, sizeof(p.requestHex), reqHex);
    p.byteOffset = 0;
    p.byteLength = byteLen;
    p.mul = mul;
    p.div = (div == 0) ? 1 : div;
    p.add = add;
    return p;
}

const char* skipWs(const char* p) {
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        ++p;
    }
    return p;
}

bool extractJsonInt(const char* objStart, const char* objEnd, const char* key, int32_t& out) {
    char pattern[32];
    std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const size_t patLen = std::strlen(pattern);
    const char* cur = objStart;
    while (cur && cur < objEnd) {
        const char* found = std::strstr(cur, pattern);
        if (!found || found >= objEnd) return false;
        const char* p = skipWs(found + patLen);
        if (*p == ':') {
            p = skipWs(p + 1);
            char* endPtr = nullptr;
            long val = std::strtol(p, &endPtr, 10);
            if (endPtr != p) {
                out = static_cast<int32_t>(val);
                return true;
            }
            return false;
        }
        cur = found + patLen;
    }
    return false;
}

bool extractJsonString(const char* objStart, const char* objEnd, const char* key, char* dst, size_t dstSize) {
    char pattern[32];
    std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const size_t patLen = std::strlen(pattern);
    const char* cur = objStart;
    while (cur && cur < objEnd) {
        const char* found = std::strstr(cur, pattern);
        if (!found || found >= objEnd) return false;
        const char* p = skipWs(found + patLen);
        if (*p == ':') {
            p = skipWs(p + 1);
            if (*p != '"') return false;
            ++p;
            size_t i = 0;
            while (p < objEnd && *p != '"' && *p != '\0') {
                if (*p == '\\' && (p + 1) < objEnd) {
                    ++p;
                }
                if (i + 1 < dstSize) {
                    dst[i++] = *p;
                }
                ++p;
            }
            dst[i] = '\0';
            return true;
        }
        cur = found + patLen;
    }
    return false;
}

bool extractJsonSlots(const char* objStart, const char* objEnd, ObdDisplayConfig& display) {
    const char* keyPos = std::strstr(objStart, "\"slots\"");
    if (!keyPos || keyPos >= objEnd) return false;
    const char* p = std::strchr(keyPos + 7, '[');
    if (!p || p >= objEnd) return false;
    ++p;

    int32_t vals[4] = {
        static_cast<int32_t>(display.centerSlot),
        static_cast<int32_t>(display.bottomLeftSlot),
        static_cast<int32_t>(display.bottomRightSlot),
        static_cast<int32_t>(display.bottomCenterSlot)
    };
    for (int i = 0; i < 4; ++i) {
        p = skipWs(p);
        if (p >= objEnd || *p == ']') break;
        char* endPtr = nullptr;
        long v = std::strtol(p, &endPtr, 10);
        if (endPtr != p) {
            vals[i] = static_cast<int32_t>(v);
            p = endPtr;
        }
        p = skipWs(p);
        if (*p == ',') ++p;
    }
    display.centerSlot       = static_cast<MetricId>(vals[0]);
    display.bottomLeftSlot   = static_cast<MetricId>(vals[1]);
    display.bottomRightSlot  = static_cast<MetricId>(vals[2]);
    display.bottomCenterSlot = static_cast<MetricId>(vals[3]);
    return true;
}

} // namespace

const char* defaultMetricLabel(MetricId id) {
    switch (id) {
        case MetricId::RPM:              return "RPM";
        case MetricId::SPEED:            return "SPD";
        case MetricId::COOLANT_TEMP:     return "TEMP";
        case MetricId::VOLTAGE:          return "VOLT";
        case MetricId::THROTTLE:         return "TPS";
        case MetricId::INTAKE_TEMP:      return "IAT";
        case MetricId::GEAR:             return "GEAR";
        case MetricId::OIL_PRESSURE:     return "OIL";
        case MetricId::LEAN_ANGLE:       return "LEAN";
        case MetricId::CLUTCH_KICKSTAND: return "CLCH";
        case MetricId::ABS_TC:           return "ABS";
        default:                         return "--";
    }
}

const char* defaultMetricUnit(MetricId id) {
    switch (id) {
        case MetricId::RPM:          return "";
        case MetricId::SPEED:        return "km/h";
        case MetricId::COOLANT_TEMP: return "\xC2\xB0" "C";
        case MetricId::VOLTAGE:      return "V";
        case MetricId::THROTTLE:     return "%";
        case MetricId::INTAKE_TEMP:  return "\xC2\xB0" "C";
        case MetricId::GEAR:         return "";
        case MetricId::OIL_PRESSURE: return "bar";
        case MetricId::LEAN_ANGLE:   return "\xC2\xB0";
        default:                     return "";
    }
}

ObdConfig makeDefaultObdConfig() {
    ObdConfig cfg;
    cfg.version = 1;
    cfg.protocol = ObdProtocol::AUTO;
    cfg.elmName[0] = '\0';
    cfg.display.maxRpm = 7000;
    cfg.display.redlineRpm = 5500;
    cfg.display.centerSlot = MetricId::GEAR;
    cfg.display.bottomLeftSlot = MetricId::COOLANT_TEMP;
    cfg.display.bottomRightSlot = MetricId::VOLTAGE;
    cfg.display.bottomCenterSlot = MetricId::NONE;

    uint8_t idx = 0;
    cfg.params[idx++] = makeParam(MetricId::RPM,          true,  "010C", 2, 1,   4,    0);
    cfg.params[idx++] = makeParam(MetricId::SPEED,        true,  "010D", 1, 1,   1,    0);
    cfg.params[idx++] = makeParam(MetricId::COOLANT_TEMP, true,  "0105", 1, 1,   1,  -40);
    cfg.params[idx++] = makeParam(MetricId::THROTTLE,     false, "0111", 1, 100, 255,  0);
    cfg.params[idx++] = makeParam(MetricId::INTAKE_TEMP,  false, "010F", 1, 1,   1,  -40);
    cfg.params[idx++] = makeParam(MetricId::VOLTAGE,      true,  "0142", 2, 1,   1000, 0);
    cfg.paramCount = idx;
    return cfg;
}

bool parseObdConfigJson(const char* json, ObdConfig& out) {
    if (!json) return false;
    const size_t totalLen = std::strlen(json);
    if (totalLen < 2) return false;
    const char* docEnd = json + totalLen;

    ObdConfig parsed = makeDefaultObdConfig();

    int32_t tmp = 0;
    if (extractJsonInt(json, docEnd, "v", tmp)) {
        parsed.version = static_cast<uint8_t>(tmp);
    }
    if (extractJsonInt(json, docEnd, "proto", tmp)) {
        parsed.protocol = static_cast<ObdProtocol>(tmp);
    }
    extractJsonString(json, docEnd, "elmName", parsed.elmName, sizeof(parsed.elmName));

    if (extractJsonInt(json, docEnd, "maxRpm", tmp) && tmp >= 1000 && tmp <= 20000) {
        parsed.display.maxRpm = static_cast<uint16_t>(tmp);
    }
    if (extractJsonInt(json, docEnd, "redline", tmp) && tmp >= 1000 && tmp <= 20000) {
        parsed.display.redlineRpm = static_cast<uint16_t>(tmp);
    }
    extractJsonSlots(json, docEnd, parsed.display);

    const char* pidsKey = std::strstr(json, "\"pids\"");
    if (pidsKey) {
        const char* arrStart = std::strchr(pidsKey + 6, '[');
        if (arrStart) {
            const char* arrEnd = std::strchr(arrStart, ']');
            if (!arrEnd) arrEnd = docEnd;

            parsed.paramCount = 0;
            const char* cursor = arrStart + 1;
            while (cursor < arrEnd && parsed.paramCount < MAX_OBD_PARAMS) {
                const char* objStart = std::strchr(cursor, '{');
                if (!objStart || objStart >= arrEnd) break;
                const char* objEnd = std::strchr(objStart, '}');
                if (!objEnd || objEnd > arrEnd) break;

                ObdParamConfig p;
                int32_t v = 0;
                if (extractJsonInt(objStart, objEnd, "id", v)) {
                    p.id = static_cast<MetricId>(v);
                }
                safeCopy(p.label, sizeof(p.label), defaultMetricLabel(p.id));
                safeCopy(p.unit, sizeof(p.unit), defaultMetricUnit(p.id));

                if (extractJsonInt(objStart, objEnd, "en", v)) p.enabled = (v != 0);
                extractJsonString(objStart, objEnd, "req", p.requestHex, sizeof(p.requestHex));
                extractJsonString(objStart, objEnd, "hdr", p.header, sizeof(p.header));
                extractJsonString(objStart, objEnd, "lbl", p.label, sizeof(p.label));
                extractJsonString(objStart, objEnd, "unit", p.unit, sizeof(p.unit));

                if (extractJsonInt(objStart, objEnd, "off", v))  p.byteOffset = static_cast<uint8_t>(v);
                if (extractJsonInt(objStart, objEnd, "len", v))  p.byteLength = (v == 2) ? 2 : 1;
                if (extractJsonInt(objStart, objEnd, "mul", v))  p.mul = v;
                if (extractJsonInt(objStart, objEnd, "div", v))  p.div = (v == 0) ? 1 : v;
                if (extractJsonInt(objStart, objEnd, "add", v))  p.add = v;
                if (extractJsonInt(objStart, objEnd, "mask", v)) p.bitMask = static_cast<uint16_t>(v);
                if (extractJsonInt(objStart, objEnd, "shr", v))  p.bitShift = static_cast<uint8_t>(v);
                if (extractJsonInt(objStart, objEnd, "sgn", v))  p.isSigned = (v != 0);

                if (p.id != MetricId::NONE) {
                    parsed.params[parsed.paramCount++] = p;
                }
                cursor = objEnd + 1;
            }
        }
    }

    out = parsed;
    return true;
}

bool loadObdConfigFromNvs(ObdConfig& out) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        return false;
    }
    String saved = prefs.getString(NVS_KEY_JSON, "");
    prefs.end();
    if (saved.length() == 0) {
        return false;
    }
    return parseObdConfigJson(saved.c_str(), out);
}

bool saveObdConfigToNvs(const char* json) {
    if (!json) return false;
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        return false;
    }
    size_t written = prefs.putString(NVS_KEY_JSON, json);
    prefs.end();
    return written > 0;
}

float evalObdFormula(const ObdParamConfig& param, const uint8_t* data, size_t len, bool* ok) {
    if (!data || param.byteOffset >= len) {
        if (ok) *ok = false;
        return 0.0f;
    }
    const size_t needed = static_cast<size_t>(param.byteOffset) + param.byteLength;
    if (needed > len) {
        if (ok) *ok = false;
        return 0.0f;
    }

    int32_t raw = 0;
    if (param.byteLength == 2) {
        uint16_t u16 = (static_cast<uint16_t>(data[param.byteOffset]) << 8) |
                       static_cast<uint16_t>(data[param.byteOffset + 1]);
        if (param.bitMask != 0) {
            u16 = (u16 & param.bitMask) >> param.bitShift;
        }
        raw = param.isSigned ? static_cast<int16_t>(u16) : static_cast<int32_t>(u16);
    } else {
        uint8_t u8 = data[param.byteOffset];
        if (param.bitMask != 0) {
            u8 = static_cast<uint8_t>((u8 & static_cast<uint8_t>(param.bitMask)) >> param.bitShift);
        }
        raw = param.isSigned ? static_cast<int8_t>(u8) : static_cast<int32_t>(u8);
    }

    const int32_t divisor = (param.div == 0) ? 1 : param.div;
    if (ok) *ok = true;
    return (static_cast<float>(raw) * static_cast<float>(param.mul) / static_cast<float>(divisor)) +
           static_cast<float>(param.add);
}

void applyObdMetricValue(BikeTelemetry& bike, const ObdParamConfig& param, float val) {
    switch (param.id) {
        case MetricId::RPM:
            bike.rpm = (val < 0.0f) ? 0 : static_cast<uint16_t>(val + 0.5f);
            break;
        case MetricId::SPEED:
            bike.speedKmh = (val < 0.0f) ? 0 : static_cast<uint16_t>(val + 0.5f);
            break;
        case MetricId::COOLANT_TEMP:
            bike.engineTempC = static_cast<int16_t>(val >= 0.0f ? val + 0.5f : val - 0.5f);
            break;
        case MetricId::VOLTAGE:
            bike.batteryVolts = val;
            break;
        case MetricId::THROTTLE:
            bike.throttlePct = (val < 0.0f) ? 0 : (val > 100.0f ? 100 : static_cast<uint8_t>(val + 0.5f));
            break;
        case MetricId::INTAKE_TEMP:
            bike.intakeTempC = static_cast<int16_t>(val >= 0.0f ? val + 0.5f : val - 0.5f);
            break;
        case MetricId::GEAR:
            bike.gear = (val <= 0.0f) ? 0 : static_cast<int8_t>(val + 0.5f);
            break;
        case MetricId::OIL_PRESSURE:
            bike.oilPressureBar = val;
            break;
        case MetricId::LEAN_ANGLE:
            bike.leanAngleDeg = static_cast<int16_t>(val >= 0.0f ? val + 0.5f : val - 0.5f);
            break;
        case MetricId::CLUTCH_KICKSTAND:
            bike.clutchEngaged = (static_cast<int>(val + 0.5f) & 0x01) != 0;
            bike.kickstandDown = (static_cast<int>(val + 0.5f) & 0x02) != 0;
            break;
        case MetricId::ABS_TC:
            bike.absActive = (val > 0.5f);
            break;
        default:
            return;
    }
    bike.setValid(param.id, true);
    bike.lastUpdateMs = millis();
}
