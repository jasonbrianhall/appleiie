#include "cpu.h"
#include <fstream>

extern std::ofstream debugLog;

const uint8_t CPU6502::instructionCycles[256] = {
    7, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 6, 0, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,
    2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 5, 0, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7
};

// Memory access
uint8_t CPU6502::readByte(uint16_t address) {
    // Log reads from the work buffer around $0580
    if (address >= 0x0580 && address < 0x0600) {
        uint8_t val = ram[address];
        if (val >= 32 && val <= 126) {
            debugLog << "RAM READ: addr=$" << std::hex << address << std::dec 
                     << " value=$" << std::hex << (int)val << std::dec
                     << " char='" << (char)val << "'\n";
            debugLog.flush();
        }
    }
    
    // Keyboard input
    if (address == 0xC000 || address == 0xC001) {
        return keyboard->readKeyboard();
    }
    // Video memory reads
    if (address >= 0x400 && address < 0x800) {
        return video->readByte(address);
    }
    return ram[address];
}

void CPU6502::writeByte(uint16_t address, uint8_t value) {
    // Log only writes to the suspicious range
    if (address >= 0x0580 && address < 0x0600) {
        debugLog << "WRITE: addr=$" << std::hex << address << std::dec 
                 << " value=$" << std::hex << (int)value << std::dec 
                 << " (" << (char)value << ")\n";
        debugLog.flush();
    }
    
    // Keyboard strobe
    if (address == 0xC010 || address == 0xC011) { 
        keyboard->strobeKeyboard(); 
        return; 
    }
    // Video memory
    if (address >= 0x400 && address < 0x800) { 
        video->writeByte(address, value); 
        return; 
    }
    ram[address] = value;
}
uint16_t CPU6502::readWord(uint16_t address) {
    return readByte(address) | (readByte(address + 1) << 8);
}

void CPU6502::writeWord(uint16_t address, uint16_t value) {
    writeByte(address, value & 0xFF);
    writeByte(address + 1, value >> 8);
}

// Stack
void CPU6502::pushByte(uint8_t value) { writeByte(0x100 + regSP, value); regSP--; }
uint8_t CPU6502::pullByte() { regSP++; return readByte(0x100 + regSP); }
void CPU6502::pushWord(uint16_t value) { pushByte(value >> 8); pushByte(value & 0xFF); }
uint16_t CPU6502::pullWord() { uint8_t lo = pullByte(); uint8_t hi = pullByte(); return lo | (hi << 8); }

// Fetch
uint8_t CPU6502::fetchByte() { return readByte(regPC++); }
uint16_t CPU6502::fetchWord() { uint16_t value = readWord(regPC); regPC += 2; return value; }

// Flags
void CPU6502::setFlag(uint8_t flag, bool value) { if (value) regP |= flag; else regP &= ~flag; }
bool CPU6502::getFlag(uint8_t flag) const { return (regP & flag) != 0; }
void CPU6502::updateZN(uint8_t value) { setFlag(FLAG_ZERO, value == 0); setFlag(FLAG_NEGATIVE, (value & 0x80) != 0); }

// Addressing modes
uint16_t CPU6502::addrImmediate() { return regPC++; }
uint16_t CPU6502::addrZeroPage() { return fetchByte(); }
uint16_t CPU6502::addrZeroPageX() { return (fetchByte() + regX) & 0xFF; }
uint16_t CPU6502::addrZeroPageY() { return (fetchByte() + regY) & 0xFF; }
uint16_t CPU6502::addrAbsolute() { return fetchWord(); }
uint16_t CPU6502::addrAbsoluteX() { return fetchWord() + regX; }
uint16_t CPU6502::addrAbsoluteY() { return fetchWord() + regY; }
uint16_t CPU6502::addrIndirect() {
    uint16_t addr = fetchWord();
    if ((addr & 0xFF) == 0xFF) return readByte(addr) | (readByte(addr & 0xFF00) << 8);
    return readWord(addr);
}
uint16_t CPU6502::addrIndirectX() {
    uint8_t addr = (fetchByte() + regX) & 0xFF;
    return readByte(addr) | (readByte((addr + 1) & 0xFF) << 8);
}
uint16_t CPU6502::addrIndirectY() {
    uint8_t addr = fetchByte();
    uint16_t base = readByte(addr) | (readByte((addr + 1) & 0xFF) << 8);
    return base + regY;
}
uint16_t CPU6502::addrRelative() { int8_t offset = fetchByte(); return regPC + offset; }

