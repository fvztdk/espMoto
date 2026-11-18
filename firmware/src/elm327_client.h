#pragma once

#include "nav_state.h"
#include "obd_config.h"

class Elm327Client {
public:
    void begin(BikeTelemetry* telemetry, const ObdConfig* config);
    void applyConfig(const ObdConfig& config);
    void loop();
    bool isConnected() const;

private:
    enum class State : uint8_t {
        IDLE,
        SCANNING,
        CONNECTING,
        INIT_ELM,
        POLLING,
        WAIT_RESPONSE,
        RECONNECT_WAIT
    };

    BikeTelemetry* _telemetry = nullptr;
    ObdConfig _config;
    State _state = State::IDLE;
    uint32_t _stateTs = 0;
    uint32_t _lastCmdMs = 0;
    uint8_t _initStep = 0;
    uint8_t _pollIdx = 0;
    uint16_t _pollCycle = 0;
    char _activeHeader[8] = "";
    bool _pendingHeaderSwitch = false;
    uint8_t _pidFailCount[MAX_OBD_PARAMS] = {};

    std::string _rxBuffer;
    bool _responseReady = false;

    void startScan();
    void sendCommand(const char* cmd);
    void handleResponse(const std::string& resp);
    bool parseHexResponse(const char* reqHex, const std::string& resp, uint8_t* outData, size_t& outLen);
    int nextEnabledParamIndex();
};
