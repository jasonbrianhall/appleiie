#include "ppu.h"
#include <cstdio>

// ========== AppleIIVideo ==========

AppleIIVideo::AppleIIVideo() : surface(nullptr), cr(nullptr), cursorPos(0) {
  memset(textMemory, 0x20, sizeof(textMemory));
}

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

  return -1; // Not a valid screen address
}

int AppleIIVideo::getColumnFromAddress(uint16_t address) {
  static const uint16_t rowStarts[24] = {
      0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
      0x0428, 0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8,
      0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0};

  for (int row = 0; row < 24; ++row) {
    uint16_t start = rowStarts[row];
    if (address >= start && address < start + 0x28) {
      return address - start; // Column is offset from row start
    }
  }

  return -1; // Not a valid screen address
}

uint16_t AppleIIVideo::screenAddrToLinear(uint16_t screenAddr) {
  // Apple II text screen is interleaved by rows across three 256-byte pages:
  // Page 0 ($0400-$04FF): rows 0, 3, 6, 9, 12, 15, 18, 21
  // Page 1 ($0500-$05FF): rows 1, 4, 7, 10, 13, 16, 19, 22
  // Page 2 ($0600-$06FF): rows 2, 5, 8, 11, 14, 17, 20, 23
  //
  // Within each page, 40 bytes per row

  if (screenAddr < 0x400 || screenAddr >= 0x800)
    return 0;

  uint16_t row = getRowFromAddress(screenAddr);
  uint16_t col = getColumnFromAddress(screenAddr);

  uint16_t linear;
  // Convert to linear position (row * 40 + col)
  if (col < 0x28) {
    linear = row * 0x28 + col;
  } else {
    return 0;
  }

  return linear;
}

void AppleIIVideo::initCairo(cairo_t *cairo_ctx) { cr = cairo_ctx; }

void AppleIIVideo::writeByte(uint16_t address, uint8_t value) {
  if (address >= 0x400 && address < 0x800) {
    uint16_t linearAddr = screenAddrToLinear(address);

    // Screen is 24 rows * 40 cols = 0x3C0 bytes max
    if (linearAddr < 0x3C0) {
      // Apple II: strip high bit to get actual character
      uint8_t displayChar = value & 0x7F;
      textMemory[linearAddr] = displayChar;

      // Check if this write triggers a scroll (newline on last line)
      /*if (displayChar>=97 && displayChar<=122) {
          int row = getRowFromAddress(address);
          int col = getColumnFromAddress(address);

          printf("%i %i\n", row, col);
      }*/
    }
  }
}

uint8_t AppleIIVideo::readByte(uint16_t address) {
  if (address >= 0x400 && address < 0x800) {
    uint16_t linearAddr = screenAddrToLinear(address);
    if (linearAddr < 0x3C0) {
      return textMemory[linearAddr];
    }
  }
  return 0;
}

void AppleIIVideo::scrollUp() {
  // Apple II has 3 pages of 256 bytes each
  // Page 0 ($0400-$04FF): rows 0, 3, 6, 9, 12, 15, 18, 21
  // Page 1 ($0500-$05FF): rows 1, 4, 7, 10, 13, 16, 19, 22
  // Page 2 ($0600-$06FF): rows 2, 5, 8, 11, 14, 17, 20, 23

  static const uint16_t rowStarts[24] = {
      0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
      0x0428, 0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8,
      0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0};

  // For each page, shift rows within that page up by 1
  // We need to work in the interleaved format, not linear

  // Create a temporary buffer to store one row
  uint8_t tempRow[0x28]; // 40 bytes per row

  // Each page has 4 rows. Shift them within the page:
  // Rows 0,3,6,9,12,15,18,21 are in offsets 0-39, 128-167, 256-295, 384-423 of page 0

  for (int page = 0; page < 3; page++) {
    uint16_t pageBase = 0x0400 + (page * 0x100);

    // Within each page, there are 4 rows (at offsets 0, 128, 256, 384)
    for (int rowInPage = 0; rowInPage < 3; rowInPage++) {
      uint16_t currentRowOffset = pageBase + (rowInPage * 0x80);
      uint16_t nextRowOffset = pageBase + ((rowInPage + 1) * 0x80);

      // Copy row up: shift nextRow into currentRow
      memcpy((uint8_t *)textMemory + currentRowOffset,
             (uint8_t *)textMemory + nextRowOffset, 0x28);
    }

    // Clear the last row in this page
    uint16_t lastRowOffset = pageBase + (3 * 0x80);
    memset((uint8_t *)textMemory + lastRowOffset, 0x20, 0x28);
  }

  cursorPos = rowStarts[23]; // Move to row 23
}

void AppleIIVideo::display() {
  if (!cr)
    return;

  // Clear background to black
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_paint(cr);

  // Set text color to green
  cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);

  // Use monospace font
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 14);

  double char_width = 8.5;
  double line_height = 16.0;
  double x_offset = 2;
  double y_offset = 12;

  for (int i = 0; i < 0x3C0; i++) {
    int row = i / WIDTH;
    int col = i % WIDTH;

    uint8_t c = textMemory[i];

    // Filter control characters
    if (c < 32)
      c = ' ';
    if (c > 126)
      c = c & 0x7F;

    char buf[2] = {(char)c, 0};

    double x = x_offset + (col * char_width);
    double y = y_offset + (row * line_height);

    cairo_move_to(cr, x, y);
    cairo_show_text(cr, buf);
  }
}

void AppleIIVideo::clear() {
  memset(textMemory, 0x20, sizeof(textMemory));
  cursorPos = 0;
}

// ========== AppleIIKeyboard ==========

AppleIIKeyboard::AppleIIKeyboard() : lastKey(0), keyWaiting(false) {}

uint8_t AppleIIKeyboard::readKeyboard() { return lastKey; }

void AppleIIKeyboard::strobeKeyboard() {
  lastKey = lastKey & 0x7F;
  keyWaiting = false;
  debugLog << "Keyboard strobe: key cleared\n";
  debugLog.flush();
}

void AppleIIKeyboard::injectKey(uint8_t key) {
  // Convert special keys to Apple II codes
  if (key == '\n' || key == '\r') {
    key = '\r'; // Carriage return (0x0D)
  }

  // Set high bit to indicate key is available
  lastKey = key | 0x80;
  keyWaiting = true;
  debugLog << "Key injected: $" << std::hex << (int)lastKey << std::dec
           << " ('" << (char)(key & 0x7F) << "')\n";
  debugLog.flush();
}

void AppleIIKeyboard::checkForInput() {
  // Input will be handled by GTK event loop or ncurses
}
