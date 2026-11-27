#include "elm327_client.h"

#include <NimBLEDevice.h>
#include <cctype>
#include <cstring>

namespace {

// Common BLE ELM327 GATT Service UUIDs (Vgate iCar Pro, OBDLink CX, Veepeak, etc.)
const NimBLEUUID ELM_SVC_FFF0("0000fff0-0000-1000-8000-00805f9b34fb");
const NimBLEUUID ELM_CHR_FFF1("0000fff1-0000-1000-8000-00805f9b34fb"); // Notify
const NimBLEUUID ELM_CHR_FFF2("0000fff2-0000-1000-8000-00805f9b34fb"); // Write

const NimBLEUUID ELM_SVC_FFE0("0000ffe0-0000-1000-8000-00805f9b34fb");
const NimBLEUUID ELM_CHR_FFE1("0000ffe1-0000-1000-8000-00805f9b34fb"); // Write + Notify

const NimBLEUUID ELM_SVC_18F0("000018f0-0000-1000-8000-00805f9b34fb");
const NimBLEUUID ELM_CHR_2AF0("00002af0-0000-1000-8000-00805f9b34fb"); // Notify
const NimBLEUUID ELM_CHR_2AF1("00002af1-0000-1000-8000-00805f9b34fb"); // Write

NimBLEClient* g_elmClient = nullptr;
NimBLERemoteCharacteristic* g_txChar = nullptr;
NimBLERemoteCharacteristic* g_rxChar = nullptr;
bool g_elmConnected = false;
bool g_deviceFound = false;
NimBLEAdvertisedDevice g_targetDevice;

std::string* g_rxBufPtr = nullptr;
bool* g_respReadyPtr = nullptr;

void onElmNotify(NimBLERemoteCharacteristic* /*chr*/, uint8_t* data, size_t len, bool /*isNotify*/) {
    if (!g_rxBufPtr || !g_respReadyPtr || !data || len == 0) return;
    for (size_t i = 0; i < len; ++i) {
        char c = static_cast<char>(data[i]);
        if (c == '>') {
            *g_respReadyPtr = true;
        } else if (c != '\r' && c != '\n') {
            g_rxBufPtr->push_back(c);
        }
    }
}

class ElmClientCallbacks : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient* /*pClient*/, int /*reason*/) override {
        g_elmConnected = false;
        g_txChar = nullptr;
        g_rxChar = nullptr;
    }
};

bool isElmNameMatch(const std::string& devName, const char* filter) {
    if (filter && filter[0] != '\0') {
        return devName.find(filter) != std::string::npos;
    }
    return devName.find("OBD") != std::string::npos ||
           devName.find("ELM") != std::string::npos ||
           devName.find("V-LINK") != std::string::npos ||
           devName.find("IOS-Vlink") != std::string::npos ||
           devName.find("Veepeak") != std::string::npos;
}

class ElmScanCallbacks : public NimBLEScanCallbacks {
public:
    const char* nameFilter = "";
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        if (!advertisedDevice) return;
        bool matchUuid = advertisedDevice->isAdvertisingService(ELM_SVC_FFF0) ||
                         advertisedDevice->isAdvertisingService(ELM_SVC_FFE0) ||
                         advertisedDevice->isAdvertisingService(ELM_SVC_18F0);
        bool matchName = isElmNameMatch(advertisedDevice->getName(), nameFilter);
        if (matchUuid || matchName) {
            g_targetDevice = *advertisedDevice;
            g_deviceFound = true;
            NimBLEDevice::getScan()->stop();
        }
    }
};

ElmScanCallbacks g_scanCallbacks;
ElmClientCallbacks g_clientCallbacks;

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

bool isFastMetric(MetricId id) {
    return id == MetricId::RPM || id == MetricId::SPEED ||
           id == MetricId::GEAR || id == MetricId::THROTTLE;
}

} // namespace

void Elm327Client::begin(BikeTelemetry* telemetry, const ObdConfig* config) {
    _telemetry = telemetry;
    _config = config ? *config : makeDefaultObdConfig();
    g_rxBufPtr = &_rxBuffer;
    g_respReadyPtr = &_responseReady;
    _state = State::SCANNING;
    _stateTs = millis();
    startScan();
}

void Elm327Client::applyConfig(const ObdConfig& config) {
    bool protoChanged = (_config.protocol != config.protocol);
    _config = config;
    std::memset(_pidFailCount, 0, sizeof(_pidFailCount));
    _activeHeader[0] = '\0';
    _pendingHeaderSwitch = false;
    _pollIdx = 0;
    if (protoChanged && g_elmConnected) {
        _initStep = 0;
        _state = State::INIT_ELM;
    }
}

bool Elm327Client::isConnected() const {
    return g_elmConnected;
}

