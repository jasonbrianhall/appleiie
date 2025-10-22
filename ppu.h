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
  // Display modes
  enum VideoMode {
    TEXT_MODE = 0,      // 40x24 text
    LORES_MODE = 1,     // 40x48 low-res graphics (4-color blocks)
    HIRES_MODE = 2      // 280x192 high-res graphics (monochrome)
  };

  // Text mode constants
  static const int TEXT_WIDTH = 40;
  static const int TEXT_HEIGHT = 24;

  // Graphics mode constants
  static const int LORES_WIDTH = 40;
  static const int LORES_HEIGHT = 48;
  static const int LORES_BLOCK_SIZE = 4;  // Each cell is 4x4 pixels in lo-res
  
  static const int HIRES_WIDTH = 280;
  static const int HIRES_HEIGHT = 192;
  
  // Apple II memory layout for graphics
  // Text/Lo-Res: $0400-$07FF (3 pages)
  // Hi-Res Page 1: $2000-$3FFF
  // Hi-Res Page 2: $4000-$5FFF
  
  static const uint16_t TEXT_START = 0x0400;
  static const uint16_t TEXT_END = 0x0800;
  static const uint16_t LORES_START = 0x0400;
  static const uint16_t LORES_END = 0x0800;
  static const uint16_t HIRES_PAGE1_START = 0x2000;
  static const uint16_t HIRES_PAGE1_END = 0x4000;
  static const uint16_t HIRES_PAGE2_START = 0x4000;
  static const uint16_t HIRES_PAGE2_END = 0x6000;
  
  // Color palette for lo-res graphics
  // Each 4-bit value selects a color from 16-color palette
  enum LoResColor {
    BLACK = 0x0,
    MAGENTA = 0x1,
    DARK_BLUE = 0x2,
    PURPLE = 0x3,
    DARK_GREEN = 0x4,
    GRAY1 = 0x5,
    MEDIUM_BLUE = 0x6,
    LIGHT_BLUE = 0x7,
    BROWN = 0x8,
    ORANGE = 0x9,
    GRAY2 = 0xA,
    PINK = 0xB,
    GREEN = 0xC,
    YELLOW = 0xD,
    AQUA = 0xE,
    WHITE = 0xF
  };

  // Video state
  VideoMode currentMode;
  uint8_t textMemory[0x400];       // Text memory (40x24 = 960 bytes, fits in 0x400)
  uint8_t loResMemory[0x400];      // Lo-res graphics shares same space as text
  uint8_t hiResPage1[0x2000];      // Hi-res page 1 (8KB)
  uint8_t hiResPage2[0x2000];      // Hi-res page 2 (8KB)
  
  bool displayPage2;               // True = show page 2, False = show page 1
  bool hiResMode;                  // Mixed/hi-res mode flag
  bool pageFlip;                   // Page 2 flag (soft switch $C05F)
  
  // Rendering
  cairo_surface_t *surface;
  cairo_t *cr;
  uint16_t cursorPos;

  AppleIIVideo();

  // Mode control
  void setTextMode();
  void setLoResMode();
  void setHiResMode();
  void setMixedMode();
  void setPage2(bool page2);
  VideoMode getMode() const { return currentMode; }
  
  // Soft switch handlers (called from CPU memory access)
  void handleGraphicsSoftSwitch(uint16_t address);

  // Text mode functions (existing)
  int getRowFromAddress(uint16_t address);
  int getColumnFromAddress(uint16_t address);
  uint16_t screenAddrToLinear(uint16_t screenAddr);

  // Hi-res mode functions
  int getHiResRow(uint16_t address);
  int getHiResCol(uint16_t address);
  uint16_t hiResAddrToLinear(uint16_t address);
  
  // Memory access
  void writeByte(uint16_t address, uint8_t value);
  uint8_t readByte(uint16_t address);
  
  // Rendering
  void initCairo(cairo_t *cairo_ctx);
  void display();
  void displayTextMode();
  void displayLoResMode();
  void displayHiResMode();
  void clear();
  void scrollUp();
  
  // Color utilities
  void getRGBForLoResColor(LoResColor color, double &r, double &g, double &b);
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
