#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "oled.h"
#include "pump.h"

static const char *TAG = "oled";

#define OLED_ADDR   0x3C        // 0x3D on some modules
#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_PAGES  (OLED_HEIGHT / 8)

/* Soil below this percentage -> angry face (same as the pump's start threshold). */
#define SOIL_THRESHOLD THRESHOLD_L

#define DEGREE_CHAR 0x7F

static i2c_master_dev_handle_t oled_dev;

/* First byte is the I2C control byte (0x40 = data), then 1024 bytes of pixels. */
static uint8_t tx_buf[1 + OLED_WIDTH * OLED_PAGES];
static uint8_t *fb = &tx_buf[1];

/* Values shared with the sensor tasks */
static portMUX_TYPE oled_mux = portMUX_INITIALIZER_UNLOCKED;
static float s_temperature;
static int s_soil;
static bool s_has_temp = false;
static bool s_has_soil = false;

/* Classic 5x7 font, ASCII 0x20..0x7E, plus a degree sign at 0x7F. Bit 0 = top row. */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x56,0x20,0x50}, {0x00,0x08,0x07,0x03,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x2A,0x1C,0x7F,0x1C,0x2A}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x80,0x70,0x30,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x00,0x60,0x60,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x72,0x49,0x49,0x49,0x46}, {0x21,0x41,0x49,0x4D,0x33},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x31}, {0x41,0x21,0x11,0x09,0x07},
    {0x36,0x49,0x49,0x49,0x36}, {0x46,0x49,0x49,0x29,0x1E}, {0x00,0x00,0x14,0x00,0x00}, {0x00,0x40,0x34,0x00,0x00},
    {0x00,0x08,0x14,0x22,0x41}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x59,0x09,0x06},
    {0x3E,0x41,0x5D,0x59,0x4E}, {0x7C,0x12,0x11,0x12,0x7C}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x41,0x3E}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x41,0x51,0x73},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x1C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x26,0x49,0x49,0x49,0x32},
    {0x03,0x01,0x7F,0x01,0x03}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x03,0x04,0x78,0x04,0x03}, {0x61,0x59,0x49,0x4D,0x43}, {0x00,0x7F,0x41,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x41,0x7F}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x03,0x07,0x08,0x00}, {0x20,0x54,0x54,0x78,0x40}, {0x7F,0x28,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x28},
    {0x38,0x44,0x44,0x28,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x00,0x08,0x7E,0x09,0x02}, {0x18,0xA4,0xA4,0x9C,0x78},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x40,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x78,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0xFC,0x18,0x24,0x24,0x18}, {0x18,0x24,0x24,0x18,0xFC}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x24},
    {0x04,0x04,0x3F,0x44,0x24}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44}, {0x4C,0x90,0x90,0x90,0x7C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x77,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x02,0x01,0x02,0x04,0x02}, {0x00,0x06,0x09,0x09,0x06},
};

/* ---------- low level ---------- */

static void oled_cmds(const uint8_t *cmds, size_t len){
    uint8_t buf[32];
    buf[0] = 0x00;                       // control byte: commands
    memcpy(&buf[1], cmds, len);
    ESP_ERROR_CHECK(i2c_master_transmit(oled_dev, buf, len + 1, 100));
}

static void oled_flush(void){
    static const uint8_t window[] = {
        0x21, 0, OLED_WIDTH - 1,         // column range
        0x22, 0, OLED_PAGES - 1,         // page range
    };
    oled_cmds(window, sizeof(window));
    tx_buf[0] = 0x40;                    // control byte: data
    esp_err_t err = i2c_master_transmit(oled_dev, tx_buf, sizeof(tx_buf), 200);
    if (err != ESP_OK){
        ESP_LOGW(TAG, "flush failed: %s", esp_err_to_name(err));
    }
}

void oled_init(i2c_master_bus_handle_t bus){
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &oled_dev));

    static const uint8_t init_seq[] = {
        0xAE,             // display off
        0xD5, 0x80,       // clock divide
        0xA8, 0x3F,       // multiplex 64
        0xD3, 0x00,       // display offset
        0x40,             // start line 0
        0x8D, 0x14,       // charge pump on
        0x20, 0x00,       // horizontal addressing mode
        0xA1,             // segment remap
        0xC8,             // COM scan direction reversed
        0xDA, 0x12,       // COM pins config
        0x81, 0xCF,       // contrast
        0xD9, 0xF1,       // pre-charge
        0xDB, 0x40,       // VCOMH deselect
        0xA4,             // display follows RAM
        0xA6,             // normal (not inverted)
        0x2E,             // no scrolling
        0xAF,             // display on
    };
    oled_cmds(init_seq, sizeof(init_seq));

    memset(fb, 0, OLED_WIDTH * OLED_PAGES);
    oled_flush();
    ESP_LOGI(TAG, "OLED initialised");
}

/* ---------- drawing ---------- */

static void set_pixel(int x, int y){
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    fb[(y / 8) * OLED_WIDTH + x] |= (1 << (y % 8));
}