// Instructions
void CPU6502::ADC(uint16_t addr) { uint8_t v = readByte(addr); uint16_t r = regA + v + (getFlag(FLAG_CARRY) ? 1 : 0); setFlag(FLAG_CARRY, r > 0xFF); setFlag(FLAG_OVERFLOW, ((regA ^ r) & (v ^ r) & 0x80) != 0); regA = r & 0xFF; updateZN(regA); }
void CPU6502::AND(uint16_t addr) { regA &= readByte(addr); updateZN(regA); }
void CPU6502::ASL(uint16_t addr) { uint8_t v = readByte(addr); setFlag(FLAG_CARRY, (v & 0x80) != 0); v <<= 1; writeByte(addr, v); updateZN(v); }
void CPU6502::ASL_ACC() { setFlag(FLAG_CARRY, (regA & 0x80) != 0); regA <<= 1; updateZN(regA); }
void CPU6502::BCC() { if (!getFlag(FLAG_CARRY)) regPC = addrRelative(); else regPC++; }
void CPU6502::BCS() { if (getFlag(FLAG_CARRY)) regPC = addrRelative(); else regPC++; }
void CPU6502::BEQ() { if (getFlag(FLAG_ZERO)) regPC = addrRelative(); else regPC++; }
void CPU6502::BIT(uint16_t addr) { uint8_t v = readByte(addr); setFlag(FLAG_ZERO, (regA & v) == 0); setFlag(FLAG_OVERFLOW, (v & 0x40) != 0); setFlag(FLAG_NEGATIVE, (v & 0x80) != 0); }
void CPU6502::BMI() { if (getFlag(FLAG_NEGATIVE)) regPC = addrRelative(); else regPC++; }
void CPU6502::BNE() { if (!getFlag(FLAG_ZERO)) regPC = addrRelative(); else regPC++; }
void CPU6502::BPL() { if (!getFlag(FLAG_NEGATIVE)) regPC = addrRelative(); else regPC++; }
void CPU6502::BRK() { regPC++; pushWord(regPC); pushByte(regP | FLAG_BREAK); setFlag(FLAG_INTERRUPT, true); regPC = readWord(0xFFFE); }
void CPU6502::BVC() { if (!getFlag(FLAG_OVERFLOW)) regPC = addrRelative(); else regPC++; }
void CPU6502::BVS() { if (getFlag(FLAG_OVERFLOW)) regPC = addrRelative(); else regPC++; }
void CPU6502::CLC() { setFlag(FLAG_CARRY, false); }
void CPU6502::CLD() { setFlag(FLAG_DECIMAL, false); }
void CPU6502::CLI() { setFlag(FLAG_INTERRUPT, false); }
void CPU6502::CLV() { setFlag(FLAG_OVERFLOW, false); }
void CPU6502::CMP(uint16_t addr) { uint8_t v = readByte(addr); uint8_t r = regA - v; setFlag(FLAG_CARRY, regA >= v); updateZN(r); }
void CPU6502::CPX(uint16_t addr) { uint8_t v = readByte(addr); uint8_t r = regX - v; setFlag(FLAG_CARRY, regX >= v); updateZN(r); }
void CPU6502::CPY(uint16_t addr) { uint8_t v = readByte(addr); uint8_t r = regY - v; setFlag(FLAG_CARRY, regY >= v); updateZN(r); }
void CPU6502::DEC(uint16_t addr) { uint8_t v = readByte(addr) - 1; writeByte(addr, v); updateZN(v); }
void CPU6502::DEX() { regX--; updateZN(regX); }
void CPU6502::DEY() { regY--; updateZN(regY); }
void CPU6502::EOR(uint16_t addr) { regA ^= readByte(addr); updateZN(regA); }
void CPU6502::INC(uint16_t addr) { uint8_t v = readByte(addr) + 1; writeByte(addr, v); updateZN(v); }
void CPU6502::INX() { regX++; updateZN(regX); }
void CPU6502::INY() { regY++; updateZN(regY); }
void CPU6502::JMP(uint16_t addr) { regPC = addr; }
void CPU6502::JSR(uint16_t addr) { pushWord(regPC - 1); regPC = addr; }
void CPU6502::LDA(uint16_t addr) { regA = readByte(addr); updateZN(regA); }
void CPU6502::LDX(uint16_t addr) { regX = readByte(addr); updateZN(regX); }
void CPU6502::LDY(uint16_t addr) { regY = readByte(addr); updateZN(regY); }
void CPU6502::LSR(uint16_t addr) { uint8_t v = readByte(addr); setFlag(FLAG_CARRY, (v & 0x01) != 0); v >>= 1; writeByte(addr, v); updateZN(v); }
void CPU6502::LSR_ACC() { setFlag(FLAG_CARRY, (regA & 0x01) != 0); regA >>= 1; updateZN(regA); }
void CPU6502::NOP() {}
void CPU6502::ORA(uint16_t addr) { regA |= readByte(addr); updateZN(regA); }
void CPU6502::PHA() { pushByte(regA); }
void CPU6502::PHP() { pushByte(regP | FLAG_BREAK | FLAG_UNUSED); }
void CPU6502::PLA() { regA = pullByte(); updateZN(regA); }
void CPU6502::PLP() { regP = pullByte() | FLAG_UNUSED; regP &= ~FLAG_BREAK; }
void CPU6502::ROL(uint16_t addr) { uint8_t v = readByte(addr); bool c = getFlag(FLAG_CARRY); setFlag(FLAG_CARRY, (v & 0x80) != 0); v = (v << 1) | (c ? 1 : 0); writeByte(addr, v); updateZN(v); }
void CPU6502::ROL_ACC() { bool c = getFlag(FLAG_CARRY); setFlag(FLAG_CARRY, (regA & 0x80) != 0); regA = (regA << 1) | (c ? 1 : 0); updateZN(regA); }
void CPU6502::ROR(uint16_t addr) { uint8_t v = readByte(addr); bool c = getFlag(FLAG_CARRY); setFlag(FLAG_CARRY, (v & 0x01) != 0); v = (v >> 1) | (c ? 0x80 : 0); writeByte(addr, v); updateZN(v); }
void CPU6502::ROR_ACC() { bool c = getFlag(FLAG_CARRY); setFlag(FLAG_CARRY, (regA & 0x01) != 0); regA = (regA >> 1) | (c ? 0x80 : 0); updateZN(regA); }
void CPU6502::RTI() { regP = pullByte() | FLAG_UNUSED; regP &= ~FLAG_BREAK; regPC = pullWord(); }
void CPU6502::RTS() { regPC = pullWord() + 1; }
void CPU6502::SBC(uint16_t addr) { uint8_t v = readByte(addr); uint16_t r = regA - v - (getFlag(FLAG_CARRY) ? 0 : 1); setFlag(FLAG_CARRY, r <= 0xFF); setFlag(FLAG_OVERFLOW, ((regA ^ r) & (~v ^ r) & 0x80) != 0); regA = r & 0xFF; updateZN(regA); }
void CPU6502::SEC() { setFlag(FLAG_CARRY, true); }
void CPU6502::SED() { setFlag(FLAG_DECIMAL, true); }
void CPU6502::SEI() { setFlag(FLAG_INTERRUPT, true); }
void CPU6502::STA(uint16_t addr) { writeByte(addr, regA); }
void CPU6502::STX(uint16_t addr) { writeByte(addr, regX); }
void CPU6502::STY(uint16_t addr) { writeByte(addr, regY); }
void CPU6502::TAX() { regX = regA; updateZN(regX); }
void CPU6502::TAY() { regY = regA; updateZN(regY); }
void CPU6502::TSX() { regX = regSP; updateZN(regX); }
void CPU6502::TXA() { regA = regX; updateZN(regA); }
void CPU6502::TXS() { regSP = regX; }
void CPU6502::TYA() { regA = regY; updateZN(regA); }

