#include <lvgl.h>
#include "../firmware/src/nav_state.h"
#include "../firmware/src/obd_config.h"
#include "../firmware/src/ui_renderer.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace {

constexpr int32_t SIM_SIZE = 466;
constexpr int32_t DRAW_BUF_LINES = 64;

uint8_t g_rgba_fb[SIM_SIZE * SIM_SIZE * 4] = {};
uint16_t g_draw_buf[SIM_SIZE * DRAW_BUF_LINES] = {};

lv_display_t* g_disp = nullptr;
lv_indev_t* g_indev = nullptr;

NavState g_nav;
BikeTelemetry g_bike;
ObdConfig g_obd_cfg = makeDefaultObdConfig();
UiRenderer g_ui;

bool g_initialized = false;
int32_t g_mouse_x = 0;
int32_t g_mouse_y = 0;
bool g_mouse_pressed = false;
std::string g_pairing_payload;

void sim_generate_pairing_payload() {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        seeded = true;
    }
    char token[17];
    std::snprintf(token, sizeof(token), "%08X%08X",
                  static_cast<unsigned int>(std::rand()),
                  static_cast<unsigned int>(std::rand()));
    g_pairing_payload = std::string("espmoto://pair?mac=DE:AD:BE:EF:12:34&token=") +
                        token + "&name=espMoto";
    g_ui.setPairingPayload(g_pairing_payload.c_str(), "espMoto • DE:AD:BE:EF:12:34");
}

void sim_on_reset_pairing(void* /*userData*/) {
    sim_generate_pairing_payload();
}

void sim_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    const auto* src = reinterpret_cast<const uint16_t*>(px_map);
    const int32_t x1 = (area->x1 < 0) ? 0 : area->x1;
    const int32_t y1 = (area->y1 < 0) ? 0 : area->y1;
    const int32_t x2 = (area->x2 >= SIM_SIZE) ? (SIM_SIZE - 1) : area->x2;
    const int32_t y2 = (area->y2 >= SIM_SIZE) ? (SIM_SIZE - 1) : area->y2;
    const int32_t area_w = area->x2 - area->x1 + 1;

    for (int32_t y = y1; y <= y2; ++y) {
        const uint16_t* row = src + (y - area->y1) * area_w + (x1 - area->x1);
        uint8_t* dst = g_rgba_fb + (y * SIM_SIZE + x1) * 4;
        for (int32_t x = x1; x <= x2; ++x) {
            uint16_t c = *row++;
            dst[0] = static_cast<uint8_t>((((c >> 11) & 0x1F) * 255) / 31);
            dst[1] = static_cast<uint8_t>((((c >> 5) & 0x3F) * 255) / 63);
            dst[2] = static_cast<uint8_t>(((c & 0x1F) * 255) / 31);
            dst[3] = 255;
            dst += 4;
        }
    }

    lv_display_flush_ready(disp);
}

void sim_touch_read_cb(lv_indev_t* /*indev*/, lv_indev_data_t* data) {
    data->point.x = g_mouse_x;
    data->point.y = g_mouse_y;
    data->state = g_mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

} // namespace

