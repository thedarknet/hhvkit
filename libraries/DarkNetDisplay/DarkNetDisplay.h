#ifndef _DARKNET_DISPLAY_H
#define _DARKNET_DISPLAY_H

#include <Wire.h>
#include "Arduino.h"
#include "Print.h"

#define swap(a, b) { int16_t t = a; a = b; b = t; }

#define WIRE_WRITE Wire.write

#define BLACK 0
#define WHITE 1
//#define INVERSE 2

#define SSD1306_I2C_ADDRESS   0x3C	// 011110+SA0+RW - 0x3C or 0x3D
// Address for 128x32 is 0x3C
// Address for 128x64 is 0x3D (default) or 0x3C (if SA0 is grounded)

#define SSD1306_128_64

#if defined SSD1306_128_64
  #define SSD1306_LCDWIDTH                  128
  #define SSD1306_LCDHEIGHT                 64
	#define HEIGHT SSD1306_LCDHEIGHT
	#define WIDTH SSD1306_LCDWIDTH
#endif

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF

#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA

#define SSD1306_SETVCOMDETECT 0xDB

#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9

#define SSD1306_SETMULTIPLEX 0xA8

#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10

#define SSD1306_SETSTARTLINE 0x40

#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR   0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8

#define SSD1306_SEGREMAP 0xA0

#define SSD1306_CHARGEPUMP 0x8D

#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2

// Scrolling #defines
#define SSD1306_ACTIVATE_SCROLL 0x2F
#define SSD1306_DEACTIVATE_SCROLL 0x2E
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A

class DarkNetDisplay : public Print {

 public:
  DarkNetDisplay(int8_t RST);

	void drawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t color);
   void drawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
   void fillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
   void fillScreen(uint16_t color);
   void invertDisplay(boolean i);

  void
    drawCircle(uint8_t x0, uint8_t y0, uint8_t r, uint16_t color),
    drawCircleHelper(uint8_t x0, uint8_t y0, uint8_t r, uint8_t cornername,
      uint16_t color),
    fillCircle(uint8_t x0, uint8_t y0, uint8_t r, uint16_t color),
    fillCircleHelper(uint8_t x0, uint8_t y0, uint8_t r, uint8_t cornername,
      uint8_t delta, uint16_t color),
    drawRoundRect(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h,
      uint8_t radius, uint16_t color),
    fillRoundRect(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h,
      uint8_t radius, uint16_t color),
    drawChar(uint8_t x, uint8_t y, unsigned char c, uint16_t color,
      uint16_t bg, uint8_t size),
    setCursor(uint8_t x, uint8_t y),
    setTextColor(uint8_t c),
    setTextSize(uint8_t s),
    setTextWrap(boolean w),
    setRotation(uint8_t r);

  void begin(uint8_t switchvcc = SSD1306_SWITCHCAPVCC, uint8_t i2caddr = SSD1306_I2C_ADDRESS, bool reset=true);
  void ssd1306_command(uint8_t c);
  void ssd1306_data(uint8_t c);
  void clearDisplay(void);
  void invertDisplay(uint8_t i);
  void display();
  void startscrollright(uint8_t start, uint8_t stop);
  void startscrollleft(uint8_t start, uint8_t stop);
  void startscrolldiagright(uint8_t start, uint8_t stop);
  void startscrolldiagleft(uint8_t start, uint8_t stop);
  void stopscroll(void);
  void dim(boolean dim);
  void drawPixel(uint8_t x, uint8_t y, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  size_t write(uint8_t);

  uint8_t height(void) const;
  uint8_t width(void) const;
  uint8_t getRotation(void) const;
  // get current cursor position (get rotation safe maximum values, using: width() for x, height() for y)
  uint8_t getCursorX(void) const;
  uint8_t getCursorY(void) const;

 protected:
  uint8_t _width, _height, // Display w/h as modified by current rotation
    cursor_x, cursor_y;
  uint8_t
	 textcolor:1,
    textsize:4,
    rotation:2, //max value is 3: 0-3
    wrap:1;   // If set, 'wrap' text at right edge of display

 private:
  int8_t _i2caddr, _vccstate, rst;

  inline void drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color) __attribute__((always_inline));
  inline void drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color) __attribute__((always_inline));
};

#endif
