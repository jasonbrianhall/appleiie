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

int getRowFromAddress(uint16_t address) {
    static const uint16_t rowStarts[24] = {
        0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
        0x0428, 0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8,
        0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0
        
    };

    for (int row = 0; row < 24; ++row) {
        uint16_t start = rowStarts[row];
        if (address >= start && address < start + 0x28) {
            return row;
        }
    }

    return -1; // Not a valid screen address
}

int getColumnFromAddress(uint16_t address) {
    static const uint16_t rowStarts[24] = {
        0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
        0x0428, 0x04A8, 0x0528, 0x05A8, 0x0628, 0x06A8, 0x0728, 0x07A8,
        0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750, 0x07D0
    };

    for (int row = 0; row < 24; ++row) {
        uint16_t start = rowStarts[row];
        if (address >= start && address < start + 0x28) {
            return address - start;  // Column is offset from row start
        }
    }

    return -1; // Not a valid screen address
}


    AppleIIVideo() : surface(nullptr), cr(nullptr), cursorPos(0) {
        memset(textMemory, 0x20, sizeof(textMemory));
    }
    
    uint16_t screenAddrToLinear(uint16_t screenAddr) {
        // Apple II text screen is interleaved by rows across three 256-byte pages:
        // Page 0 ($0400-$04FF): rows 0, 3, 6, 9, 12, 15, 18, 21
        // Page 1 ($0500-$05FF): rows 1, 4, 7, 10, 13, 16, 19, 22
        // Page 2 ($0600-$06FF): rows 2, 5, 8, 11, 14, 17, 20, 23
        //
        // Within each page, 40 bytes per row
        
        if (screenAddr < 0x400 || screenAddr >= 0x800) return 0;
        
        uint16_t offset = screenAddr - 0x400;
        
        // Which 256-byte page (0, 1, or 2)
        uint16_t page = (offset >> 8);
        
        // Position within that page
        uint16_t posInPage = offset & 0xFF;
        
        // Which row within the page (0-7), since each row is 40 bytes
        uint16_t rowInPage = posInPage / 0x28;
        
        // Column within the row (0-39)
        uint16_t col = posInPage % 0x28;
        
        // Absolute row: page determines the interleaving pattern
        // Page 0: rows 0, 3, 6, 9, 12, 15, 18, 21
        // Page 1: rows 1, 4, 7, 10, 13, 16, 19, 22
        // Page 2: rows 2, 5, 8, 11, 14, 17, 20, 23
        uint16_t row = rowInPage * 3 + page;
        
        // Convert to linear position (row * 40 + col)
        uint16_t linear = row * 0x28 + col;
        
        return linear;
    }

    void initCairo(cairo_t* cairo_ctx) {
        cr = cairo_ctx;
    }

    void writeByte(uint16_t address, uint8_t value) {
        if (address >= 0x400 && address < 0x800) {
            uint16_t linearAddr = screenAddrToLinear(address);
            
            // Screen is 24 rows * 40 cols = 0x3C0 bytes max
            if (linearAddr < 0x3C0) {
                // Apple II: strip high bit to get actual character
                uint8_t displayChar = value & 0x7F;
                textMemory[linearAddr] = displayChar;
                
                // Log only printable ASCII
if (displayChar >= 97 && displayChar <= 122) {
    uint16_t row=0;
    uint16_t col = 0;

            
    
    

    printf("row %i %x %i\n", getRowFromAddress(address), address, getColumnFromAddress(address));
    debugLog << "SCREEN WRITE: addr=$" << std::hex << address << std::dec 
             << " linear=$" << std::hex << linearAddr << std::dec
             << " row=" << row << " col=" << col
             << " char='" << (char)displayChar << "'\n";
    debugLog.flush();
}
            } else {
                debugLog << "WARNING: screenAddrToLinear($" << std::hex << address << std::dec 
                         << ") returned out-of-bounds linear=$" << std::hex << linearAddr << std::dec << "\n";
                debugLog.flush();
            }
        }
    }

    uint8_t readByte(uint16_t address) {
        if (address >= 0x400 && address < 0x800) {
            uint16_t linearAddr = screenAddrToLinear(address);
            if (linearAddr < 0x3C0) {
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

        for (int i = 0; i < 0x3C0; i++) {
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
