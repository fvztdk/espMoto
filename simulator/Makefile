CC      := gcc
CXX     := g++
BUILD   := build
TARGET  := $(BUILD)/libespmoto_lvgl.so

LVGL_DIR := $(if $(wildcard ../firmware/.pio/libdeps/m5stack-stopwatch/lvgl/src),../firmware/.pio/libdeps/m5stack-stopwatch/lvgl,../firmware/lib/lvgl)
FW_INC   := ../firmware/include
FW_SRC   := ../firmware/src

INC_FLAGS := -DLV_CONF_INCLUDE_SIMPLE -I$(FW_INC) -I$(LVGL_DIR) -I$(LVGL_DIR)/src -I$(FW_SRC)
CFLAGS    := -O2 -fPIC -Wall -Wextra -Wno-unused-parameter $(INC_FLAGS)
CXXFLAGS  := -std=c++17 -O2 -fPIC -Wall -Wextra -Wno-unused-parameter $(INC_FLAGS)
LDFLAGS   := -shared

LVGL_SRCS := $(shell find $(LVGL_DIR)/src -type f -name '*.c')
LVGL_OBJS := $(patsubst $(LVGL_DIR)/src/%.c,$(BUILD)/lvgl/%.o,$(LVGL_SRCS))

APP_OBJS  := $(BUILD)/app/obd_config.o $(BUILD)/app/ui_renderer.o $(BUILD)/app/sim_bridge.o

.PHONY: all app clean

all: $(TARGET)

app: $(APP_OBJS)
	@mkdir -p $(dir $(TARGET))
	$(CXX) $(LDFLAGS) -o $(TARGET) $(LVGL_OBJS) $(APP_OBJS) -lm

$(TARGET): $(LVGL_OBJS) $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) -o $@ $^ -lm

$(BUILD)/lvgl/%.o: $(LVGL_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/app/obd_config.o: $(FW_SRC)/obd_config.cpp $(FW_SRC)/obd_config.h $(FW_SRC)/nav_state.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/app/ui_renderer.o: $(FW_SRC)/ui_renderer.cpp $(FW_SRC)/ui_renderer.h $(FW_SRC)/obd_config.h $(FW_SRC)/nav_state.h $(FW_INC)/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/app/sim_bridge.o: sim_bridge.cpp $(FW_SRC)/ui_renderer.h $(FW_SRC)/obd_config.h $(FW_SRC)/nav_state.h $(FW_INC)/lv_conf.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)
