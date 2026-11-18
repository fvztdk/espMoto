#pragma once

#include <lvgl.h>
#include "nav_state.h"
#include "obd_config.h"

enum class ScreenPage : uint8_t {
    NAVIGATION = 0,
    TELEMETRY  = 1,
    SETTINGS   = 2,
    PAIRING    = 3
};

class UiRenderer {
public:
    using ResetPairingCallback = void (*)(void* userData);

    void begin();
    void nextPage();
    void setPage(ScreenPage page);
    ScreenPage currentPage() const { return _page; }
    void setPairingPayload(const char* payload, const char* subtitle = nullptr);
    void setResetPairingCallback(ResetPairingCallback cb, void* userData = nullptr);
    void render(const NavState& nav,
                const BikeTelemetry& bike,
                bool phoneConnected,
                bool demoMode,
                const ObdConfig* obdConfig = nullptr);

private:
    ScreenPage _page = ScreenPage::TELEMETRY;
    ScreenPage _returnPage = ScreenPage::TELEMETRY;
    bool _initialized = false;

    // Root screen containers
    lv_obj_t* _navCont = nullptr;
    lv_obj_t* _telCont = nullptr;
    lv_obj_t* _settingsCont = nullptr;
    lv_obj_t* _pairingCont = nullptr;

    // Navigation page widgets
    lv_obj_t* _navBezelArc = nullptr;
    lv_obj_t* _navStatusPill = nullptr;
    lv_obj_t* _navStatusLabel = nullptr;
    lv_obj_t* _navIconBox = nullptr;
    lv_obj_t* _navRbtBaseRing = nullptr;
    lv_obj_t* _navRbtStub[3] = {nullptr, nullptr, nullptr};
    lv_obj_t* _navRbtExitLabel = nullptr;
    lv_obj_t* _navArrowRing = nullptr;
    lv_obj_t* _navArrowShaft = nullptr;
    lv_obj_t* _navArrowHead = nullptr;
    lv_obj_t* _navArrowAux = nullptr;
    lv_obj_t* _navDistLabel = nullptr;
    lv_obj_t* _navStreetLabel = nullptr;
    lv_obj_t* _navDivider = nullptr;
    lv_obj_t* _navEtaPill = nullptr;
    lv_obj_t* _navEtaLabel = nullptr;

    // Telemetry page widgets
    lv_obj_t* _telRpmArc = nullptr;
    lv_obj_t* _telInnerArc = nullptr;
    lv_obj_t* _telHeaderLabel = nullptr;
    lv_obj_t* _telUnitLabel = nullptr;
    lv_obj_t* _telGearBadge = nullptr;
    lv_obj_t* _telGearLabel = nullptr;
    lv_obj_t* _telRpmLabel = nullptr;
    lv_obj_t* _telTempLabel = nullptr;
    lv_obj_t* _telVoltLabel = nullptr;
    lv_obj_t* _telAuxLabel = nullptr;

    // Settings & Pairing widgets
    lv_obj_t* _pairingQr = nullptr;
    lv_obj_t* _pairingSubtitleLabel = nullptr;
    ResetPairingCallback _resetPairingCb = nullptr;
    void* _resetPairingUserData = nullptr;
    uint32_t _pressStartMs = 0;
    bool _isPressing = false;
    bool _longPressTriggered = false;

    // Persistent polyline buffers for LVGL line widgets
    lv_point_precise_t _shaftPts[8] = {};
    lv_point_precise_t _headPts[4] = {};
    lv_point_precise_t _auxPts[4] = {};
    lv_point_precise_t _rbtStubPts[3][2] = {};
    lv_point_precise_t _dividerPts[2] = {};

    // Cached state to avoid redundant widget updates
    ManeuverType _lastManeuver = ManeuverType::NONE;
    uint8_t _lastRoundaboutExit = 0;
    bool _lastNavActive = false;
    bool _lastAutoNavActive = false;
    bool _forceIconRefresh = true;
    uint16_t _lastMaxRpm = 0;

    void buildNavScreen(lv_obj_t* root);
    void buildTelemetryScreen(lv_obj_t* root);
    void buildSettingsScreen(lv_obj_t* root);
    void buildPairingScreen(lv_obj_t* root);
    void updatePageVisibility();
    void updateNavScreen(const NavState& nav, bool phoneConnected, bool demoMode);
    void updateTelemetryScreen(const BikeTelemetry& bike, bool demoMode, const ObdDisplayConfig& display);
    void updateManeuverIcon(ManeuverType type, uint8_t roundaboutExit, bool active);

    static void onScreenClicked(lv_event_t* e);
    static void onScreenTouchEvent(lv_event_t* e);
    static void onSettingsPairClicked(lv_event_t* e);
    static void onBackToMainClicked(lv_event_t* e);
};
