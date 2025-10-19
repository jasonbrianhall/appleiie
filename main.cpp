#include "cpu.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <gtk/gtk.h>
#include <iostream>
#include <string>
#include <thread>

std::ofstream debugLog;
AppleIIVideo *g_video;
AppleIIKeyboard *g_keyboard;
CPU6502 *g_cpu;
DiskII *g_disk;
bool g_running = true;

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

static uint16_t lastPC = 0;
static int sameCount = 0;

gboolean cpu_tick(gpointer data) {
  GtkWidget *widget = (GtkWidget *)data;

  const uint64_t CYCLES_PER_TICK = 20000;

  for (uint64_t i = 0; i < CYCLES_PER_TICK && g_running; i++) {
    g_cpu->executeInstruction();

    if (g_cpu->ram[g_cpu->regPC] == 0x00) {
      debugLog << "[BRK at $" << std::hex << g_cpu->regPC << std::dec << "]\n";
      debugLog.flush();
      g_running = false;
      break;
    }
  }

  // Check if CPU is stuck
  if (g_cpu->regPC == lastPC) {
    sameCount++;
    if (sameCount > 60) { // About 1 second at 60 FPS
      debugLog << "CPU appears stuck at $" << std::hex << g_cpu->regPC
               << std::dec << "\n";
      debugLog << "  A=$" << std::hex << (int)g_cpu->regA << " X=$"
               << (int)g_cpu->regX << " Y=$" << (int)g_cpu->regY << " SP=$"
               << (int)g_cpu->regSP << std::dec << "\n";
      debugLog.flush();
      sameCount = 0;
    }
  } else {
    sameCount = 0;
  }
  lastPC = g_cpu->regPC;

  gtk_widget_queue_draw(widget);

  // Keep running until CPU stops
  return g_running ? TRUE : FALSE;
}

class BasicSystem {
private:
  AppleIIVideo video;
  AppleIIKeyboard keyboard;
  DiskII diskController;
  CPU6502 cpu;

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

    // Fill slot areas with RTS (0x60) to safely return from slot probes
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

  void run(int argc, char *argv[]) {
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

    // Start CPU tick timer (every 16ms ≈ 60 FPS)
    g_timeout_add(16, cpu_tick, drawing_area);

    gtk_main();
  }
};

int main(int argc, char *argv[]) {
  gtk_init(&argc, &argv);

  BasicSystem system;

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <rom.bin> [disk_drive1.dsk] [disk_drive2.dsk]\n";
    std::cerr << "Example: " << argv[0] << " appleii.rom dos33.dsk\n";
    return 1;
  }

  if (!system.loadROM(argv[1])) {
    return 1;
  }

  // Load disk images if provided
  if (argc > 2) {
    if (!system.loadDisk(0, argv[2])) {
      std::cerr << "Warning: Could not load disk 1\n";
    }
  }

  if (argc > 3) {
    if (!system.loadDisk(1, argv[3])) {
      std::cerr << "Warning: Could not load disk 2\n";
    }
  }

  system.run(argc, argv);
  return 0;
}
