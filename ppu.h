#ifndef PPU_HPP
#define PPU_HPP

#include <cstdint>
#include <cstring>
#include <cairo.h>
#include <gtk/gtk.h>
#include <queue>
#include <fstream>

extern std::ofstream debugLog;

class AppleIIVideo {
public:
    static const int WIDTH = 40;
    static const int HEIGHT = 24;
    
    uint8_t textMemory[0x400];
    cairo_surface_t* surface;
    cairo_t* cr;
    uint16_t cursorPos;

    AppleIIVideo() : surface(nullptr), cr(nullptr), cursorPos(0) {
        memset(textMemory, 0x20, sizeof(textMemory));
    }
    
    // Convert Apple II screen address to linear display buffer
    // Apple II uses interleaved memory: $0400-$04FF, $0500-$05FF, $0600-$06FF
    // $0400-$0427 = line 0, $0500-$0527 = line 8, $0600-$0627 = line 16
    // $0428-$044F = line 1, $0528-$054F = line 9, $0628-$064F = line 17, etc.
    uint16_t screenAddrToLinear(uint16_t screenAddr) {
        uint16_t offset = screenAddr - 0x400;
        
        // High byte determines which group of 8 lines
        uint16_t highBlock = (offset >> 8) & 3;  // 0, 1, or 2
        uint16_t lowOffset = offset & 0xFF;
        
        // Within the 256-byte block, which line (0-7)?
        uint16_t lineInBlock = lowOffset / 0x28;
        uint16_t col = lowOffset % 0x28;
        
        // Absolute line number
        uint16_t line = highBlock * 8 + lineInBlock;
        
        return line * 0x28 + col;
    }
    
    uint16_t linearToScreenAddr(uint16_t linearAddr) {
        return 0x400 + linearAddr;
    }

    void initCairo(cairo_t* cairo_ctx) {
        cr = cairo_ctx;
    }

    void writeByte(uint16_t address, uint8_t value) {
        if (address >= 0x400 && address < 0x800) {
            uint16_t linearAddr = screenAddrToLinear(address);
            if (linearAddr < 0x400) {
                uint16_t row = linearAddr / 0x28;  // 40 chars per line
                uint16_t col = linearAddr % 0x28;
                
                // Only log printable ASCII
                if (value >= 32 && value <= 126) {
                    debugLog << "SCREEN WRITE: addr=$" << std::hex << address << std::dec 
                             << " row=" << row << " col=" << col
                             << " char='" << (char)value << "' ($" << std::hex << (int)value << std::dec << ")\n";
                    debugLog.flush();
                }
                
                // Apple II: strip high bit to get actual character
                uint8_t displayChar = value & 0x7F;
                textMemory[linearAddr] = displayChar;
            }
        }
    }

    uint8_t readByte(uint16_t address) {
        if (address >= 0x400 && address < 0x800) {
            uint16_t linearAddr = screenAddrToLinear(address);
            if (linearAddr < 0x400) {
                return textMemory[linearAddr];
            }
        }
        return 0;
    }

    void scrollUp() {
        // Scroll text up by one line
        for (int i = 0; i < (HEIGHT - 1) * WIDTH; i++) {
            textMemory[i] = textMemory[i + WIDTH];
        }
        // Clear bottom line
        for (int i = (HEIGHT - 1) * WIDTH; i < HEIGHT * WIDTH; i++) {
            textMemory[i] = 0x20;
        }
    }

    void display() {
        if (!cr) return;

        // Clear background to black
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_paint(cr);

        // Set text color to green
        cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);

        // Use monospace font
        cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 14);

        double char_width = 8.5;
        double line_height = 16.0;
        double x_offset = 2;
        double y_offset = 12;

        for (int i = 0; i < 0x400; i++) {
            int row = i / WIDTH;
            int col = i % WIDTH;
            
            uint8_t c = textMemory[i];
            
            // Filter control characters
            if (c < 32) c = ' ';
            if (c > 126) c = c & 0x7F;

            char buf[2] = {(char)c, 0};
            
            double x = x_offset + (col * char_width);
            double y = y_offset + (row * line_height);
            
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, buf);
        }
    }

    void clear() {
        memset(textMemory, 0x20, sizeof(textMemory));
        cursorPos = 0;
    }
};

class AppleIIKeyboard {
private:
    uint8_t lastKey;
    bool keyWaiting;

public:
    AppleIIKeyboard() : lastKey(0), keyWaiting(false) {}

    uint8_t readKeyboard() {
        return lastKey;
    }

    void strobeKeyboard() {
        lastKey = lastKey & 0x7F;
        keyWaiting = false;
        debugLog << "Keyboard strobe: key cleared\n";
        debugLog.flush();
    }

    void injectKey(uint8_t key) {
        // Convert special keys to Apple II codes
        if (key == '\n' || key == '\r') {
            key = '\r';  // Carriage return (0x0D)
        }
        
        // Set high bit to indicate key is available
        lastKey = key | 0x80;
        keyWaiting = true;
        debugLog << "Key injected: $" << std::hex << (int)lastKey << std::dec 
                 << " ('" << (char)(key & 0x7F) << "')\n";
        debugLog.flush();
    }

    void checkForInput() {
        // Input will be handled by GTK event loop
    }
};

#endif
