void lcd_write_cmd(uint8_t cmd);
void lcd_write_data(uint8_t* buf, size_t len);
void lcd_init();
void lcd_set_off();
void lcd_set_on();
uint16_t color_565(uint8_t r, uint8_t g, uint8_t b);
void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_fill_screen(uint16_t color);