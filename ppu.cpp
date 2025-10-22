#include "ppu.h"
#include <cstdio>
#include <iostream>

// ========== AppleIIVideo ==========

AppleIIVideo::AppleIIVideo() 
    : currentMode(TEXT_MODE), displayPage2(false), hiResMode(false), 
      pageFlip(false), surface(nullptr), cr(nullptr), cursorPos(0) {
  memset(textMemory, 0x20, sizeof(textMemory));
  memset(loResMemory, 0, sizeof(loResMemory));
  memset(hiResPage1, 0, sizeof(hiResPage1));
  memset(hiResPage2, 0, sizeof(hiResPage2));
}

// ========== Mode Control ==========

void AppleIIVideo::setTextMode() {
  if (currentMode != TEXT_MODE) {
    currentMode = TEXT_MODE;
    debugLog << "Video mode changed to TEXT\n";
    debugLog.flush();
  }
}

void AppleIIVideo::setLoResMode() {
  if (currentMode != LORES_MODE) {
    currentMode = LORES_MODE;
    debugLog << "Video mode changed to LO-RES\n";
    debugLog.flush();
  }
}

void AppleIIVideo::setHiResMode() {
  if (currentMode != HIRES_MODE) {
    currentMode = HIRES_MODE;
    hiResMode = true;
    debugLog << "Video mode changed to HI-RES\n";
    debugLog.flush();
  }
}

void AppleIIVideo::setMixedMode() {
  // Mixed mode: lower 4 lines show text, rest shows graphics
  currentMode = HIRES_MODE;
  debugLog << "Video mode changed to MIXED (HI-RES with text overlay)\n";
  debugLog.flush();
}

void AppleIIVideo::setPage2(bool page2) {
  displayPage2 = page2;
  if (currentMode == HIRES_MODE) {
    debugLog << "Hi-Res display switched to page " << (page2 ? "2" : "1") << "\n";
    debugLog.flush();
  }
}

void AppleIIVideo::handleGraphicsSoftSwitch(uint16_t address) {
  // Apple II soft switches for graphics control
  // $C050 - TEXT mode OFF (read/write, toggles)
  // $C051 - GRAPHICS mode ON (read/write, toggles)
  // $C052 - FULL SCREEN mode (no text window)
  // $C053 - MIXED mode (text window at bottom)
  // $C054 - PAGE 1 (display $2000-$3FFF for hi-res)
  // $C055 - PAGE 2 (display $4000-$5FFF for hi-res)
  // $C056 - LO-RES mode
  // $C057 - HI-RES mode
  
  switch (address & 0xFF) {
    case 0x50:  // TEXT mode OFF
      debugLog << "Soft switch at $c050 -> GRAPHICS mode\n";
      break;
    case 0x51:  // GRAPHICS mode ON
      debugLog << "Soft switch at $c051 -> TEXT mode\n";
      break;
    case 0x52:  // FULL SCREEN
      debugLog << "Soft switch at $c052 -> FS Mode\n";

      break;
    case 0x53:  // MIXED mode
      debugLog << "Soft switch at $c053 -> MIXED mode\n";
      setMixedMode();
      break;
    case 0x54:  // PAGE 1
      debugLog << "Soft switch at $c054 -> PAGE 1\n";
      setPage2(false);
      break;
    case 0x55:  // PAGE 2
      debugLog << "Soft switch at $c055 -> PAGE 2\n";
      setPage2(true);
      break;
    case 0x56:  // LO-RES mode
      debugLog << "Soft switch at $c056 -> LO-RES mode\n";
      //setLoResMode();
      //hiResMode = false;
      break;
    case 0x57:  // HI-RES mode
      debugLog << "Soft switch at $c057 -> HI-RES mode\n";
      setHiResMode();
      hiResMode = true;
      break;
  }
  debugLog.flush();
}

// ========== Text Mode Address Mapping ==========

int AppleIIVideo::getRowFromAddress(uint16_t address) {
  static const uint16_t rowStarts[24] = {
      0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780, 0x0428,
      0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8, 0x0450, 0x04D0,
      0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0};

  for (int row = 0; row < 24; ++row) {
    uint16_t start = rowStarts[row];
    if (address >= start && address < start + 0x28) {
      return row;
    }
  }
  return -1;
}

