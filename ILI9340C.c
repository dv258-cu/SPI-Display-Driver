#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"

#define SPI_PORT   spi0
#define PIN_SCK    18
#define PIN_MOSI   19
#define PIN_CS     17
#define PIN_DC     20
#define PIN_RST    21
#define PIN_BL     15
#define PIN_SDCS   22 

// Commands
#define SW_RST     0x01
#define EX_SLP     0x11
#define DP_ON      0x29
#define DP_OFF     0x28
#define PIX_FMT    0x3A
#define MADCTL     0x36
#define MEM_WR     0x2C

void lcd_write_cmd(uint8_t cmd) {
    gpio_put(PIN_DC, 0); 
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

void lcd_write_data(uint8_t* buf, size_t len) {
    gpio_put(PIN_DC, 1); 
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, buf, len);
    gpio_put(PIN_CS, 1);
}

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t x_buf[] = { (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF) };
    uint8_t y_buf[] = { (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF) };

    // Set Columns
    lcd_write_cmd(0x2A); 
    lcd_write_data(x_buf, 4);

    // Set Rows
    lcd_write_cmd(0x2B); 
    lcd_write_data(y_buf, 4);
}

void lcd_init() {
    spi_init(SPI_PORT, 20 * 1000 * 1000); // Bumped to 20MHz for faster fills
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS); gpio_init(PIN_DC); gpio_init(PIN_RST); gpio_init(PIN_SDCS);
    gpio_set_dir(PIN_CS, GPIO_OUT); gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RST, GPIO_OUT); gpio_set_dir(PIN_SDCS, GPIO_OUT);

    gpio_put(PIN_SDCS, 1); // Silence SD Card
    gpio_put(PIN_CS, 1);

    // Hardware Reset (Only at the start!)
    gpio_put(PIN_RST, 0); sleep_ms(100);
    gpio_put(PIN_RST, 1); sleep_ms(150);

    lcd_write_cmd(SW_RST); sleep_ms(150);
    lcd_write_cmd(EX_SLP); sleep_ms(150);

    // Power Settings
    uint8_t p1 = 0x23; lcd_write_cmd(0xC0); lcd_write_data(&p1, 1);
    uint8_t p2 = 0x10; lcd_write_cmd(0xC1); lcd_write_data(&p2, 1);
    uint8_t v1[] = {0x3E, 0x28}; lcd_write_cmd(0xC5); lcd_write_data(v1, 2);

    // Pixel Format: 16-bit RGB565
    uint8_t px = 0x55; lcd_write_cmd(PIX_FMT); lcd_write_data(&px, 1);

    // Orientation
    uint8_t m = 0x48; lcd_write_cmd(MADCTL); lcd_write_data(&m, 1);

    lcd_write_cmd(DP_ON); sleep_ms(100);

    // PWM Setup for Backlight
    gpio_set_function(PIN_BL, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PIN_BL);
    pwm_set_wrap(slice_num, 255);
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PIN_BL), 0); // Start OFF
    pwm_set_enabled(slice_num, true);
}

void lcd_fill_screen(uint16_t color) {
    // For Portrait: X goes 0-239, Y goes 0-319
    lcd_set_window(0, 0, 239, 319); 
    lcd_write_cmd(0x2C); // Memory Write

    uint8_t hb = color >> 8;
    uint8_t lb = color & 0xFF;
    uint8_t pixel[2] = {hb, lb};

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    // 240 * 320 = 76,800 pixels
    for (uint32_t i = 0; i < (240 * 320); i++) {
        spi_write_blocking(SPI_PORT, pixel, 2);
    }
    gpio_put(PIN_CS, 1);
}

void lcd_set_brightness(uint8_t brightness) {
    pwm_set_gpio_level(PIN_BL, brightness);
}

void lcd_fade_in(uint32_t duration_ms) {
    uint delay = duration_ms / 255;
    for (int i = 0; i <= 255; i++) {
        lcd_set_brightness(i);
        sleep_ms(delay);
    }
}

uint16_t screen_width = 320;
uint16_t screen_height = 240;

void lcd_set_rotation(uint8_t m) {
    lcd_write_cmd(0x36); // MADCTL
    lcd_write_data(&m, 1);
    
    // Update dimensions based on rotation bit (simplified logic)
    if (m & 0x20) { // If the "Row/Column Exchange" bit is set
        screen_width = 320;
        screen_height = 240;
    } else {
        screen_width = 240;
        screen_height = 320;
    }
}

void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    // 1. Force the window to be exactly ONE pixel
    lcd_set_window(x, y, x, y);

    // 2. Open memory for writing
    lcd_write_cmd(0x2C); 

    // 3. Send the 2 color bytes manually
    uint8_t hb = color >> 8;
    uint8_t lb = color & 0xFF;

    gpio_put(PIN_DC, 1); // Data mode
    gpio_put(PIN_CS, 0); // Select
    spi_write_blocking(SPI_PORT, &hb, 1);
    spi_write_blocking(SPI_PORT, &lb, 1);
    gpio_put(PIN_CS, 1); // Deselect - THIS STOPS THE FILL
}

