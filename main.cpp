#include "cpu.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <ncurses.h>
#include <unistd.h>

std::ofstream debugLog;
AppleIIVideo *g_video;
AppleIIKeyboard *g_keyboard;
CPU6502 *g_cpu;
DiskII *g_disk;
bool g_running = true;
bool g_use_ncurses = false;

#ifdef WITH_GTK
#include <gtk/gtk.h>

gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
  g_video->initCairo(cr);
  g_video->display();
  return FALSE;
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
  uint8_t key = 0;
  bool shouldInject = false;

  if ((event->state & GDK_CONTROL_MASK) && (event->keyval == 'c' || event->keyval == 'C')) {
    g_cpu->requestIRQ();
    return TRUE;
  }

  if (event->keyval == GDK_KEY_Return) {
    key = '\r';
    shouldInject = true;
  } else if (event->keyval == GDK_KEY_BackSpace) {
    key = 0x08;
    shouldInject = true;
  } else if (event->keyval == GDK_KEY_Delete) {
    key = 0x7F;
    shouldInject = true;
  } else if (event->keyval < 128) {
    key = (uint8_t)event->keyval;
    shouldInject = true;
  }

  if (shouldInject) {
    g_keyboard->injectKey(key);
    gtk_widget_queue_draw(widget);
  }

  return TRUE;
}

gboolean cpu_tick(gpointer data) {
  const uint64_t CYCLES_PER_TICK = 20000;
  
  for (uint64_t i = 0; i < CYCLES_PER_TICK && g_running; i++) {
    g_cpu->executeInstruction();
  }
  
  gtk_widget_queue_draw((GtkWidget *)data);
  
  return g_running ? TRUE : FALSE;
}

void runGTK(int argc, char *argv[]) {
  gtk_init(&argc, &argv);

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Apple II Emulator");
  gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);

  GtkWidget *drawing_area = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER(window), drawing_area);

  g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw), NULL);
  g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  gtk_widget_set_can_focus(drawing_area, TRUE);
  gtk_widget_grab_focus(drawing_area);
  gtk_widget_show_all(window);

  g_timeout_add(16, cpu_tick, drawing_area);

  gtk_main();
}
#endif

class BasicSystem {
private:
  AppleIIVideo video;
  AppleIIKeyboard keyboard;
  DiskII diskController;
  CPU6502 cpu;
  std::ifstream inputFileStream;

public:
  BasicSystem() : cpu(&video, &keyboard, &diskController) {}

  bool loadROM(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "Error: Cannot open " << filename << "\n";
      return false;
    }

    debugLog.open("debug.log", std::ios::trunc);
    debugLog << "Loading ROM: " << filename << "\n";

    memset(cpu.ram, 0, sizeof(cpu.ram));

    for (uint16_t i = 0xC100; i < 0xD000; i++) {
      cpu.ram[i] = 0x60;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > 0x10000) {
      std::cerr << "Error: ROM too large\n";
      return false;
    }

    uint8_t buffer[0x10000];
    file.read((char *)buffer, size);
    file.close();

    const uint16_t LOAD = 0x10000 - size;

    for (size_t i = 0; i < size; i++) {
      cpu.ram[LOAD + i] = buffer[i];
    }

    uint16_t resetAddr = cpu.ram[0xFFFC] | (cpu.ram[0xFFFD] << 8);

    cpu.regPC = resetAddr;
    cpu.regSP = 0xFF;
    cpu.regP = 0x24;

    debugLog << "Loaded " << size << " bytes at $" << std::hex << LOAD
             << std::dec << "\n";
    debugLog << "Reset vector at $FFFC: $" << std::hex << resetAddr << std::dec
             << "\n";
    debugLog.flush();

    g_video = &video;
    g_keyboard = &keyboard;
    g_cpu = &cpu;
    g_disk = &diskController;