int AppleIIVideo::getColumnFromAddress(uint16_t address) {
  static const uint16_t rowStarts[24] = {
      0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
      0x0428, 0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8,
      0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0};

  for (int row = 0; row < 24; ++row) {
    uint16_t start = rowStarts[row];
    if (address >= start && address < start + 0x28) {
      return address - start;
    }
  }
  return -1;
}

uint16_t AppleIIVideo::screenAddrToLinear(uint16_t screenAddr) {
  if (screenAddr < 0x400 || screenAddr >= 0x800)
    return 0;

  uint16_t row = getRowFromAddress(screenAddr);
  uint16_t col = getColumnFromAddress(screenAddr);

  if (col < 0x28) {
    return row * 0x28 + col;
  }
  return 0;
}

// ========== Hi-Res Mode Address Mapping ==========

int AppleIIVideo::getHiResRow(uint16_t address) {
  // Hi-res memory layout is interleaved by rows across three 256-byte pages
  // Each 128-byte section in a page represents 8 rows
  // Pages 0-2 are within each $2000 block
  
  // Determine which page (0-2) and position within page
  uint16_t offset = address & 0x1FFF;  // Position within $2000 block
  
  int page = offset / 0x100;      // Which 256-byte page (0-31)
  int posInPage = offset % 0x100;  // Position within page
  
  // Each page has 8 rows
  int row = (page * 8) + (posInPage / 0x80);
  
  if (row >= HIRES_HEIGHT) row = HIRES_HEIGHT - 1;
  return row;
}

int AppleIIVideo::getHiResCol(uint16_t address) {
  uint16_t offset = address & 0x1FFF;
  int posInPage = offset % 0x100;
  int colByte = posInPage % 0x80;  // 0-127 (128 bytes = 280 pixels at 7 bits per byte)
  return colByte * 7;  // Each byte holds 7 pixels
}

uint16_t AppleIIVideo::hiResAddrToLinear(uint16_t address) {
  return getHiResRow(address) * HIRES_WIDTH + getHiResCol(address);
}

// ========== Memory Access ==========

void AppleIIVideo::writeByte(uint16_t address, uint8_t value) {
  // Text/Lo-Res mode writes
  if (address >= TEXT_START && address < TEXT_END) {
    uint16_t linearAddr = screenAddrToLinear(address);
    if (linearAddr < 0x3C0) {
      uint8_t displayChar = value & 0x7F;
      textMemory[linearAddr] = displayChar;
      // Also write to lo-res memory for graphics mode
      loResMemory[linearAddr] = value;
    }
    return;
  }
  
  // Hi-Res Page 1 writes
  if (address >= HIRES_PAGE1_START && address < HIRES_PAGE1_END) {
    uint16_t offset = address - HIRES_PAGE1_START;
    if (offset < 0x2000) {
      hiResPage1[offset] = value;
    }
    return;
  }
  
  // Hi-Res Page 2 writes
  if (address >= HIRES_PAGE2_START && address < HIRES_PAGE2_END) {
    uint16_t offset = address - HIRES_PAGE2_START;
    if (offset < 0x2000) {
      hiResPage2[offset] = value;
    }
    return;
  }
}

uint8_t AppleIIVideo::readByte(uint16_t address) {
  if (address >= TEXT_START && address < TEXT_END) {
    uint16_t linearAddr = screenAddrToLinear(address);
    if (linearAddr < 0x3C0) {
      return textMemory[linearAddr];
    }
    return 0;
  }
  
  if (address >= HIRES_PAGE1_START && address < HIRES_PAGE1_END) {
    uint16_t offset = address - HIRES_PAGE1_START;
    if (offset < 0x2000) {
      return hiResPage1[offset];
    }
    return 0;
  }
  
  if (address >= HIRES_PAGE2_START && address < HIRES_PAGE2_END) {
    uint16_t offset = address - HIRES_PAGE2_START;
    if (offset < 0x2000) {
      return hiResPage2[offset];
    }
    return 0;
  }
  
  return 0;
}

// ========== Color Utilities ==========