void CPU6502::executeInstruction() {
    uint8_t opcode = fetchByte();
    uint8_t cycles = instructionCycles[opcode];
    totalCycles += cycles;

    //if (nmiRequested) {printf("NMI\n"); nmiRequested=false;}
    //if (irqRequested) {printf("IRQ\n"); irqRequested=false;}

/*    if (nmiRequested) {
        nmiRequested = false;
        pushWord(regPC);
        pushByte(regP | FLAG_UNUSED);
        setFlag(FLAG_INTERRUPT, true);
        regPC = readWord(0xFFFA);
        totalCycles += 7;
        return;
    }

    // Handle IRQ (only if interrupt flag is clear)
    if (irqRequested && !getFlag(FLAG_INTERRUPT)) {
        irqRequested = false;
        pushWord(regPC);
        pushByte(regP | FLAG_UNUSED);
        setFlag(FLAG_INTERRUPT, true);
        regPC = readWord(0xFFFE);
        totalCycles += 7;
        return;
    }*/

    if (nmiRequested) {
        printf("NMI Requested\n");
        nmiRequested = false;
        pushWord(regPC);
        pushByte(regP | FLAG_UNUSED);
        setFlag(FLAG_INTERRUPT, true);
        regPC = readWord(0xFFFA);
        totalCycles += 7;
        return;
    }

    //if (irqRequested && !getFlag(FLAG_INTERRUPT)) {
    if (irqRequested) {
        printf("IRQ Requested\n");

        irqRequested = false;
        pushWord(regPC);
        pushByte(regP | FLAG_UNUSED);
        setFlag(FLAG_INTERRUPT, true);
        regPC = readWord(0xFFFE);
        printf("IRQ vector = $%04X\n", regPC);
        totalCycles += 7;
        return;
    }


    switch (opcode) {
    case 0x69: ADC(addrImmediate()); break; case 0x65: ADC(addrZeroPage()); break; case 0x75: ADC(addrZeroPageX()); break;
    case 0x6D: ADC(addrAbsolute()); break; case 0x7D: ADC(addrAbsoluteX()); break; case 0x79: ADC(addrAbsoluteY()); break;
    case 0x61: ADC(addrIndirectX()); break; case 0x71: ADC(addrIndirectY()); break;
    case 0x29: AND(addrImmediate()); break; case 0x25: AND(addrZeroPage()); break; case 0x35: AND(addrZeroPageX()); break;
    case 0x2D: AND(addrAbsolute()); break; case 0x3D: AND(addrAbsoluteX()); break; case 0x39: AND(addrAbsoluteY()); break;
    case 0x21: AND(addrIndirectX()); break; case 0x31: AND(addrIndirectY()); break;
    case 0x0A: ASL_ACC(); break; case 0x06: ASL(addrZeroPage()); break; case 0x16: ASL(addrZeroPageX()); break;
    case 0x0E: ASL(addrAbsolute()); break; case 0x1E: ASL(addrAbsoluteX()); break;
    case 0x90: BCC(); break; case 0xB0: BCS(); break; case 0xF0: BEQ(); break; case 0x30: BMI(); break;
    case 0xD0: BNE(); break; case 0x10: BPL(); break; case 0x50: BVC(); break; case 0x70: BVS(); break;
    case 0x24: BIT(addrZeroPage()); break; case 0x2C: BIT(addrAbsolute()); break;
    case 0x00: BRK(); break;
    case 0x18: CLC(); break; case 0xD8: CLD(); break; case 0x58: CLI(); break; case 0xB8: CLV(); break;
    case 0xC9: CMP(addrImmediate()); break; case 0xC5: CMP(addrZeroPage()); break; case 0xD5: CMP(addrZeroPageX()); break;
    case 0xCD: CMP(addrAbsolute()); break; case 0xDD: CMP(addrAbsoluteX()); break; case 0xD9: CMP(addrAbsoluteY()); break;
    case 0xC1: CMP(addrIndirectX()); break; case 0xD1: CMP(addrIndirectY()); break;
    case 0xE0: CPX(addrImmediate()); break; case 0xE4: CPX(addrZeroPage()); break; case 0xEC: CPX(addrAbsolute()); break;
    case 0xC0: CPY(addrImmediate()); break; case 0xC4: CPY(addrZeroPage()); break; case 0xCC: CPY(addrAbsolute()); break;
    case 0xC6: DEC(addrZeroPage()); break; case 0xD6: DEC(addrZeroPageX()); break; case 0xCE: DEC(addrAbsolute()); break;
    case 0xDE: DEC(addrAbsoluteX()); break;
    case 0xCA: DEX(); break; case 0x88: DEY(); break;
    case 0x49: EOR(addrImmediate()); break; case 0x45: EOR(addrZeroPage()); break; case 0x55: EOR(addrZeroPageX()); break;
    case 0x4D: EOR(addrAbsolute()); break; case 0x5D: EOR(addrAbsoluteX()); break; case 0x59: EOR(addrAbsoluteY()); break;
    case 0x41: EOR(addrIndirectX()); break; case 0x51: EOR(addrIndirectY()); break;
    case 0xE6: INC(addrZeroPage()); break; case 0xF6: INC(addrZeroPageX()); break; case 0xEE: INC(addrAbsolute()); break;
    case 0xFE: INC(addrAbsoluteX()); break;
    case 0xE8: INX(); break; case 0xC8: INY(); break;
    case 0x4C: JMP(addrAbsolute()); break; case 0x6C: JMP(addrIndirect()); break;
    case 0x20: JSR(addrAbsolute()); break;
    case 0xA9: LDA(addrImmediate()); break; case 0xA5: LDA(addrZeroPage()); break; case 0xB5: LDA(addrZeroPageX()); break;
    case 0xAD: LDA(addrAbsolute()); break; case 0xBD: LDA(addrAbsoluteX()); break; case 0xB9: LDA(addrAbsoluteY()); break;
    case 0xA1: LDA(addrIndirectX()); break; case 0xB1: LDA(addrIndirectY()); break;
    case 0xA2: LDX(addrImmediate()); break; case 0xA6: LDX(addrZeroPage()); break; case 0xB6: LDX(addrZeroPageY()); break;
    case 0xAE: LDX(addrAbsolute()); break; case 0xBE: LDX(addrAbsoluteY()); break;
    case 0xA0: LDY(addrImmediate()); break; case 0xA4: LDY(addrZeroPage()); break; case 0xB4: LDY(addrZeroPageX()); break;
    case 0xAC: LDY(addrAbsolute()); break; case 0xBC: LDY(addrAbsoluteX()); break;
    case 0x4A: LSR_ACC(); break; case 0x46: LSR(addrZeroPage()); break; case 0x56: LSR(addrZeroPageX()); break;
    case 0x4E: LSR(addrAbsolute()); break; case 0x5E: LSR(addrAbsoluteX()); break;
    case 0xEA: NOP(); break;
    case 0x09: ORA(addrImmediate()); break; case 0x05: ORA(addrZeroPage()); break; case 0x15: ORA(addrZeroPageX()); break;
    case 0x0D: ORA(addrAbsolute()); break; case 0x1D: ORA(addrAbsoluteX()); break; case 0x19: ORA(addrAbsoluteY()); break;
    case 0x01: ORA(addrIndirectX()); break; case 0x11: ORA(addrIndirectY()); break;
    case 0x48: PHA(); break; case 0x08: PHP(); break; case 0x68: PLA(); break; case 0x28: PLP(); break;
    case 0x2A: ROL_ACC(); break; case 0x26: ROL(addrZeroPage()); break; case 0x36: ROL(addrZeroPageX()); break;
    case 0x2E: ROL(addrAbsolute()); break; case 0x3E: ROL(addrAbsoluteX()); break;
    case 0x6A: ROR_ACC(); break; case 0x66: ROR(addrZeroPage()); break; case 0x76: ROR(addrZeroPageX()); break;
    case 0x6E: ROR(addrAbsolute()); break; case 0x7E: ROR(addrAbsoluteX()); break;
    case 0x40: RTI(); break;
    case 0x60: RTS(); break;
    case 0xE9: SBC(addrImmediate()); break; case 0xE5: SBC(addrZeroPage()); break; case 0xF5: SBC(addrZeroPageX()); break;
    case 0xED: SBC(addrAbsolute()); break; case 0xFD: SBC(addrAbsoluteX()); break; case 0xF9: SBC(addrAbsoluteY()); break;
    case 0xE1: SBC(addrIndirectX()); break; case 0xF1: SBC(addrIndirectY()); break;
    case 0x38: SEC(); break; case 0xF8: SED(); break; case 0x78: SEI(); break;
    case 0x85: STA(addrZeroPage()); break; case 0x95: STA(addrZeroPageX()); break; case 0x8D: STA(addrAbsolute()); break;
    case 0x9D: STA(addrAbsoluteX()); break; case 0x99: STA(addrAbsoluteY()); break; case 0x81: STA(addrIndirectX()); break;
    case 0x91: STA(addrIndirectY()); break;
    case 0x86: STX(addrZeroPage()); break; case 0x96: STX(addrZeroPageY()); break; case 0x8E: STX(addrAbsolute()); break;
    case 0x84: STY(addrZeroPage()); break; case 0x94: STY(addrZeroPageX()); break; case 0x8C: STY(addrAbsolute()); break;
    case 0xAA: TAX(); break; case 0xA8: TAY(); break; case 0xBA: TSX(); break; case 0x8A: TXA(); break;
    case 0x9A: TXS(); break; case 0x98: TYA(); break;
    default: break;
    }
}
