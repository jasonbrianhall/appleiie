// disk.h - Apple II Disk II Controller Emulation
#ifndef DISK_H
#define DISK_H

#include <cstdint>
#include <cstring>
#include <fstream>

class DiskII {
public:
    static const int NUM_DRIVES = 2;
    static const int DOS_NUM_SECTORS = 16;
    static const int DOS_NUM_TRACKS = 35;
    static const int MAX_PHYS_TRACK = (2 * DOS_NUM_TRACKS) - 1;
    static const int DOS_TRACK_BYTES = 256 * DOS_NUM_SECTORS; // 4096
    static const int RAW_TRACK_BYTES = 0x1A00; // 6656 for .NIB
    
    // Boot ROM address space (PR#6 loads from $C600-$C6FF)
    static const uint16_t ROM_BASE = 0xC600;
    static const uint16_t ROM_SIZE = 0x100;
    
    // GCR encoding table (64 valid values)
    static const uint8_t GCR_ENCODING_TABLE[64];
    
    // DOS 3.3 logical sector ordering
    static const int GCR_LOGICAL_DOS33_SECTOR[16];
    static const int GCR_LOGICAL_PRODOS_SECTOR[16];
    
    // Boot ROM
    static const uint8_t DISK_BOOT_ROM[256];

    DiskII();
    ~DiskII();

    // Load a disk image
    bool loadDisk(int drive, const std::string& filename);
    
    // I/O access
    uint8_t ioRead(uint16_t address);
    void ioWrite(uint16_t address, uint8_t value);
    
    // ROM access (for PR#6 - addresses $C600-$C6FF)
    uint8_t readROM(uint16_t address) const;
    
    // Query disk state
    bool isMotorOn() const { return motorOn; }
    int getCurrentTrack() const { return currPhysTrack; }
    
private:
    // Disk storage
    uint8_t* diskData[NUM_DRIVES];      // Raw nibble data
    int diskTracks[NUM_DRIVES];         // Number of tracks per drive
    bool writeProtected[NUM_DRIVES];
    
    // Drive state
    int currentDrive;
    int phases;                         // 4 phase magnets
    bool motorOn;
    int currPhysTrack;
    int currNibble;
    uint8_t latchData;
    bool writeMode;
    bool loadMode;
    int driveSpin;                      // Anti-stuck counter
    
    // GCR encoding/decoding
    uint8_t gcrBuffer[256];             // 6-bit data
    uint8_t gcrBuffer2[86];             // 2-bit remainder
    uint8_t gcrNibbles[RAW_TRACK_BYTES];
    int gcrNibblesPos;
    
    // Helper functions
    void setPhase(uint16_t address);
    void setDrive(int newDrive);
    void ioLatchC();
    
    // Conversion functions
    void trackToNibbles(const uint8_t* track, uint8_t* nibbles, 
                        int volumeNum, int trackNum, bool dos33);
    void encode44(uint8_t value);
    void encode62(const uint8_t* track, int offset);
    void writeAddressField(int volumeNum, int trackNum, int sectorNum);
    void writeDataField();
    void writeSync(int length);
    void writeNibbles(uint8_t value, int length);
    
    // Bit manipulation
    const static int GCR_SWAP_BIT[4];
};

#endif