extern "C" {

void sim_init() {
    if (g_initialized) {
        return;
    }

    // Initialize opaque black RGBA framebuffer
    for (int32_t i = 0; i < SIM_SIZE * SIM_SIZE; ++i) {
        g_rgba_fb[i * 4 + 0] = 0;
        g_rgba_fb[i * 4 + 1] = 0;
        g_rgba_fb[i * 4 + 2] = 0;
        g_rgba_fb[i * 4 + 3] = 255;
    }

    lv_init();

    g_disp = lv_display_create(SIM_SIZE, SIM_SIZE);
    lv_display_set_buffers(g_disp, g_draw_buf, nullptr, sizeof(g_draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(g_disp, sim_flush_cb);

    g_indev = lv_indev_create();
    lv_indev_set_type(g_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_indev, sim_touch_read_cb);

    g_ui.begin();
    g_ui.setResetPairingCallback(&sim_on_reset_pairing, nullptr);
    sim_generate_pairing_payload();
    g_initialized = true;

    lv_timer_handler();
    lv_refr_now(g_disp);
}

void sim_tick(uint32_t ms) {
    if (!g_initialized) {
        return;
    }
    lv_tick_inc(ms);
    lv_timer_handler();
}

void sim_render(int demo_mode,
                int phone_connected,
                int nav_active,
                int maneuver,
                const char* distance,
                const char* street,
                const char* eta,
                int obd_connected,
                int rpm,
                int speed_kmh,
                int gear,
                int engine_temp_c,
                float battery_volts) {
    if (!g_initialized) {
        return;
    }

    g_nav.active = (nav_active != 0);
    g_nav.maneuver = static_cast<ManeuverType>(constrain(maneuver, 0, 17));
    g_nav.distance = distance ? distance : "--";
    g_nav.street = street ? street : "";
    g_nav.eta = eta ? eta : "--:--";

    g_bike.obdConnected = (obd_connected != 0);
    g_bike.rpm = static_cast<uint16_t>(constrain(rpm, 0, 12000));
    g_bike.speedKmh = static_cast<uint16_t>(constrain(speed_kmh, 0, 400));
    g_bike.gear = static_cast<int8_t>(constrain(gear, 0, 6));
    g_bike.engineTempC = static_cast<int16_t>(engine_temp_c);
    g_bike.batteryVolts = battery_volts;

    g_ui.render(g_nav, g_bike, (phone_connected != 0), (demo_mode != 0), &g_obd_cfg);
    lv_timer_handler();
    lv_refr_now(g_disp);
}

void sim_set_obd_config_json(const char* json) {
    if (!json) return;
    parseObdConfigJson(json, g_obd_cfg);
    if (g_initialized) {
        g_ui.render(g_nav, g_bike, false, true, &g_obd_cfg);
        lv_timer_handler();
        lv_refr_now(g_disp);
    }
}

void sim_set_obd_display(int max_rpm, int redline_rpm, int center_slot, int bl_slot, int br_slot, int bc_slot) {
    if (max_rpm >= 1000 && max_rpm <= 20000) g_obd_cfg.display.maxRpm = static_cast<uint16_t>(max_rpm);
    if (redline_rpm >= 1000 && redline_rpm <= 20000) g_obd_cfg.display.redlineRpm = static_cast<uint16_t>(redline_rpm);
    g_obd_cfg.display.centerSlot = static_cast<MetricId>(center_slot);
    g_obd_cfg.display.bottomLeftSlot = static_cast<MetricId>(bl_slot);
    g_obd_cfg.display.bottomRightSlot = static_cast<MetricId>(br_slot);
    g_obd_cfg.display.bottomCenterSlot = static_cast<MetricId>(bc_slot);
}

void sim_next_page() {
    if (!g_initialized) {
        return;
    }
    g_ui.nextPage();
    lv_timer_handler();
    lv_refr_now(g_disp);
}

void sim_set_page(int page) {
    if (!g_initialized) {
        return;
    }
    ScreenPage target = ScreenPage::TELEMETRY;
    if (page == 0) target = ScreenPage::NAVIGATION;
    else if (page == 2) target = ScreenPage::SETTINGS;
    else if (page == 3) target = ScreenPage::PAIRING;
    g_ui.setPage(target);
    lv_timer_handler();
    lv_refr_now(g_disp);
}

void sim_reset_pairing() {
    if (!g_initialized) {
        return;
    }
    sim_generate_pairing_payload();
    g_ui.setPage(ScreenPage::PAIRING);
    lv_timer_handler();
    lv_refr_now(g_disp);
}

void sim_complete_pairing() {
    if (!g_initialized) {
        return;
    }
    g_ui.setPage(g_nav.active ? ScreenPage::NAVIGATION : ScreenPage::TELEMETRY);
    lv_timer_handler();
    lv_refr_now(g_disp);
}

const char* sim_get_pairing_payload() {
    return g_pairing_payload.c_str();
}

int sim_get_page() {
    return static_cast<int>(g_ui.currentPage());
}

void sim_set_mouse(int x, int y, int pressed) {
    g_mouse_x = constrain(x, 0, SIM_SIZE - 1);
    g_mouse_y = constrain(y, 0, SIM_SIZE - 1);
    g_mouse_pressed = (pressed != 0);
    if (g_initialized) {
        lv_tick_inc(20);
        lv_timer_handler();
    }
}

const uint8_t* sim_get_rgba_buffer() {
    return g_rgba_fb;
}

} // extern "C"
