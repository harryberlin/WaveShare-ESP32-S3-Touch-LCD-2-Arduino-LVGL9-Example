#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <CST816S.h>

/*******************************************************************************
 * Display & Touch Pin Configuration (WaveShare ESP32-S3 Touch LCD 2.0)
 * Start of Arduino_GFX setting
 * WaveShare ESP32-S3-Touch-LCD-2 : CS: 45, DC: 42, RST: -1, BL: 1, SCK: 39, MOSI: 38, MISO: 40
 ******************************************************************************/


#define PIN_LCD_SCLK 39
#define PIN_LCD_MOSI 38
#define PIN_LCD_MISO 40
#define PIN_LCD_DC   42
#define PIN_LCD_RST  -1
#define PIN_LCD_CS   45
#define PIN_LCD_BL    1

#define PIN_TP_SDA 48
#define PIN_TP_SCL 47
#define PIN_TP_RST -1
#define PIN_TP_INT 46

#define LCD_ROTATION LV_DISPLAY_ROTATION_0
#define LCD_H_RES 240
#define LCD_V_RES 320

/*******************************************************************************
 * Backlight PWM Settings
 ******************************************************************************/
#define LEDC_FREQ         2000
#define LEDC_TIMER_10_BIT 10

/*******************************************************************************
 * Arduino_GFX Display Bus & Driver
 ******************************************************************************/

Arduino_DataBus *bus = new Arduino_ESP32SPI( PIN_LCD_DC /* DC */, PIN_LCD_CS /* CS */, PIN_LCD_SCLK /* SCK */, PIN_LCD_MOSI /* MOSI */, PIN_LCD_MISO /* MISO */ );

Arduino_GFX *gfx = new Arduino_ST7789(bus, PIN_LCD_RST /* RST */, LCD_ROTATION /* rotation */, true /* IPS */, LCD_H_RES /* width */, LCD_V_RES /* height */ );

/*******************************************************************************
 * Touch Controller (CST816S)
 ******************************************************************************/
CST816S touch(PIN_TP_SDA /* SDA */, PIN_TP_SCL /* SCL */, PIN_TP_RST /* RST */, PIN_TP_INT /* IRQ */);

/*******************************************************************************
 * LVGL Globals
 ******************************************************************************/
uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;

lv_display_t *disp;
lv_color_t *disp_draw_buf;

lv_obj_t *btn_label;
lv_obj_t *slider;
lv_obj_t *slider_label;

/*******************************************************************************
 * LVGL Logging (optional)
 ******************************************************************************/
#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

/*******************************************************************************
 * LVGL Tick Callback
 ******************************************************************************/
uint32_t millis_cb(void) {
    return millis();
}

/*******************************************************************************
 * LVGL Display Flush Callback
 * Called whenever LVGL wants to draw a rendered area to the screen.
 ******************************************************************************/
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

    lv_disp_flush_ready(disp);
}

/*******************************************************************************
 * LVGL Touch Input Callback
 ******************************************************************************/
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (touch.available()) {
        if (lv_display_get_rotation(disp) == LV_DISPLAY_ROTATION_90 ||
            lv_display_get_rotation(disp) == LV_DISPLAY_ROTATION_270) {

            data->point.x = screenWidth  - touch.data.x;
            data->point.y = screenHeight - touch.data.y;
        } else {
            data->point.x = touch.data.x;
            data->point.y = touch.data.y;
        }

        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/*******************************************************************************
 * Slider Event Callback (controls backlight brightness)
 ******************************************************************************/
static void slider_event_cb(lv_event_t *e) {
    uint16_t value = lv_slider_get_value(slider);

    lv_label_set_text_fmt(slider_label, "%d%%", value);

    // Update backlight brightness (0–1023)
    // value (0-100)
    ledcWrite(PIN_LCD_BL, (1 << LEDC_TIMER_10_BIT) / 100 * value);
}

/*******************************************************************************
 * Rotate Display Helper
 ******************************************************************************/
void rotateDisplay(lv_display_rotation_t rotation) {
    gfx->setRotation((uint8_t)rotation);
    lv_display_set_rotation(disp, rotation);

    lv_label_set_text_fmt(btn_label, "%d°", (int)rotation * 90);

    lv_obj_align_to(slider_label, slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_size(slider, gfx->width() - 30, 20);
}

/*******************************************************************************
 * Button Event Callback (rotates display)
 ******************************************************************************/
static void btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    static lv_event_code_t last_code;

    if (code != last_code) {
        if (code == LV_EVENT_PRESSED) {
            static lv_display_rotation_t rotation = lv_display_get_rotation(disp);
            rotation = (rotation == LV_DISPLAY_ROTATION_270) ? LV_DISPLAY_ROTATION_0 : (lv_display_rotation_t)(rotation + 1);
            rotateDisplay(rotation);
        }
    }

    last_code = code;
}

/*******************************************************************************
 * LVGL Task (runs on second core)
 ******************************************************************************/
void guiTask(void *pvParameters) {
    while (true) {
        lv_timer_handler();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

/*******************************************************************************
 * Arduino Setup
 ******************************************************************************/
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(5);

    Serial.println("Arduino_GFX LVGL v9 Example");
    Serial.printf("LVGL Version: V%d.%d.%d\n", lv_version_major(), lv_version_minor(), lv_version_patch());

    // Initialize display
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_BLACK);

    // Backlight PWM
    ledcAttach(PIN_LCD_BL, LEDC_FREQ, LEDC_TIMER_10_BIT);
    ledcWrite(PIN_LCD_BL, (1 << LEDC_TIMER_10_BIT) / 100 * 50);

    // Touch init
    touch.begin();

    // LVGL init
    lv_init();
    lv_tick_set_cb(millis_cb);

    #if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
    #endif

    screenWidth  = gfx->width();
    screenHeight = gfx->height();
    bufSize = screenWidth * 40;

    // Allocate LVGL draw buffer
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    if (!disp_draw_buf) {
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_DMA);
    }

    if (!disp_draw_buf) {
        Serial.println("LVGL draw buffer allocation failed!");
        return;
    }

    // Create LVGL display driver
    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Create LVGL input device (touch)
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);

    /***************************************************************************
     * GUI Elements
     **************************************************************************/

    // Label
    lv_obj_t *label = lv_label_create(lv_screen_active());    
    lv_label_set_text_fmt(label, "Hello Arduino, I'm LVGL!(V%d.%d.%d)", lv_version_major(), lv_version_minor(), lv_version_patch());
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Button (rotate display)
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_size(btn, 80, 40);

    btn_label = lv_label_create(btn);
    lv_label_set_text_fmt(btn_label, "%d°", (int)lv_display_get_rotation(disp) * 90);
    lv_obj_center(btn_label);

    // Slider (backlight)
    slider = lv_slider_create(lv_screen_active());
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 40);
    lv_slider_set_range(slider, 1, 100);
    lv_obj_set_size(slider, screenWidth - 30, 20);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);

    slider_label = lv_label_create(lv_screen_active());
    lv_obj_align_to(slider_label, slider, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text_fmt(slider_label, "%d%%", 50);

    /***************************************************************************
     * Start LVGL Task on Core 1
     **************************************************************************/
    xTaskCreatePinnedToCore(
        guiTask,
        "GUI Task",
        8192,
        NULL,
        1,
        NULL,
        1
    );

    Serial.println("Setup complete");
}

/*******************************************************************************
 * Arduino Loop (unused)
 ******************************************************************************/
void loop() {
    delay(500);
}