static void draw_line(int x0, int y0, int x1, int y1){
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1){
        set_pixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy){ err += dy; x0 += sx; }
        if (e2 <= dx){ err += dx; y0 += sy; }
    }
}

static void draw_circle(int cx, int cy, int r){
    int x = r, y = 0, err = 1 - r;
    while (x >= y){
        set_pixel(cx + x, cy + y); set_pixel(cx + y, cy + x);
        set_pixel(cx - y, cy + x); set_pixel(cx - x, cy + y);
        set_pixel(cx - x, cy - y); set_pixel(cx - y, cy - x);
        set_pixel(cx + y, cy - x); set_pixel(cx + x, cy - y);
        y++;
        if (err < 0){ err += 2 * y + 1; }
        else { x--; err += 2 * (y - x) + 1; }
    }
}

static void fill_circle(int cx, int cy, int r){
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r) set_pixel(cx + dx, cy + dy);
}

/* Half circle: smile (lower half) when down = true, frown (upper half) otherwise. Drawn 2px thick. */
static void draw_arc(int cx, int cy, int r, int half_width, bool down){
    int prev_y = 0;
    for (int dx = -half_width; dx <= half_width; dx++){
        int off = (int)lroundf(sqrtf((float)(r * r - dx * dx)));
        int y = down ? cy + off : cy - off;
        if (dx > -half_width) draw_line(cx + dx - 1, prev_y, cx + dx, y);
        set_pixel(cx + dx, y + (down ? 1 : -1));
        prev_y = y;
    }
}

static void draw_char(int x, int y, char c, int scale){
    uint8_t idx = (uint8_t)c;
    if (idx < 0x20 || idx > 0x7F) idx = '?';
    const uint8_t *glyph = font5x7[idx - 0x20];
    for (int col = 0; col < 5; col++){
        for (int row = 0; row < 8; row++){
            if (glyph[col] & (1 << row)){
                for (int sx = 0; sx < scale; sx++)
                    for (int sy = 0; sy < scale; sy++)
                        set_pixel(x + col * scale + sx, y + row * scale + sy);
            }
        }
    }
}

static void draw_text(int x, int y, const char *s, int scale){
    while (*s){
        draw_char(x, y, *s++, scale);
        x += 6 * scale;
    }
}

/* Face in the right part of the screen */
#define FACE_CX 104
#define FACE_CY 32
#define FACE_R  22

static void draw_face(bool happy){
    draw_circle(FACE_CX, FACE_CY, FACE_R);
    draw_circle(FACE_CX, FACE_CY, FACE_R - 1);

    // eyes
    fill_circle(FACE_CX - 8, FACE_CY - 6, 3);
    fill_circle(FACE_CX + 8, FACE_CY - 6, 3);

    if (happy){
        draw_arc(FACE_CX, FACE_CY - 2, 12, 9, true);     // smile
    } else {
        // angry eyebrows slanting down toward the nose
        draw_line(FACE_CX - 14, FACE_CY - 16, FACE_CX - 4, FACE_CY - 11);
        draw_line(FACE_CX - 14, FACE_CY - 15, FACE_CX - 4, FACE_CY - 10);
        draw_line(FACE_CX + 14, FACE_CY - 16, FACE_CX + 4, FACE_CY - 11);
        draw_line(FACE_CX + 14, FACE_CY - 15, FACE_CX + 4, FACE_CY - 10);
        draw_arc(FACE_CX, FACE_CY + 20, 12, 9, false);   // frown
    }
}

/* ---------- public API ---------- */

void oled_set_temperature(float temperature){
    taskENTER_CRITICAL(&oled_mux);
    s_temperature = temperature;
    s_has_temp = true;
    taskEXIT_CRITICAL(&oled_mux);
}

void oled_set_soil(int percent){
    taskENTER_CRITICAL(&oled_mux);
    s_soil = percent;
    s_has_soil = true;
    taskEXIT_CRITICAL(&oled_mux);
}

void oled_task(void *pvParameter){
    char line[16];

    while (1){
        taskENTER_CRITICAL(&oled_mux);
        float temperature = s_temperature;
        int soil = s_soil;
        bool has_temp = s_has_temp;
        bool has_soil = s_has_soil;
        taskEXIT_CRITICAL(&oled_mux);

        memset(fb, 0, OLED_WIDTH * OLED_PAGES);

        draw_text(0, 0, "Temp", 1);
        if (has_temp) snprintf(line, sizeof(line), "%.1f%cC", temperature, DEGREE_CHAR);
        else          snprintf(line, sizeof(line), "--");
        draw_text(0, 10, line, 2);

        draw_text(0, 34, "Soil", 1);
        if (has_soil) snprintf(line, sizeof(line), "%d%%", soil);
        else          snprintf(line, sizeof(line), "--");
        draw_text(0, 44, line, 2);

        if (has_soil){
            draw_face(soil >= SOIL_THRESHOLD);
        }

        oled_flush();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
