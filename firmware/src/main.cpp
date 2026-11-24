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

alignas(LV_DRAW_BUF_ALIGN) uint16_t g_drawBuf[SCREEN_SIZE * DRAW_BUF_LINES];

bool g_demoMode = true;
bool g_btnBLongPressHandled = false;
bool g_firstFrameLogged = false;
uint32_t g_lastRenderMs = 0;
uint32_t g_lastDemoTickMs = 0;

constexpr uint8_t M5PM1_I2C_ADDR = 0x6E;
constexpr uint8_t M5IOE1_I2C_ADDR = 0x4F;
constexpr uint32_t STOPWATCH_I2C_FREQ = 100000;

constexpr uint8_t M5PM1_REG_PWR_CFG = 0x06;
constexpr uint8_t M5PM1_REG_HOLD_CFG = 0x07;
constexpr uint8_t M5PM1_REG_I2C_CFG = 0x09;
constexpr uint8_t M5PM1_REG_WDT_CNT = 0x0A;
constexpr uint8_t M5PM1_REG_GPIO_MODE = 0x10;
constexpr uint8_t M5PM1_REG_GPIO_OUT = 0x11;
constexpr uint8_t M5PM1_REG_GPIO_DRV = 0x13;
constexpr uint8_t M5PM1_REG_GPIO_FUNC = 0x16;
constexpr uint8_t M5PM1_REG_BTN_CFG_1 = 0x49;
constexpr uint8_t M5PM1_REG_BTN_CFG_2 = 0x4A;

constexpr uint8_t M5IOE1_REG_GPIO_MODE_L = 0x03;
constexpr uint8_t M5IOE1_REG_GPIO_OUT_L = 0x05;
constexpr uint8_t M5IOE1_REG_GPIO_IN_L = 0x07;
constexpr uint8_t M5IOE1_REG_GPIO_DRV_L = 0x13;
constexpr uint8_t M5IOE1_REG_I2C_CFG = 0x23;
constexpr uint8_t M5IOE1_L3B_EN_MASK = (1u << 7); // Pin 8 (PB0-IO8): AMOLED L3B power enable

constexpr uint8_t DISPLAY_BRIGHTNESS = 220;

void updateUiPairingQr() {
    std::string qr = g_ble.getPairingQrPayload();
    std::string sub = std::string("espMoto • ") + g_ble.getBleMac();
    g_ui.setPairingPayload(qr.c_str(), sub.c_str());
}

void onUiResetPairing(void* /*userData*/) {
    g_ble.resetPairing();
    updateUiPairingQr();
}

void flushStopWatchBusBegin() {
    auto* panel = M5.Display.getPanel();
    auto* bus = panel ? panel->getBus() : nullptr;
    if (bus) {
        bus->beginTransaction();
    }
}

void flushStopWatchBusEnd() {
    auto* panel = M5.Display.getPanel();
    auto* bus = panel ? panel->getBus() : nullptr;
    if (bus) {
        bus->wait();
        int16_t cs = panel->config().pin_cs;
        if (cs >= 0) {
            lgfx::gpio_hi(cs);
        }
        bus->endTransaction();
    }
}

void flushDisplay(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
    flushStopWatchBusBegin();
    M5.Display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<const uint16_t*>(px_map));
    M5.Display.display();
    flushStopWatchBusEnd();
    if (!g_firstFrameLogged) {
        g_firstFrameLogged = true;
        Serial.printf("[espMoto] first frame flushed (%ld,%ld %lux%lu)\n",
                      (long)area->x1, (long)area->y1, (unsigned long)w, (unsigned long)h);
    }
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

