#include "disk.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

// GCR encoding: 4-bit value -> 8-bit GCR
const uint8_t DiskII::GCR_ENCODING_TABLE[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

// Boot ROM for PR#6 - Disk II controller (loaded at $C600)
const uint8_t DiskII::DISK_BOOT_ROM[256] = {
    0xA2,0x20,0xA0,0x00,0xA2,0x03,0x86,0x3C,0x8A,0x0A,0x24,0x3C,0xF0,0x10,0x05,0x3C,
    0x49,0xFF,0x29,0x7E,0xB0,0x08,0x4A,0xD0,0xFB,0x98,0x9D,0x56,0x03,0xC8,0xE8,0x10,
    0xE5,0x20,0x58,0xFF,0xBA,0xBD,0x00,0x01,0x0A,0x0A,0x0A,0x0A,0x85,0x2B,0xAA,0xBD,
    0x8E,0xC0,0xBD,0x8C,0xC0,0xBD,0x8A,0xC0,0xBD,0x89,0xC0,0xA0,0x50,0xBD,0x80,0xC0,
    0x98,0x29,0x03,0x0A,0x05,0x2B,0xAA,0xBD,0x81,0xC0,0xA9,0x56,0xA9,0x00,0xEA,0x88,
    0x10,0xEB,0x85,0x26,0x85,0x3D,0x85,0x41,0xA9,0x08,0x85,0x27,0x18,0x08,0xBD,0x8C,
    0xC0,0x10,0xFB,0x49,0xD5,0xD0,0xF7,0xBD,0x8C,0xC0,0x10,0xFB,0xC9,0xAA,0xD0,0xF3,
    0xEA,0xBD,0x8C,0xC0,0x10,0xFB,0xC9,0x96,0xF0,0x09,0x28,0x90,0xDF,0x49,0xAD,0xF0,
    0x25,0xD0,0xD9,0xA0,0x03,0x85,0x40,0xBD,0x8C,0xC0,0x10,0xFB,0x2A,0x85,0x3C,0xBD,
    0x8C,0xC0,0x10,0xFB,0x25,0x3C,0x88,0xD0,0xEC,0x28,0xC5,0x3D,0xD0,0xBE,0xA5,0x40,
    0xC5,0x41,0xD0,0xB8,0xB0,0xB7,0xA0,0x56,0x84,0x3C,0xBC,0x8C,0xC0,0x10,0xFB,0x59,
    0xD6,0x02,0xA4,0x3C,0x88,0x99,0x00,0x03,0xD0,0xEE,0x84,0x3C,0xBC,0x8C,0xC0,0x10,
    0xFB,0x59,0xD6,0x02,0xA4,0x3C,0x91,0x26,0xC8,0xD0,0xEF,0xBC,0x8C,0xC0,0x10,0xFB,
    0x59,0xD6,0x02,0xD0,0x87,0xA0,0x00,0xA2,0x56,0xCA,0x30,0xFB,0xB1,0x26,0x5E,0x00,
    0x03,0x2A,0x5E,0x00,0x03,0x2A,0x91,0x26,0xC8,0xD0,0xEE,0xE6,0x27,0xE6,0x3D,0xA5,
    0x3D,0xCD,0x00,0x08,0xA6,0x2B,0x90,0xDB,0x4C,0x01,0x08,0x00,0x00,0x00,0x00,0x00,
};

// DOS 3.3 physical -> logical sector mapping
const int DiskII::GCR_LOGICAL_DOS33_SECTOR[16] = {
    0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4,
    0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF
};

// ProDOS physical -> logical sector mapping
const int DiskII::GCR_LOGICAL_PRODOS_SECTOR[16] = {
    0x0, 0x8, 0x1, 0x9, 0x2, 0xA, 0x3, 0xB,
    0x4, 0xC, 0x5, 0xD, 0x6, 0xE, 0x7, 0xF
};

const int DiskII::GCR_SWAP_BIT[4] = {0, 2, 1, 3};

DiskII::DiskII() 
    : currentDrive(0), phases(0), motorOn(false), currPhysTrack(0), 
      currNibble(0), latchData(0), writeMode(false), loadMode(false), driveSpin(0) {
    
    for (int i = 0; i < NUM_DRIVES; i++) {
        diskData[i] = nullptr;
        diskTracks[i] = 0;
        writeProtected[i] = true;
    }
}

DiskII::~DiskII() {
    for (int i = 0; i < NUM_DRIVES; i++) {
        if (diskData[i]) {
            delete[] diskData[i];
            diskData[i] = nullptr;
        }
    }
}

bool DiskII::loadDisk(int drive, const std::string& filename) {
    if (drive < 0 || drive >= NUM_DRIVES) {
        return false;
    }
    
    // Free existing disk
    if (diskData[drive]) {
        delete[] diskData[drive];
        diskData[drive] = nullptr;
    }
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        printf("Failed to open disk image: %s\n", filename.c_str());
        return false;
    }
    
    // Determine if DOS or ProDOS by filename
    bool isDos33 = (filename.find(".dsk") != std::string::npos);
    
    // Read file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Allocate storage for raw nibbles
    // Each track is RAW_TRACK_BYTES, 35 tracks standard
    int numTracks = DOS_NUM_TRACKS;
    diskData[drive] = new uint8_t[numTracks * RAW_TRACK_BYTES];
    diskTracks[drive] = numTracks;
    
    uint8_t track[DOS_TRACK_BYTES];
    
    // Read tracks and convert to nibbles
    for (int trackNum = 0; trackNum < numTracks; trackNum++) {
        if (file.read((char*)track, DOS_TRACK_BYTES).fail()) {
            printf("Failed reading track %d\n", trackNum);
            delete[] diskData[drive];
            diskData[drive] = nullptr;
            return false;
        }
        
        // Convert sector format to nibbles
        uint8_t* nibblePtr = diskData[drive] + (trackNum * RAW_TRACK_BYTES);
        trackToNibbles(track, nibblePtr, 254, trackNum, isDos33);
    }
    
    file.close();
    writeProtected[drive] = true;  // For now, always write-protected
    
    printf("Loaded disk drive %d: %d tracks\n", drive, numTracks);
    return true;
}

