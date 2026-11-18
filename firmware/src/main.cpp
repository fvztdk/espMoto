#include <M5Unified.h>
#include <lvgl.h>
#include "ble_manager.h"
#include "nav_state.h"
#include "ui_renderer.h"

namespace {
constexpr int32_t SCREEN_SIZE = 466;
constexpr int32_t DRAW_BUF_LINES = 46;

NavState g_nav;
BikeTelemetry g_bike;
BleManager g_ble;
UiRenderer g_ui;

uint16_t g_drawBuf[SCREEN_SIZE * DRAW_BUF_LINES];

bool g_demoMode = true;
bool g_btnBLongPressHandled = false;
uint32_t g_lastRenderMs = 0;
uint32_t g_lastDemoTickMs = 0;

void updateUiPairingQr() {
    std::string qr = g_ble.getPairingQrPayload();
    std::string sub = std::string("espMoto • ") + g_ble.getBleMac();
    g_ui.setPairingPayload(qr.c_str(), sub.c_str());
}

void onUiResetPairing(void* /*userData*/) {
    g_ble.resetPairing();
    updateUiPairingQr();
}

void flushDisplay(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
    M5.Display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<uint16_t*>(px_map));
    lv_display_flush_ready(disp);
}

void readTouch(lv_indev_t* /*indev*/, lv_indev_data_t* data) {
    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail(0);
        if (t.isPressed()) {
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = t.x;
            data->point.y = t.y;
            return;
        }
    }
    data->state = LV_INDEV_STATE_RELEASED;
}

void updateDemoData() {
    uint32_t now = millis();
    if (now - g_lastDemoTickMs < 150) return;
    g_lastDemoTickMs = now;

    // Simulate motorcycle idle/cruise telemetry
    static uint16_t phase = 0;
    phase = (phase + 1) % 60;
    g_bike.rpm = 1100 + static_cast<uint16_t>(abs(30 - phase) * 95);
    g_bike.speedKmh = (g_bike.rpm > 1500) ? (g_bike.rpm - 1200) / 38 : 0;
    g_bike.gear = (g_bike.speedKmh == 0) ? 0 : constrain(g_bike.speedKmh / 22 + 1, 1, 5);
    g_bike.engineTempC = 92;
    g_bike.batteryVolts = 13.8f;

    if (!g_ble.isPhoneConnected()) {
        g_nav.active = false;
        g_nav.maneuver = ManeuverType::TURN_RIGHT;
        g_nav.distance = "250 m";
        g_nav.street = "Passeig de Gràcia";
        g_nav.eta = "18:15";
    }
}
} // namespace

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setBrightness(220);

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });

    lv_display_t* disp = lv_display_create(SCREEN_SIZE, SCREEN_SIZE);
    lv_display_set_buffers(disp, g_drawBuf, nullptr, sizeof(g_drawBuf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flushDisplay);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, readTouch);

    g_ui.begin();
    g_ble.begin(&g_nav, &g_bike);
    g_ui.setResetPairingCallback(&onUiResetPairing, nullptr);
    updateUiPairingQr();
    if (!g_ble.isPaired()) {
        g_ui.setPage(ScreenPage::PAIRING);
    }
}

void loop() {
    M5.update();
    g_ble.loop();

    if (g_ble.consumeJustPaired()) {
        g_ui.setPage(g_nav.active ? ScreenPage::NAVIGATION : ScreenPage::TELEMETRY);
    }

    // Physical button A switches between Nav and Telemetry pages (screen tap handled by LVGL)
    if (M5.BtnA.wasClicked()) {
        if (g_ui.currentPage() == ScreenPage::NAVIGATION || g_ui.currentPage() == ScreenPage::TELEMETRY) {
            g_ui.nextPage();
        }
    }

    // Physical button B: 10s hold opens Settings menu; short click toggles Demo Mode
    if (M5.BtnB.pressedFor(10000)) {
        if (!g_btnBLongPressHandled) {
            g_btnBLongPressHandled = true;
            g_ui.setPage(ScreenPage::SETTINGS);
        }
    } else if (!M5.BtnB.isPressed()) {
        g_btnBLongPressHandled = false;
    }

    if (M5.BtnB.wasClicked() && !g_btnBLongPressHandled) {
        g_demoMode = !g_demoMode;
    }

    if (g_demoMode) {
        updateDemoData();
    }

    uint32_t now = millis();
    if (now - g_lastRenderMs >= 50) { // ~20 FPS state refresh
        g_lastRenderMs = now;
        g_ui.render(g_nav, g_bike, g_ble.isPhoneConnected(), g_demoMode, &g_ble.getObdConfig());
    }

    lv_timer_handler();
    delay(5);
}
