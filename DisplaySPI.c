#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ILI9340C.h"

#define SPI_PORT  spi0
#define PIN_SCK   18
#define PIN_MOSI  19
#define PIN_CS    17
#define PIN_DC    20
#define PIN_RST   21
#define PIN_BL    15



int main() {
    stdio_init_all();
    sleep_ms(2000); // Wait for USB/Serial to settle
        
        // Force a known state
    lcd_init(); 

    // 1. Define the window (Start X, Start Y, End X, End Y)
    uint16_t x_start = 0;
    uint16_t y_start = 0;
    uint16_t x_end = 239; // Width - 1
    uint16_t y_end = 319; // Height - 1

    // Send Column Address
    lcd_write_cmd(0x2A);
    uint8_t col[] = {x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF};
    lcd_write_data(col, 4);

    // Send Row Address
    lcd_write_cmd(0x2B);
    uint8_t row[] = {y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF};
    lcd_write_data(row, 4);

    // 2. THE CRITICAL COMMAND: Write to RAM
    lcd_write_cmd(0x2C); 

    // 3. Send RED pixels (0xF800)
    uint8_t color_bytes[] = {0xF8, 0x00};
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);

    for (uint32_t i = 0; i < (240 * 320); i++) {
        spi_write_blocking(SPI_PORT, color_bytes, 2);
    }
    gpio_put(PIN_CS, 1);
}