uint8_t DiskII::ioRead(uint16_t address) {
    address &= 0x0F;
    printf("IO Read Addrss is %x\n", address);
    switch (address) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
            setPhase(address);
            break;
            
        case 0x8:
            motorOn = false;
            break;
            
        case 0x9:
            motorOn = true;
            break;
            
        case 0xa:
            setDrive(0);
            break;
            
        case 0xb:
            setDrive(1);
            break;
            
        case 0xc:
            ioLatchC();
            break;
            
        case 0xd:
            loadMode = true;
            if (motorOn && !writeMode && writeProtected[currentDrive]) {
                latchData |= 0x80;  // Set write-protect bit
            }
            break;
            
        case 0xe:
            writeMode = false;
            break;
            
        case 0xf:
            writeMode = true;
            break;
    }
    
    // Only even addresses return the latch
    printf("Returned %x\n", ((address & 1) == 0) ? latchData : (rand() & 0xFF));
    return ((address & 1) == 0) ? latchData : (rand() & 0xFF);
}

void DiskII::ioWrite(uint16_t address, uint8_t value) {
    address &= 0x0F;
    
    switch (address) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
            setPhase(address);
            break;
            
        case 0x8:
            motorOn = false;
            break;
            
        case 0x9:
            motorOn = true;
            break;
            
        case 0xa:
            setDrive(0);
            break;
            
        case 0xb:
            setDrive(1);
            break;
            
        case 0xc:
            ioLatchC();
            break;
            
        case 0xd:
            loadMode = true;
            break;
            
        case 0xe:
            writeMode = false;
            break;
            
        case 0xf:
            writeMode = true;
            break;
    }
    
    // Write latch for disk data
    if (motorOn && writeMode && loadMode) {
        latchData = value;
    }
}