    return true;
  }

  bool loadDisk(int drive, const std::string &filename) {
    if (drive < 0 || drive >= 2) {
      std::cerr << "Error: Invalid drive number: " << drive << "\n";
      return false;
    }

    debugLog << "Loading disk " << drive << ": " << filename << "\n";
    debugLog.flush();

    if (!diskController.loadDisk(drive, filename)) {
      std::cerr << "Error: Failed to load disk: " << filename << "\n";
      return false;
    }

    return true;
  }

  void setInputFile(const std::string &filename) {
    inputFileStream.open(filename);
    if (!inputFileStream.is_open()) {
      std::cerr << "Warning: Could not open input file: " << filename << "\n";
    }
  }

  void runNCurses() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    set_escdelay(0);

    if (has_colors()) {
      start_color();
      init_pair(1, COLOR_GREEN, COLOR_BLACK);
      attron(COLOR_PAIR(1));
    }

    const uint64_t CYCLES_PER_FRAME = 20000;
    auto lastTime = std::chrono::high_resolution_clock::now();
    const auto FRAME_TIME = std::chrono::milliseconds(16);

    while (g_running) {
      int ch;
      while ((ch = getch()) != ERR) {
        if (ch == 3) { // Ctrl+C
          g_cpu->requestIRQ();
        } else if (ch == 17) { // Ctrl+Q to exit
          g_running = false;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
          g_keyboard->injectKey(0x08);
        } else if (ch == '\n' || ch == '\r') {
          g_keyboard->injectKey('\r');
        } else if (ch >= 32 && ch < 127) {
          g_keyboard->injectKey((uint8_t)ch);
        }
      }

      // Also check for input from file
      if (inputFileStream.is_open() && inputFileStream.peek() != EOF) {
        ch = inputFileStream.get();
        if (ch == '\n' || ch == '\r') {
          g_keyboard->injectKey('\r');
        } else if (ch >= 32 && ch < 127) {
          g_keyboard->injectKey((uint8_t)ch);
        }
      }

      for (uint64_t i = 0; i < CYCLES_PER_FRAME && g_running; i++) {
        g_cpu->executeInstruction();
      }

      erase();
      for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 40; col++) {
          int idx = row * 40 + col;
          uint8_t c = g_video->textMemory[idx];
          if (c < 32 || c > 126) c = ' ';
          mvaddch(row, col, c);
        }
      }
      refresh();

      auto now = std::chrono::high_resolution_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime);
      if (elapsed < FRAME_TIME) {
        std::this_thread::sleep_for(FRAME_TIME - elapsed);
      }
      lastTime = std::chrono::high_resolution_clock::now();
    }

    endwin();
  }

  void run(int argc, char *argv[]) {
    if (g_use_ncurses) {
      runNCurses();
    }
#ifdef WITH_GTK
    else {
      runGTK(argc, argv);
    }
#else
    else {
      // No GTK available, fall back to ncurses
      runNCurses();
    }
#endif
  }
};

int main(int argc, char *argv[]) {
  bool use_ncurses = false;
  int rom_idx = -1;
  std::string input_file = "";

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-ncurses") {
      use_ncurses = true;
      g_use_ncurses = true;
    } else if (arg == "-input" && i + 1 < argc) {
      input_file = argv[++i];
    } else if (arg[0] != '-' && rom_idx == -1) {
      rom_idx = i;
    }
  }

  BasicSystem system;

  if (rom_idx == -1) {
    std::cerr << "Usage: " << argv[0] << " [-ncurses] [-input file.bas] <rom.bin> [disk1.dsk] [disk2.dsk]\n";
    std::cerr << "Example: " << argv[0] << " appleii.rom dos33.dsk\n";
    std::cerr << "Example: " << argv[0] << " -ncurses -input hello.bas appleii.rom\n";
    return 1;
  }

  if (!system.loadROM(argv[rom_idx])) {
    return 1;
  }

  for (int i = rom_idx + 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg != "-ncurses" && arg != "-input") {
      int disk_num = i - rom_idx - 1;
      if (disk_num >= 2) break;
      if (!system.loadDisk(disk_num, arg)) {
        std::cerr << "Warning: Could not load disk " << (disk_num + 1) << "\n";
      }
    }
  }

  if (!input_file.empty()) {
    system.setInputFile(input_file);
  }

  system.run(argc, argv);
  return 0;
}