void Elm327Client::startScan() {
    g_deviceFound = false;
    g_scanCallbacks.nameFilter = _config.elmName;
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&g_scanCallbacks, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(50);
    scan->start(5000, false);
}

void Elm327Client::sendCommand(const char* cmd) {
    if (!g_txChar || !cmd) return;
    _rxBuffer.clear();
    _responseReady = false;
    std::string full(cmd);
    full.push_back('\r');
    g_txChar->writeValue(reinterpret_cast<const uint8_t*>(full.data()), full.size(), false);
    _lastCmdMs = millis();
}

int Elm327Client::nextEnabledParamIndex() {
    if (_config.paramCount == 0) return -1;
    for (uint8_t step = 0; step < _config.paramCount; ++step) {
        uint8_t idx = (_pollIdx + step) % _config.paramCount;
        const ObdParamConfig& p = _config.params[idx];
        if (!p.enabled || p.requestHex[0] == '\0') continue;
        // Back off PIDs that returned 7F / NO DATA repeatedly (poll every 20th cycle)
        if (_pidFailCount[idx] >= 3 && (_pollCycle % 20) != 0) continue;
        // Slow metrics (Temp, Voltage, Intake) polled every 5th cycle
        if (!isFastMetric(p.id) && (_pollCycle % 5) != 0) continue;
        _pollIdx = (idx + 1) % _config.paramCount;
        if (_pollIdx == 0) ++_pollCycle;
        return static_cast<int>(idx);
    }
    ++_pollCycle;
    return -1;
}

bool Elm327Client::parseHexResponse(const char* reqHex, const std::string& resp, uint8_t* outData, size_t& outLen) {
    outLen = 0;
    if (!reqHex || resp.empty()) return false;

    // Strip spaces and uppercase into compact hex string
    std::string clean;
    clean.reserve(resp.size());
    for (char c : resp) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            clean.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }

    if (clean.find("NODATA") != std::string::npos ||
        clean.find("ERROR") != std::string::npos ||
        clean.find('?') != std::string::npos) {
        return false;
    }

    // UDS Negative Response: 7F <service> <NRC>
    if (clean.rfind("7F", 0) == 0 && clean.size() >= 6) {
        return false;
    }

    // Convert hex pairs to bytes
    uint8_t rawBytes[64];
    size_t rawCount = 0;
    for (size_t i = 0; i + 1 < clean.size() && rawCount < sizeof(rawBytes); i += 2) {
        int hi = hexVal(clean[i]);
        int lo = hexVal(clean[i + 1]);
        if (hi < 0 || lo < 0) break;
        rawBytes[rawCount++] = static_cast<uint8_t>((hi << 4) | lo);
    }

    // Determine echo prefix length (Service + 0x40 + PID bytes)
    size_t reqHexLen = std::strlen(reqHex);
    size_t echoBytes = reqHexLen / 2;
    if (rawCount <= echoBytes) return false;

    // Verify positive service response byte (reqService + 0x40)
    int reqSvcHi = hexVal(reqHex[0]);
    int reqSvcLo = hexVal(reqHex[1]);
    if (reqSvcHi >= 0 && reqSvcLo >= 0) {
        uint8_t expectedSvc = static_cast<uint8_t>(((reqSvcHi << 4) | reqSvcLo) + 0x40);
        if (rawBytes[0] != expectedSvc) {
            return false;
        }
    }

    outLen = rawCount - echoBytes;
    std::memcpy(outData, rawBytes + echoBytes, outLen);
    return true;
}

void Elm327Client::handleResponse(const std::string& resp) {
    if (_state == State::INIT_ELM) {
        ++_initStep;
        if (_initStep >= 7) {
            _state = State::POLLING;
        }
        return;
    }

    if (_state == State::WAIT_RESPONSE) {
        if (_pendingHeaderSwitch) {
            _pendingHeaderSwitch = false;
            _state = State::POLLING;
            return;
        }

        const uint8_t lastIdx = (_pollIdx == 0) ? (_config.paramCount - 1) : (_pollIdx - 1);
        if (lastIdx < _config.paramCount) {
            const ObdParamConfig& p = _config.params[lastIdx];
            uint8_t data[32];
            size_t dataLen = 0;
            if (parseHexResponse(p.requestHex, resp, data, dataLen)) {
                bool ok = false;
                float val = evalObdFormula(p, data, dataLen, &ok);
                if (ok && _telemetry) {
                    applyObdMetricValue(*_telemetry, p, val);
                    _pidFailCount[lastIdx] = 0;
                }
            } else {
                if (_pidFailCount[lastIdx] < 255) {
                    ++_pidFailCount[lastIdx];
                }
                if (_telemetry && _pidFailCount[lastIdx] >= 3) {
                    _telemetry->setValid(p.id, false);
                }
            }
        }
        _state = State::POLLING;
    }
}