void AppleIIVideo::getRGBForLoResColor(LoResColor color, double &r, double &g, double &b) {
  switch (color) {
    case BLACK:       r = 0.0;   g = 0.0;   b = 0.0;   break;
    case MAGENTA:     r = 1.0;   g = 0.0;   b = 1.0;   break;
    case DARK_BLUE:   r = 0.0;   g = 0.0;   b = 0.7;   break;
    case PURPLE:      r = 1.0;   g = 0.0;   b = 0.7;   break;
    case DARK_GREEN:  r = 0.0;   g = 0.5;   b = 0.0;   break;
    case GRAY1:       r = 0.5;   g = 0.5;   b = 0.5;   break;
    case MEDIUM_BLUE: r = 0.0;   g = 0.0;   b = 1.0;   break;
    case LIGHT_BLUE:  r = 0.5;   g = 0.5;   b = 1.0;   break;
    case BROWN:       r = 0.5;   g = 0.25;  b = 0.0;   break;
    case ORANGE:      r = 1.0;   g = 0.5;   b = 0.0;   break;
    case GRAY2:       r = 0.75;  g = 0.75;  b = 0.75;  break;
    case PINK:        r = 1.0;   g = 0.5;   b = 0.5;   break;
    case GREEN:       r = 0.0;   g = 1.0;   b = 0.0;   break;
    case YELLOW:      r = 1.0;   g = 1.0;   b = 0.0;   break;
    case AQUA:        r = 0.0;   g = 1.0;   b = 1.0;   break;
    case WHITE:       r = 1.0;   g = 1.0;   b = 1.0;   break;
  }
}

// ========== Display Functions ==========

void AppleIIVideo::displayTextMode() {
  if (!cr) return;

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_paint(cr);

  cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 14);

  double char_width = 8.5;
  double line_height = 16.0;
  double x_offset = 2;
  double y_offset = 12;

  for (int i = 0; i < 0x3C0; i++) {
    int row = i / TEXT_WIDTH;
    int col = i % TEXT_WIDTH;

    uint8_t c = textMemory[i];
    if (c < 32) c = ' ';
    if (c > 126) c = c & 0x7F;

    char buf[2] = {(char)c, 0};
    double x = x_offset + (col * char_width);
    double y = y_offset + (row * line_height);

    cairo_move_to(cr, x, y);
    cairo_show_text(cr, buf);
  }
}

void AppleIIVideo::displayLoResMode() {
  if (!cr) return;

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_paint(cr);

  double pixel_size = 2.0;  // Scale up pixels for visibility
  
  // Lo-res is 40x48 blocks, each block is 7x8 pixels
  // Display is 280x192 pixels
  for (int row = 0; row < LORES_HEIGHT; row++) {
    for (int col = 0; col < LORES_WIDTH; col++) {
      // Get the byte containing color for this block
      // Lo-res memory is same as text memory, but interpreted as graphics
      int memoryIndex = row / 2 * 40 + col;
      if (memoryIndex >= 0x400) continue;
      
      uint8_t byte = loResMemory[memoryIndex];
      
      // Each byte contains two 4-bit color values
      // Even rows use lower nibble, odd rows use upper nibble
      uint8_t colorIndex = (row & 1) ? (byte >> 4) : (byte & 0x0F);
      
      double r, g, b;
      getRGBForLoResColor((LoResColor)colorIndex, r, g, b);
      cairo_set_source_rgb(cr, r, g, b);
      
      // Draw the block
      double x = col * 7 * pixel_size;
      double y = row * 4 * pixel_size;
      cairo_rectangle(cr, x, y, 7 * pixel_size, 4 * pixel_size);
      cairo_fill(cr);
    }
  }
}

