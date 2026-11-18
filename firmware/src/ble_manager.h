#pragma once

#include <string>
#include "nav_state.h"
#include "obd_config.h"
#include "elm327_client.h"

class BleManager {
public:
    void begin(NavState* navState, BikeTelemetry* telemetry);
    void loop();
    bool isPhoneConnected() const;
    bool isPaired() const;
    bool consumeJustPaired();
    void resetPairing();
    std::string getPairingQrPayload() const;
    const std::string& getBleMac() const { return _bleMac; }
    const ObdConfig& getObdConfig() const { return _obdConfig; }

private:
    NavState* _navState = nullptr;
    BikeTelemetry* _telemetry = nullptr;
    ObdConfig _obdConfig;
    Elm327Client _elmClient;
    std::string _bleMac;
};
