# Apple II Emulator

A 6502 CPU emulator designed to run Apple II 2e firmware and BASIC programs. This project provides a functional emulation of the classic Apple II computer with a graphical text display powered by GTK.

## Features

- **6502 CPU Emulation**: Full implementation of the 6502 instruction set used in Apple II computers
- **Text Display**: 40x24 character text mode rendering with authentic Apple II memory layout
- **Keyboard Input**: Real-time keyboard input mapped to Apple II keyboard codes
- **GTK UI**: Lightweight graphical interface for viewing screen output and providing input
- **ROM Loading**: Load and execute Apple II firmware ROM files
- **BASIC Support**: Load and run Apple II BASIC programs

## Building

### Prerequisites

- G++ (C++17 support)
- GTK+ 3.0 development libraries
- pkg-config

### Compilation

```bash
./build.sh
```

This generates an executable `appleiie`.

## Usage

```bash
./appleiie <rom.bin> [basic_program.bin]
```

### Arguments

- `<rom.bin>` (required): The Apple II firmware ROM file to boot
- `[basic_program.bin]` (optional): A BASIC program to load into memory at address $0801

### Example

```bash
./a.out apple2e.rom hello.bas
```

## Controls

- **Regular Keys**: Type to inject keys into the emulated system
- **Enter**: Send carriage return
- **Backspace**: Send backspace character
- **Delete**: Send delete character
- **Ctrl+C**: Trigger IRQ interrupt

## Architecture

### Components

- **CPU6502**: Main processor implementation with all 6502 instructions, addressing modes, and interrupt handling
- **AppleIIVideo**: Text screen memory management and rendering with Cairo graphics library
- **AppleIIKeyboard**: Keyboard input handling with Apple II protocol compatibility
- **Memory**: 64KB addressable RAM with ROM area, I/O addresses, and video memory

### Memory Layout

- `$0000-$00FF`: Zero page
- `$0100-$01FF`: Stack
- `$0200-$03FF`: General purpose RAM
- `$0400-$07FF`: Text screen memory (40x24 characters)
- `$C000-$C0FF`: I/O addresses (keyboard, display, etc.)
- `$C100-$CFFF`: Peripheral slot ROM areas
- `$D000-$FFFF`: Firmware ROM

### Key Features

- **Instruction Timing**: Accurate cycle counts for each 6502 instruction
- **Interrupts**: Supports both NMI (non-maskable interrupt) and IRQ (interrupt request)
- **Status Flags**: Complete flag register implementation (Carry, Zero, Interrupt, Decimal, Break, Overflow, Negative)

## Debugging

The emulator generates a `debug.log` file containing:
- ROM loading information
- Memory access patterns
- CPU state when stuck or halted
- Interrupt events
- Keyboard input logging

Monitor this file to troubleshoot program execution.

## Limitations

- No disk support (no Disk II emulation)
- Text mode only (no graphics modes)
- No audio support
- No peripheral card support beyond basic I/O