void configureStopWatchPmic() {
    // Disable PM1 I2C idle sleep & watchdog
    M5.In_I2C.writeRegister8(M5PM1_I2C_ADDR, M5PM1_REG_I2C_CFG, 0x00, STOPWATCH_I2C_FREQ);
    M5.In_I2C.writeRegister8(M5PM1_I2C_ADDR, M5PM1_REG_I2C_CFG, 0x00, STOPWATCH_I2C_FREQ);
    M5.In_I2C.writeRegister8(M5PM1_I2C_ADDR, M5PM1_REG_WDT_CNT, 0x00, STOPWATCH_I2C_FREQ);

    // Hold LDO power config across power-off (matches M5StopWatch-UserDemo)
    M5.In_I2C.bitOn(M5PM1_I2C_ADDR, M5PM1_REG_HOLD_CFG, (1u << 5), STOPWATCH_I2C_FREQ);

    // Enable battery charging + 5V DCDC + 3.3V LDO, and keep redundant green PMIC LED off while AMOLED is active
    M5.In_I2C.bitOn(M5PM1_I2C_ADDR, M5PM1_REG_PWR_CFG, 0x07, STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOff(M5PM1_I2C_ADDR, M5PM1_REG_PWR_CFG, (1u << 4), STOPWATCH_I2C_FREQ);

    // Drive PM1 GPIO3 (CHG_PROG) LOW push-pull for active charge programming; GPIO2 (CHG_STAT) input
    M5.In_I2C.bitOff(M5PM1_I2C_ADDR, M5PM1_REG_GPIO_FUNC, (1u << 3) | (1u << 2), STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOn(M5PM1_I2C_ADDR, M5PM1_REG_GPIO_MODE, (1u << 3), STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOff(M5PM1_I2C_ADDR, M5PM1_REG_GPIO_MODE, (1u << 2), STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOff(M5PM1_I2C_ADDR, M5PM1_REG_GPIO_DRV, (1u << 3), STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOff(M5PM1_I2C_ADDR, M5PM1_REG_GPIO_OUT, (1u << 3), STOPWATCH_I2C_FREQ);

    // Button config matching M5StopWatch-UserDemo:
    // - CLK_DLY (bits 2:1) = 0b11 (1000ms click window so normal single-press reliably powers on from OFF)
    // - SINGLE_RST_DIS (bit 0) = 1 (disable single-click reset while ON)
    // - DL_LOCK (bit 7) = 0 (allow long-press Download mode for firmware flashing)
    // - DOUBLE_OFF_DIS (0x4A bit 0) = 0 (allow double-click power-off)
    M5.In_I2C.bitOn(M5PM1_I2C_ADDR, M5PM1_REG_BTN_CFG_1, (0x03u << 1) | (1u << 0), STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOff(M5PM1_I2C_ADDR, M5PM1_REG_BTN_CFG_1, (1u << 7), STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOff(M5PM1_I2C_ADDR, M5PM1_REG_BTN_CFG_2, (1u << 0), STOPWATCH_I2C_FREQ);
}

void preInitStopWatchHardware() {
    M5.In_I2C.begin(I2C_NUM_1, GPIO_NUM_47, GPIO_NUM_48);
    configureStopWatchPmic();

    // Wait for M5IOE1 (0x4F) and latch L3B_EN (AMOLED power rail, bit 7) HIGH before M5.begin() initializes CO5300
    M5.In_I2C.writeRegister8(M5IOE1_I2C_ADDR, M5IOE1_REG_I2C_CFG, 0x00, STOPWATCH_I2C_FREQ);
    M5.In_I2C.writeRegister8(M5IOE1_I2C_ADDR, M5IOE1_REG_I2C_CFG, 0x00, STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOff(M5IOE1_I2C_ADDR, M5IOE1_REG_GPIO_DRV_L, 0x9D, STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOn(M5IOE1_I2C_ADDR, M5IOE1_REG_GPIO_MODE_L, 0x9D, STOPWATCH_I2C_FREQ);
    M5.In_I2C.bitOn(M5IOE1_I2C_ADDR, M5IOE1_REG_GPIO_OUT_L, 0x9D, STOPWATCH_I2C_FREQ);

    for (int retry = 0; retry < 15; ++retry) {
        delay(80);
        uint8_t inL = M5.In_I2C.readRegister8(M5IOE1_I2C_ADDR, M5IOE1_REG_GPIO_IN_L, STOPWATCH_I2C_FREQ);
        if ((inL & M5IOE1_L3B_EN_MASK) != 0) {
            break;
        }
        M5.In_I2C.bitOn(M5IOE1_I2C_ADDR, M5IOE1_REG_GPIO_MODE_L, 0x9D, STOPWATCH_I2C_FREQ);
        M5.In_I2C.bitOn(M5IOE1_I2C_ADDR, M5IOE1_REG_GPIO_OUT_L, 0x9D, STOPWATCH_I2C_FREQ);
    }
    delay(50);
    M5.In_I2C.release();
}

void postInitStopWatchHardware() {
    configureStopWatchPmic();
    uint8_t pwrCfg = M5.In_I2C.readRegister8(M5PM1_I2C_ADDR, M5PM1_REG_PWR_CFG, STOPWATCH_I2C_FREQ);
    uint8_t holdCfg = M5.In_I2C.readRegister8(M5PM1_I2C_ADDR, M5PM1_REG_HOLD_CFG, STOPWATCH_I2C_FREQ);
    uint8_t btn1 = M5.In_I2C.readRegister8(M5PM1_I2C_ADDR, M5PM1_REG_BTN_CFG_1, STOPWATCH_I2C_FREQ);
    uint8_t btn2 = M5.In_I2C.readRegister8(M5PM1_I2C_ADDR, M5PM1_REG_BTN_CFG_2, STOPWATCH_I2C_FREQ);
    uint8_t ioeIn = M5.In_I2C.readRegister8(M5IOE1_I2C_ADDR, M5IOE1_REG_GPIO_IN_L, STOPWATCH_I2C_FREQ);
    Serial.printf("[espMoto] pmic: pwr=0x%02X hold=0x%02X btn1=0x%02X btn2=0x%02X ioe_in=0x%02X\n",
                  pwrCfg, holdCfg, btn1, btn2, ioeIn);
}
} // namespace

void setup() {
    Serial.begin(115200);
    preInitStopWatchHardware();

    auto cfg = M5.config();
    M5.begin(cfg);
    postInitStopWatchHardware();
    M5.Display.setBrightness(DISPLAY_BRIGHTNESS);
    M5.Display.setSwapBytes(true);

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

    Serial.printf("[espMoto] ready: board=%d disp=%dx%d paired=%d mac=%s\n",
                  static_cast<int>(M5.getBoard()),
                  static_cast<int>(M5.Display.width()),
                  static_cast<int>(M5.Display.height()),
                  g_ble.isPaired() ? 1 : 0,
                  g_ble.getBleMac().c_str());
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
