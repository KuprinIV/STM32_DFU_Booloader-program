#ifndef DFU_MISC_H
#define DFU_MISC_H

#include <QObject>
#include "STM32_DFU_Bootloader_Lib_global.h"

class DFU_Status
{
public:
    DFU_Status(){};

    uint8_t bStatus;
    uint32_t bwPollTimeout;
    uint8_t bState;
    uint8_t iString;
};

class DFU_FlashSector
{
public:
    enum SectorType : uint8_t
    {
        Readable = 0x01,
        Eraseable = 0x02,
        Writable = 0x04,
    };

    DFU_FlashSector() {}
    uint32_t address;
    uint32_t size;
    int sector_type;
};

class STM32_DFU_BOOTLOADER_LIB_EXPORT DFU_TargetMemoryMap
{
public:
    DFU_TargetMemoryMap(){}
    QString memoryName;
    uint32_t startAddress;
    QList<DFU_FlashSector> sectorsMap;
};

class STM32_DFU_BOOTLOADER_LIB_EXPORT DFU_Attributes
{
public:
    DFU_Attributes(){};

    bool isCanDnload;
    bool isCanUpload;
    bool isManifestationTolerant;
    bool willDetach;
    bool isAcceleratedST;
    uint16_t detachTimeout;
    uint16_t transferSize;
    uint16_t bcdDfuVersion;
    QList<DFU_TargetMemoryMap>* memoryMap;
};

#endif // DFU_MISC_H