void Elm327Client::loop() {
    uint32_t now = millis();

    if (_telemetry) {
        if (g_elmConnected) {
            _telemetry->simOverride = false;
            _telemetry->obdConnected = true;
        } else if (_telemetry->simOverride) {
            if (now - _telemetry->lastSimUpdateMs > 5000U) {
                _telemetry->simOverride = false;
                _telemetry->obdConnected = false;
                _telemetry->validMask = 0;
            }
        } else {
            _telemetry->obdConnected = false;
        }
    }

    switch (_state) {
        case State::IDLE:
            break;

        case State::SCANNING:
            if (g_deviceFound) {
                _state = State::CONNECTING;
                _stateTs = now;
            } else if (now - _stateTs > 6000) {
                _state = State::RECONNECT_WAIT;
                _stateTs = now;
            }
            break;

        case State::CONNECTING: {
            if (!g_elmClient) {
                g_elmClient = NimBLEDevice::createClient();
                g_elmClient->setClientCallbacks(&g_clientCallbacks, false);
            }
            if (g_elmClient->connect(&g_targetDevice)) {
                NimBLERemoteService* svc = g_elmClient->getService(ELM_SVC_FFF0);
                if (svc) {
                    g_rxChar = svc->getCharacteristic(ELM_CHR_FFF1);
                    g_txChar = svc->getCharacteristic(ELM_CHR_FFF2);
                }
                if (!svc || !g_txChar || !g_rxChar) {
                    svc = g_elmClient->getService(ELM_SVC_FFE0);
                    if (svc) {
                        g_rxChar = svc->getCharacteristic(ELM_CHR_FFE1);
                        g_txChar = g_rxChar;
                    }
                }
                if (!svc || !g_txChar || !g_rxChar) {
                    svc = g_elmClient->getService(ELM_SVC_18F0);
                    if (svc) {
                        g_rxChar = svc->getCharacteristic(ELM_CHR_2AF0);
                        g_txChar = svc->getCharacteristic(ELM_CHR_2AF1);
                    }
                }

                if (g_txChar && g_rxChar && g_rxChar->canNotify()) {
                    g_rxChar->subscribe(true, onElmNotify);
                    g_elmConnected = true;
                    _initStep = 0;
                    _state = State::INIT_ELM;
                    _lastCmdMs = 0;
                    break;
                }
                g_elmClient->disconnect();
            }
            _state = State::RECONNECT_WAIT;
            _stateTs = now;
            break;
        }

        case State::INIT_ELM: {
            if (!g_elmConnected) {
                _state = State::RECONNECT_WAIT;
                _stateTs = now;
                break;
            }
            if (_responseReady) {
                _responseReady = false;
                handleResponse(_rxBuffer);
            } else if (_lastCmdMs == 0 || (now - _lastCmdMs > 1200)) {
                char protoCmd[12];
                std::snprintf(protoCmd, sizeof(protoCmd), "ATSP%u", static_cast<unsigned>(_config.protocol));
                const char* cmds[7] = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATAT1", protoCmd};
                if (_initStep < 7) {
                    sendCommand(cmds[_initStep]);
                } else {
                    _state = State::POLLING;
                }
            }
            break;
        }

        case State::POLLING: {
            if (!g_elmConnected) {
                _state = State::RECONNECT_WAIT;
                _stateTs = now;
                break;
            }
            if (now - _lastCmdMs < 40) break;

            int idx = nextEnabledParamIndex();
            if (idx < 0) break;

            const ObdParamConfig& p = _config.params[idx];
            if (std::strcmp(_activeHeader, p.header) != 0) {
                std::strncpy(_activeHeader, p.header, sizeof(_activeHeader) - 1);
                _activeHeader[sizeof(_activeHeader) - 1] = '\0';
                _pendingHeaderSwitch = true;
                if (p.header[0] != '\0') {
                    char hdrCmd[16];
                    std::snprintf(hdrCmd, sizeof(hdrCmd), "ATSH%s", p.header);
                    sendCommand(hdrCmd);
                } else {
                    sendCommand("ATSH7DF");
                }
                _state = State::WAIT_RESPONSE;
                break;
            }

            sendCommand(p.requestHex);
            _state = State::WAIT_RESPONSE;
            break;
        }

        case State::WAIT_RESPONSE:
            if (!g_elmConnected) {
                _state = State::RECONNECT_WAIT;
                _stateTs = now;
                break;
            }
            if (_responseReady) {
                _responseReady = false;
                handleResponse(_rxBuffer);
            } else if (now - _lastCmdMs > 800) {
                _state = State::POLLING;
            }
            break;

        case State::RECONNECT_WAIT:
            if (now - _stateTs > 4000) {
                _state = State::SCANNING;
                _stateTs = now;
                startScan();
            }
            break;
    }
}
