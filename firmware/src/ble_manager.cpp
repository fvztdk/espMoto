#include "ble_manager.h"
#include "ble_protocol.h"
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_system.h>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {
constexpr const char* PAIR_NVS_NAMESPACE = "espmoto_pair";
constexpr const char* PAIR_NVS_KEY_PAIRED = "paired";
constexpr const char* PAIR_NVS_KEY_TOKEN  = "token";

bool g_phoneConnected = false;
bool g_paired = false;
bool g_sessionAuthenticated = false;
bool g_justPaired = false;
std::string g_pairToken;
NimBLEServer* g_server = nullptr;
NavState* g_navState = nullptr;
ObdConfig* g_obdConfig = nullptr;
Elm327Client* g_elmClient = nullptr;
std::string g_cfgRxBuffer;

std::string generateRandomToken() {
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%08X%08X", r1, r2);
    return std::string(buf);
}

std::string trimAndUpper(std::string val) {
    while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' ' || val.back() == '\t' || val.back() == '\0')) {
        val.pop_back();
    }
    size_t start = 0;
    while (start < val.size() && (val[start] == ' ' || val[start] == '\t')) {
        ++start;
    }
    val = val.substr(start);
    if (val.rfind("PAIR:", 0) == 0 || val.rfind("pair:", 0) == 0) {
        val = val.substr(5);
    }
    for (char& c : val) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return val;
}

void savePairingToNvs(bool paired, const std::string& token) {
    Preferences prefs;
    if (prefs.begin(PAIR_NVS_NAMESPACE, false)) {
        prefs.putBool(PAIR_NVS_KEY_PAIRED, paired);
        prefs.putString(PAIR_NVS_KEY_TOKEN, token.c_str());
        prefs.end();
    }
}

void loadPairingFromNvs() {
    Preferences prefs;
    bool paired = false;
    std::string token;
    if (prefs.begin(PAIR_NVS_NAMESPACE, true)) {
        paired = prefs.getBool(PAIR_NVS_KEY_PAIRED, false);
        String saved = prefs.getString(PAIR_NVS_KEY_TOKEN, "");
        token = saved.c_str();
        prefs.end();
    }

    if (token.size() != 16) {
        token = generateRandomToken();
        paired = false;
        savePairingToNvs(paired, token);
    }

    g_paired = paired;
    g_pairToken = token;
}

// Parse "<active>|<maneuver>|<distance>|<street>|<eta>"
void parseNavPayload(const std::string& raw) {
    if (!g_navState) return;

    String s(raw.c_str());
    int p1 = s.indexOf('|');
    int p2 = s.indexOf('|', p1 + 1);
    int p3 = s.indexOf('|', p2 + 1);
    int p4 = s.indexOf('|', p3 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) {
        return;
    }

    g_navState->active = (s.substring(0, p1).toInt() != 0);
    int mId = s.substring(p1 + 1, p2).toInt();
    g_navState->maneuver = static_cast<ManeuverType>(mId);
    g_navState->distance = s.substring(p2 + 1, p3);
    g_navState->street = s.substring(p3 + 1, p4);
    g_navState->eta = s.substring(p4 + 1);
    g_navState->lastUpdateMs = millis();
}

void processObdConfigChunk(const std::string& chunk) {
    if (chunk.empty()) return;

    // Guard against runaway buffer on malformed writes
    if (g_cfgRxBuffer.size() + chunk.size() > 4096) {
        g_cfgRxBuffer.clear();
    }
    g_cfgRxBuffer.append(chunk);

    const bool hasNewline = (g_cfgRxBuffer.find('\n') != std::string::npos);
    const bool singleCompleteJson = (!g_cfgRxBuffer.empty() && g_cfgRxBuffer.front() == '{' && g_cfgRxBuffer.back() == '}');
    if (!hasNewline && !singleCompleteJson) {
        return;
    }

    // Strip trailing newline(s)
    while (!g_cfgRxBuffer.empty() && (g_cfgRxBuffer.back() == '\n' || g_cfgRxBuffer.back() == '\r')) {
        g_cfgRxBuffer.pop_back();
    }

    ObdConfig nextCfg;
    if (parseObdConfigJson(g_cfgRxBuffer.c_str(), nextCfg)) {
        if (g_obdConfig) {
            *g_obdConfig = nextCfg;
        }
        saveObdConfigToNvs(g_cfgRxBuffer.c_str());
        if (g_elmClient) {
            g_elmClient->applyConfig(nextCfg);
        }
    }
    g_cfgRxBuffer.clear();
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& /*connInfo*/) override {
        g_phoneConnected = true;
        g_sessionAuthenticated = false;
    }

    void onDisconnect(NimBLEServer* /*pServer*/, NimBLEConnInfo& /*connInfo*/, int /*reason*/) override {
        g_phoneConnected = false;
        g_sessionAuthenticated = false;
        g_cfgRxBuffer.clear();
        if (g_navState) {
            g_navState->active = false;
        }
        NimBLEDevice::startAdvertising();
    }
};

class PairCharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& /*connInfo*/) override {
        std::string val = trimAndUpper(pCharacteristic->getValue());
        if (!val.empty() && val == g_pairToken) {
            g_sessionAuthenticated = true;
            if (!g_paired) {
                g_paired = true;
                g_justPaired = true;
                savePairingToNvs(true, g_pairToken);
            }
            pCharacteristic->setValue("OK");
        } else {
            g_sessionAuthenticated = false;
            pCharacteristic->setValue("ERR");
        }
    }
};

class NavCharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& /*connInfo*/) override {
        if (!g_sessionAuthenticated) return;
        std::string val = pCharacteristic->getValue();
        if (!val.empty()) {
            parseNavPayload(val);
        }
    }
};

class ObdConfigCharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& /*connInfo*/) override {
        if (!g_sessionAuthenticated) return;
        std::string val = pCharacteristic->getValue();
        if (!val.empty()) {
            processObdConfigChunk(val);
        }
    }
};
} // namespace

void BleManager::begin(NavState* navState, BikeTelemetry* telemetry) {
    _navState = navState;
    _telemetry = telemetry;
    g_navState = navState;

    if (!loadObdConfigFromNvs(_obdConfig)) {
        _obdConfig = makeDefaultObdConfig();
    }
    g_obdConfig = &_obdConfig;
    g_elmClient = &_elmClient;

    loadPairingFromNvs();

    NimBLEDevice::init(BleProtocol::DEVICE_NAME);
    NimBLEDevice::setMTU(517);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    _bleMac = NimBLEDevice::getAddress().toString();
    for (char& c : _bleMac) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    NimBLEServer* server = NimBLEDevice::createServer();
    g_server = server;
    server->setCallbacks(new ServerCallbacks());

    NimBLEService* navService = server->createService(BleProtocol::NAV_SERVICE_UUID);
    NimBLECharacteristic* navChar = navService->createCharacteristic(
        BleProtocol::NAV_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    navChar->setCallbacks(new NavCharCallbacks());

    NimBLECharacteristic* obdCfgChar = navService->createCharacteristic(
        BleProtocol::OBD_CONFIG_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::READ
    );
    obdCfgChar->setCallbacks(new ObdConfigCharCallbacks());

    NimBLECharacteristic* pairChar = navService->createCharacteristic(
        BleProtocol::PAIR_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ
    );
    pairChar->setCallbacks(new PairCharCallbacks());
    pairChar->setValue(g_paired ? "PAIRED" : "UNPAIRED");

    navService->start();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(BleProtocol::NAV_SERVICE_UUID);
    advertising->setName(BleProtocol::DEVICE_NAME);
    advertising->start();

    _elmClient.begin(telemetry, &_obdConfig);
}

void BleManager::loop() {
    _elmClient.loop();
}

bool BleManager::isPhoneConnected() const {
    return g_phoneConnected;
}

bool BleManager::isPaired() const {
    return g_paired;
}

bool BleManager::consumeJustPaired() {
    bool v = g_justPaired;
    g_justPaired = false;
    return v;
}

void BleManager::resetPairing() {
    g_paired = false;
    g_sessionAuthenticated = false;
    g_justPaired = false;
    g_pairToken = generateRandomToken();
    savePairingToNvs(false, g_pairToken);

    if (g_server) {
        auto peers = g_server->getPeerDevices();
        for (uint16_t connHandle : peers) {
            g_server->disconnect(connHandle);
        }
    }
    NimBLEDevice::startAdvertising();
}

std::string BleManager::getPairingQrPayload() const {
    return std::string("espmoto://pair?mac=") + _bleMac +
           "&token=" + g_pairToken +
           "&name=" + BleProtocol::DEVICE_NAME;
}
