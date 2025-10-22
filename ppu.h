#ifndef PPU_HPP
#define PPU_HPP

#include <cairo.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <gtk/gtk.h>
#include <queue>

extern std::ofstream debugLog;

class AppleIIVideo {
public:
  static const int WIDTH = 40;
  static const int HEIGHT = 24;

  uint8_t textMemory[0x400];
  cairo_surface_t *surface;
  cairo_t *cr;
  uint16_t cursorPos;

  AppleIIVideo();

  int getRowFromAddress(uint16_t address);
  int getColumnFromAddress(uint16_t address);
  uint16_t screenAddrToLinear(uint16_t screenAddr);
  void initCairo(cairo_t *cairo_ctx);
  void writeByte(uint16_t address, uint8_t value);
  uint8_t readByte(uint16_t address);
  void scrollUp();
  void display();
  void clear();
};

class AppleIIKeyboard {
private:
  uint8_t lastKey;
  bool keyWaiting;

public:
  AppleIIKeyboard();

  uint8_t readKeyboard();
  void strobeKeyboard();
  void injectKey(uint8_t key);
  void checkForInput();
};

#endif
