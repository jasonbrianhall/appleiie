#ifndef CPU_HPP
#define CPU_HPP

#include <cstdint>
#include <cstring>
#include "ppu.h"
#include "disk.h"

class CPU6502 {
public:
    uint8_t regA, regX, regY, regSP;
    uint16_t regPC;
    uint8_t regP;
    uint8_t ram[65536];
    uint64_t totalCycles;

    AppleIIVideo* video;
    AppleIIKeyboard* keyboard;
    DiskII* diskController;

    bool irqRequested = false;
    bool nmiRequested = false;

    // Constructor WITH disk support
    CPU6502(AppleIIVideo* v, AppleIIKeyboard* k, DiskII* d)
        : regA(0), regX(0), regY(0), regSP(0xFF), regPC(0xD000), regP(0x24),
          totalCycles(0), video(v), keyboard(k), diskController(d) {
        memset(ram, 0, sizeof(ram));
    }

    enum StatusFlags {
        FLAG_CARRY = 0x01,
        FLAG_ZERO = 0x02,
        FLAG_INTERRUPT = 0x04,
        FLAG_DECIMAL = 0x08,
        FLAG_BREAK = 0x10,
        FLAG_UNUSED = 0x20,
        FLAG_OVERFLOW = 0x40,
        FLAG_NEGATIVE = 0x80
    };

    static const uint8_t instructionCycles[256];

    uint8_t readByte(uint16_t address);
    void writeByte(uint16_t address, uint8_t value);
    uint16_t readWord(uint16_t address);
    void writeWord(uint16_t address, uint16_t value);

    void pushByte(uint8_t value);
    uint8_t pullByte();
    void pushWord(uint16_t value);
    uint16_t pullWord();

    uint8_t fetchByte();
    uint16_t fetchWord();

    void setFlag(uint8_t flag, bool value);
    bool getFlag(uint8_t flag) const;
    void updateZN(uint8_t value);

    // Addressing modes
    uint16_t addrImmediate();
    uint16_t addrZeroPage();
    uint16_t addrZeroPageX();
    uint16_t addrZeroPageY();
    uint16_t addrAbsolute();
    uint16_t addrAbsoluteX();
    uint16_t addrAbsoluteY();
    uint16_t addrIndirect();
    uint16_t addrIndirectX();
    uint16_t addrIndirectY();
    uint16_t addrRelative();

    // Instructions
    void ADC(uint16_t addr);
    void AND(uint16_t addr);
    void ASL(uint16_t addr);
    void ASL_ACC();
    void BCC();
    void BCS();
    void BEQ();
    void BIT(uint16_t addr);
    void BMI();
    void BNE();
    void BPL();
    void BRK();
    void BVC();
    void BVS();
    void CLC();
    void CLD();
    void CLI();
    void CLV();
    void CMP(uint16_t addr);
    void CPX(uint16_t addr);
    void CPY(uint16_t addr);
    void DEC(uint16_t addr);
    void DEX();
    void DEY();
    void EOR(uint16_t addr);
    void INC(uint16_t addr);
    void INX();
    void INY();
    void JMP(uint16_t addr);
    void JSR(uint16_t addr);
    void LDA(uint16_t addr);
    void LDX(uint16_t addr);
    void LDY(uint16_t addr);
    void LSR(uint16_t addr);
    void LSR_ACC();
    void NOP();
    void ORA(uint16_t addr);
    void PHA();
    void PHP();
    void PLA();
    void PLP();
    void ROL(uint16_t addr);
    void ROL_ACC();
    void ROR(uint16_t addr);
    void ROR_ACC();
    void RTI();
    void RTS();
    void SBC(uint16_t addr);
    void SEC();
    void SED();
    void SEI();
    void STA(uint16_t addr);
    void STX(uint16_t addr);
    void STY(uint16_t addr);
    void TAX();
    void TAY();
    void TSX();
    void TXA();
    void TXS();
    void TYA();

    void executeInstruction();


    void requestIRQ() { irqRequested = true; }
    void requestNMI() { nmiRequested = true; }

};

#endif
