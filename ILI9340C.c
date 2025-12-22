#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SPI_PORT  spi0
#define PIN_SCK   18
#define PIN_MOSI  19
#define PIN_CS    17
#define PIN_DC    20
#define PIN_RST   21
#define PIN_BL    15
#define PIN_SDCS  22  // Added SD Card Silence Pin

// Commands
#define SW_RST    0x01
#define EX_SLP    0x11
#define DP_ON     0x29
#define DP_OFF    0x28
#define PIX_FMT   0x3A
#define FRM_CTRL  0xB1
#define DFP_CTRL  0xB6 // Display Function Control (The "Half-Pixel" Fix)

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

void lcd_init() {
    // 1. Initialize Hardware Peripheral
    spi_init(SPI_PORT, 10 * 1000 * 1000); 
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_init(PIN_DC);
    gpio_init(PIN_RST);
    gpio_init(PIN_BL);
    gpio_init(PIN_SDCS); // SD Card CS

    gpio_set_dir(PIN_CS,   GPIO_OUT);
    gpio_set_dir(PIN_DC,   GPIO_OUT);
    gpio_set_dir(PIN_RST,  GPIO_OUT);
    gpio_set_dir(PIN_BL,   GPIO_OUT);
    gpio_set_dir(PIN_SDCS, GPIO_OUT);

    // 2. Initial State: Silence SD Card and Reset Display
    gpio_put(PIN_SDCS, 1); // Pull SDCS High to stop interference
    gpio_put(PIN_CS, 1);
    gpio_put(PIN_BL, 1);   // Backlight on
    
    gpio_put(PIN_RST, 0);
    sleep_ms(100);
    gpio_put(PIN_RST, 1);
    sleep_ms(150);

    // 3. Software Reset & Wake
    lcd_write_cmd(SW_RST);
    sleep_ms(150);
    lcd_write_cmd(EX_SLP);
    sleep_ms(150);

    // 4. Power & VCOM Control (Fixes Dimness/Flicker)
    lcd_write_cmd(0xC0); // Power Control 1
    uint8_t p1 = 0x23; 
    lcd_write_data(&p1, 1);

    lcd_write_cmd(0xC1); // Power Control 2
    uint8_t p2 = 0x10; 
    lcd_write_data(&p2, 1);

    lcd_write_cmd(0xC5); // VCOM Control 1
    uint8_t v1[] = {0x3E, 0x28};
    lcd_write_data(v1, 2);

    // 5. Frame Rate Control (Fixes "Scanline" flicker)
    lcd_write_cmd(FRM_CTRL);
    uint8_t f1[] = {0x00, 0x18}; // 79Hz refresh
    lcd_write_data(f1, 2);

    // 6. Display Function Control (Fixes "Half-Pixel" look)
    // This defines the scan mode and drive voltage for the panel
    lcd_write_cmd(DFP_CTRL); 
    uint8_t d1[] = {0x08, 0x82, 0x27};
    lcd_write_data(d1, 3);

    // 7. Pixel Format (Set to 16-bit RGB565)
    lcd_write_cmd(PIX_FMT);
    uint8_t px = 0x55;
    lcd_write_data(&px, 1);

    // 8. Final Display Enable
    lcd_write_cmd(DP_ON);
    sleep_ms(100);

    lcd_write_cmd(SW_RST);
    sleep_ms(150);

    // --- EXTENDED POWER CONTROL ---
    // Power on sequence control
    uint8_t pwr_seq[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    lcd_write_cmd(0xCB); lcd_write_data(pwr_seq, 5);

    // Power control B
    uint8_t pwr_b[] = {0x00, 0xC1, 0x30};
    lcd_write_cmd(0xCF); lcd_write_data(pwr_b, 3);

    // Driver timing control A
    uint8_t drv_a[] = {0x85, 0x00, 0x78};
    lcd_write_cmd(0xE8); lcd_write_data(drv_a, 3);

    // Driver timing control B
    uint8_t drv_b[] = {0x00, 0x00};
    lcd_write_cmd(0xEA); lcd_write_data(drv_b, 2);

    // Pump ratio control
    uint8_t pump[] = {0x20};
    lcd_write_cmd(0xF7); lcd_write_data(pump, 1);

    lcd_write_cmd(EX_SLP);
    sleep_ms(120);

    gpio_put(PIN_RST, 0);
    sleep_us(100);
    gpio_put(PIN_RST, 1);
    sleep_us(100);
    lcd_write_cmd(DP_ON);
    sleep_ms(100);
}

void lcd_set_off() {
    lcd_write_cmd(DP_OFF);
    gpio_put(PIN_BL, 0);
}

void lcd_set_on() {
    lcd_write_cmd(DP_ON);
    gpio_put(PIN_BL, 1);
}

uint16_t color_565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    lcd_write_cmd(0x2A); // Column Address Set
    data[0] = x0 >> 8; data[1] = x0 & 0xFF;
    data[2] = x1 >> 8; data[3] = x1 & 0xFF;
    lcd_write_data(data, 4);

    lcd_write_cmd(0x2B); // Page (Row) Address Set
    data[0] = y0 >> 8; data[1] = y0 & 0xFF;
    data[2] = y1 >> 8; data[3] = y1 & 0xFF;
    lcd_write_data(data, 4);

    lcd_write_cmd(0x2C); // Memory Write (Prepare to receive pixels)
}

void lcd_fill_screen(uint16_t color) {
    lcd_set_window(0, 0, 239, 319); // For a 240x320 display

    uint8_t high_byte = color >> 8;
    uint8_t low_byte = color & 0xFF;

    // Put DC High for Data once
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);

    for (int i = 0; i < 240 * 320; i++) {
        spi_write_blocking(SPI_PORT, &high_byte, 1);
        spi_write_blocking(SPI_PORT, &low_byte, 1);
    }

    gpio_put(PIN_CS, 1);
}