uint8_t DiskII::readROM(uint16_t address) const {
    // PR#6 loads the Disk II controller ROM from $C600-$C6FF
    // Address should be in the range $C600-$C6FF
    if (address >= ROM_BASE && address < ROM_BASE + ROM_SIZE) {
        return DISK_BOOT_ROM[address - ROM_BASE];
    }
    return 0x00;  // Default return if out of range
}

void DiskII::setPhase(uint16_t address) {
    int phase = (address >> 1) & 3;
    int phaseBit = (1 << phase);
    
    if ((address & 1) != 0) {
        // Phase on
        phases |= phaseBit;
    } else {
        // Phase off
        phases &= ~phaseBit;
    }
    
    // Check for stepping
    int direction = 0;
    if ((phases & (1 << ((currPhysTrack + 1) & 3))) != 0)
        direction += 1;
    if ((phases & (1 << ((currPhysTrack + 3) & 3))) != 0)
        direction -= 1;
    
    // Apply step
    if (direction != 0) {
        currPhysTrack += direction;
        if (currPhysTrack < 0)
            currPhysTrack = 0;
        else if (currPhysTrack > MAX_PHYS_TRACK)
            currPhysTrack = MAX_PHYS_TRACK;
        currNibble = 0;  // Reset position on track change
    }
}

void DiskII::setDrive(int newDrive) {
    if (newDrive >= 0 && newDrive < NUM_DRIVES) {
        currentDrive = newDrive;
    }
}

void DiskII::ioLatchC() {
    loadMode = false;
    
    if (!writeMode) {
        // Read mode
        if (!motorOn) {
            // Hack: fool RWTS drive spin check (usually at $BD34)
            driveSpin = (driveSpin + 1) & 0xF;
            if (driveSpin == 0) {
                latchData = 0x7F;
            }
            // else: latchData keeps its previous value
        } else if (diskData[currentDrive]) {
            // Read data from disk
            int trackNum = currPhysTrack >> 1;
            if (trackNum >= diskTracks[currentDrive]) {
                latchData = 0x7F;
            } else {
                uint8_t* track = diskData[currentDrive] + (trackNum * RAW_TRACK_BYTES);
                latchData = track[currNibble];
                
                // Skip invalid nibbles (0x7F padding)
                if (latchData == 0x7F) {
                    int count = RAW_TRACK_BYTES / 16;
                    do {
                        currNibble++;
                        if (currNibble >= RAW_TRACK_BYTES)
                            currNibble = 0;
                        latchData = track[currNibble];
                    } while (latchData == 0x7F && --count > 0);
                }
            }
        } else {
            latchData = 0x7F;
        }
    } else {
        // Write mode: store data to disk
        int trackNum = currPhysTrack >> 1;
        if (trackNum < diskTracks[currentDrive] && diskData[currentDrive]) {
            uint8_t* track = diskData[currentDrive] + (trackNum * RAW_TRACK_BYTES);
            track[currNibble] = latchData;
        }
    }
    
    // Always increment position
    currNibble++;
    if (currNibble >= RAW_TRACK_BYTES)
        currNibble = 0;
}

void DiskII::writeNibbles(uint8_t value, int length) {
    while (length > 0 && gcrNibblesPos < RAW_TRACK_BYTES) {
        gcrNibbles[gcrNibblesPos++] = value;
        length--;
    }
}

void DiskII::writeSync(int length) {
    writeNibbles(0xff, length);
}

void DiskII::encode44(uint8_t value) {
    if (gcrNibblesPos + 1 < RAW_TRACK_BYTES) {
        gcrNibbles[gcrNibblesPos++] = ((value >> 1) | 0xaa);
        gcrNibbles[gcrNibblesPos++] = (value | 0xaa);
    }
}