void AppleIIVideo::displayHiResMode() {
  if (!cr) return;

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_paint(cr);

  // Select which page to display
  uint8_t* displayBuffer = displayPage2 ? hiResPage2 : hiResPage1;
  
  cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);  // Green monochrome display
  double pixel_size = 2.0;  // Scale up for visibility
  
  // Apple II hi-res memory layout (correct):
  // Address bits: AAAAAAA BBBCCCC D
  // A (bits 0-6): column byte (0-127, but only 0-39 used for 280 pixels)
  // B (bits 7-9): row within 8-row block (0-7)
  // C (bits 10-13): block within section (0-15, but only 0-2 used... actually 0-23)
  // D (bit 14): section (0-2, for three vertical thirds)
  // So: addr = column + (row_in_block * 128) + (block * 1024)
  // Reverse: row_in_block = (addr >> 7) & 7, block = (addr >> 10) & 31, column = addr & 127
  
  for (int addr = 0; addr < 0x2000; addr++) {
    uint8_t byte = displayBuffer[addr];
    
    // Decode address
    int column = addr & 0x7F;      // Bits 0-6: column (0-127)
    int rowInBlock = (addr >> 7) & 7;   // Bits 7-9: row within 8-row block
    int block = (addr >> 10) & 31;  // Bits 10-14: block number (0-31, but only 0-2 valid)
    
    // Calculate actual row from block and row-in-block
    // Block structure: each block is 8 rows
    // So row = block * 8 + rowInBlock, but blocks are organized in 3 sections of 8 blocks each
    int row = (block * 8) + rowInBlock;
    if (row >= HIRES_HEIGHT) continue;
    
    // Only render columns 0-39 (280 pixels / 7 bits per byte)
    if (column >= 40) continue;
    
    // Render this byte's pixels
    for (int bit = 0; bit < 7; bit++) {
      if (byte & (1 << bit)) {
        double x = (column * 7 + bit) * pixel_size;
        double y = row * pixel_size;
        
        cairo_rectangle(cr, x, y, pixel_size, pixel_size);
        cairo_fill(cr);
      }
    }
  }
}

void AppleIIVideo::display() {
  if (!cr) return;

  switch (currentMode) {
    case TEXT_MODE:
      displayTextMode();
      break;
    case LORES_MODE:
      displayLoResMode();
      break;
    case HIRES_MODE:
      displayHiResMode();
      break;
  }
}

void AppleIIVideo::initCairo(cairo_t *cairo_ctx) {
  cr = cairo_ctx;
}

void AppleIIVideo::clear() {
  memset(textMemory, 0x20, sizeof(textMemory));
  memset(loResMemory, 0, sizeof(loResMemory));
  memset(hiResPage1, 0, sizeof(hiResPage1));
  memset(hiResPage2, 0, sizeof(hiResPage2));
  cursorPos = 0;
}

void AppleIIVideo::scrollUp() {
  static const uint16_t rowStarts[24] = {
      0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
      0x0428, 0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8,
      0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0};

  for (int page = 0; page < 3; page++) {
    uint16_t pageBase = 0x0400 + (page * 0x100);

    for (int rowInPage = 0; rowInPage < 3; rowInPage++) {
      uint16_t currentRowOffset = pageBase + (rowInPage * 0x80);
      uint16_t nextRowOffset = pageBase + ((rowInPage + 1) * 0x80);

      memcpy((uint8_t *)textMemory + currentRowOffset,
             (uint8_t *)textMemory + nextRowOffset, 0x28);
    }

    uint16_t lastRowOffset = pageBase + (3 * 0x80);
    memset((uint8_t *)textMemory + lastRowOffset, 0x20, 0x28);
  }

  cursorPos = rowStarts[23];
}

// ========== AppleIIKeyboard ==========

AppleIIKeyboard::AppleIIKeyboard() : lastKey(0), keyWaiting(false) {}

uint8_t AppleIIKeyboard::readKeyboard() { 
  return lastKey; 
}

void AppleIIKeyboard::strobeKeyboard() {
  lastKey = lastKey & 0x7F;
  keyWaiting = false;
  debugLog << "Keyboard strobe: key cleared\n";
  debugLog.flush();
}

void AppleIIKeyboard::injectKey(uint8_t key) {
  if (key == '\n' || key == '\r') {
    key = '\r';
  }

  lastKey = key | 0x80;
  keyWaiting = true;
  /*debugLog << "Key injected: $" << std::hex << (int)lastKey << std::dec
           << " ('" << (char)(key & 0x7F) << "')\n";
  debugLog.flush(); */
}

void AppleIIKeyboard::checkForInput() {
  // Input handled by GTK or ncurses
}