void DiskII::encode62(const uint8_t* track, int offset) {
    // 86 * 3 = 258 bytes
    gcrBuffer2[0] = GCR_SWAP_BIT[track[offset + 1] & 0x03];
    gcrBuffer2[1] = GCR_SWAP_BIT[track[offset] & 0x03];
    
    // Extract 6-bit values and 2-bit remainders
    for (int i = 255, j = 2; i >= 0; i--, j = (j == 85) ? 0 : j + 1) {
        gcrBuffer2[j] = ((gcrBuffer2[j] << 2) | GCR_SWAP_BIT[track[offset + i] & 0x03]);
        gcrBuffer[i] = (track[offset + i] >> 2);
    }
    
    // Mask to 6 bits
    for (int i = 0; i < 86; i++) {
        gcrBuffer2[i] &= 0x3f;
    }
}

void DiskII::writeAddressField(int volumeNum, int trackNum, int sectorNum) {
    if (gcrNibblesPos + 14 >= RAW_TRACK_BYTES) return;
    
    // Address field prologue
    gcrNibbles[gcrNibblesPos++] = 0xd5;
    gcrNibbles[gcrNibblesPos++] = 0xaa;
    gcrNibbles[gcrNibblesPos++] = 0x96;
    
    // Write volume, track, sector, checksum with 4-4 encoding
    encode44(volumeNum);
    encode44(trackNum);
    encode44(sectorNum);
    encode44(volumeNum ^ trackNum ^ sectorNum);
    
    // Epilogue
    gcrNibbles[gcrNibblesPos++] = 0xde;
    gcrNibbles[gcrNibblesPos++] = 0xaa;
    gcrNibbles[gcrNibblesPos++] = 0xeb;
}

void DiskII::writeDataField() {
    if (gcrNibblesPos + 350 >= RAW_TRACK_BYTES) return;
    
    uint8_t last = 0;
    uint8_t checksum;
    
    // Data field prologue
    gcrNibbles[gcrNibblesPos++] = 0xd5;
    gcrNibbles[gcrNibblesPos++] = 0xaa;
    gcrNibbles[gcrNibblesPos++] = 0xad;
    
    // Write 6-2 encoded data
    for (int i = 0x55; i >= 0; i--) {
        checksum = last ^ gcrBuffer2[i];
        gcrNibbles[gcrNibblesPos++] = GCR_ENCODING_TABLE[checksum];
        last = gcrBuffer2[i];
    }
    
    for (int i = 0; i < 256; i++) {
        checksum = last ^ gcrBuffer[i];
        gcrNibbles[gcrNibblesPos++] = GCR_ENCODING_TABLE[checksum];
        last = gcrBuffer[i];
    }
    
    // Write checksum
    gcrNibbles[gcrNibblesPos++] = GCR_ENCODING_TABLE[last];
    
    // Epilogue
    gcrNibbles[gcrNibblesPos++] = 0xde;
    gcrNibbles[gcrNibblesPos++] = 0xaa;
    gcrNibbles[gcrNibblesPos++] = 0xeb;
}

void DiskII::trackToNibbles(const uint8_t* track, uint8_t* nibbles, 
                            int volumeNum, int trackNum, bool dos33) {
    // Use the provided nibbles buffer directly
    memset(nibbles, 0, RAW_TRACK_BYTES);
    gcrNibblesPos = 0;
    
    const int* logicalSector = dos33 ? GCR_LOGICAL_DOS33_SECTOR : GCR_LOGICAL_PRODOS_SECTOR;
    
    // Process all 16 sectors
    for (int sectorNum = 0; sectorNum < DOS_NUM_SECTORS; sectorNum++) {
        // Encode sector data (6-2 encoding)
        encode62(track, logicalSector[sectorNum] << 8);
        
        // Write sync bytes
        writeSync(12);
        
        // Write address field
        writeAddressField(volumeNum, trackNum, sectorNum);
        
        // Sync between address and data
        writeSync(8);
        
        // Write data field
        writeDataField();
    }
    
    // Pad remaining space with invalid nibbles
    while (gcrNibblesPos < RAW_TRACK_BYTES) {
        nibbles[gcrNibblesPos++] = 0x7F;
    }
    
    // Copy to persistent member for write operations
    memcpy(gcrNibbles, nibbles, RAW_TRACK_BYTES);
}
