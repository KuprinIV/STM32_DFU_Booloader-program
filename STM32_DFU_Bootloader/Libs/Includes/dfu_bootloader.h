#ifndef DFU_BOOTLOADER_H
#define DFU_BOOTLOADER_H

#include <QObject>
#include <QMap>
#include <QtCore/qglobal.h>
#include "dfu_ll.h"
#include "dfu_misc.h"

#if defined(STM32_DFU_BOOTLOADER_LIB_LIBRARY)
#  define STM32_DFU_BOOTLOADER_LIB_EXPORT Q_DECL_EXPORT
#else
#  define STM32_DFU_BOOTLOADER_LIB_EXPORT Q_DECL_IMPORT
#endif

class STM32_DFU_BOOTLOADER_LIB_EXPORT DFU_Bootloader : public QObject
{
    Q_OBJECT
public:
    DFU_Bootloader();
    ~DFU_Bootloader();

    enum ErrorCodes : int
    {
        LL_ISNT_INITED = -1,
        CANT_OPEN_DEVICE = -2,
        CANT_CLOSE_DEVICE = -3,
        CANT_SET_ADDRESS = -4,
        CANT_ERASE_PAGE = -5,
        PROTECTED_MEMORY = -6,
        CANT_REMOVE_READ_PROTECTION = -7,
        CANT_LEAVE_DFU_MODE = -8,
        CANT_ERASE_MEMORY = -9,
        CANT_WRITE_DATA_TO_FLASH = -10,
        FIRMWARE_DATA_DOESNT_MATCH_TARGET = -11,
        CANT_READ_FLASH_MEMORY = -12,
        CANT_RESET_DFU_STATE = -13,
        CANT_SEND_CHECKSUM = -14,
        CANT_SAVE_FW_VERIFY_DATA = -15,
        CANT_LOCK_FLASH_READ = -16,
    };

    enum DFU_BootloaderOperations : int
    {
        NO_OPERATION,
        ERASE_MEMORY,
        WRITE_FW_DATA,
        READ_FW_DATA,
        LEAVE_DFU_MODE,
        LOCK_FLASH_MEMORY_READING
    };

    QList<USB_DevInfo*> GetUsbDevicesList(void);
    bool OpenDfuDevice(USB_DevInfo* dev);
    bool CloseDfuDevice(void);
    QString GetErrorDescription(ErrorCodes error_code);
    DFU_Attributes* GetDfuAttributes(void);
    // bootloader functions
    void EraseMemory(int targetIndex);
    void WriteFwData(int targetIndex, QMap<int, QByteArray*>* fw_data);
    void ReadFwData(int targetIndex, QMap<int, QByteArray*>* fw_data);
    void LeaveDfuMode(void);
    /** extended bootloader functions */
    void WriteEncryptedFwData(int targetIndex, QMap<int, QByteArray*>* fw_data, uint32_t checksum);
    void LockFlashMemoryReading(void);

signals:
    void sendError(ErrorCodes error_code);
    void operationProgressChanged(int progress);
    void sendStatus(QString op_status);

private:
    // members
    DFU_LL* dfu;
    DFU_Attributes* dfu_attributes = Q_NULLPTR;
    QByteArray flash_read_data;
    bool isLL_inited = false;
    bool isTargetMemoryErased = false;

    // functions
    bool setAddressPointer(uint32_t address);
    bool erasePage(uint32_t pageAddress);
    bool writeFlashData(uint16_t wBlockNum, uint8_t* data, uint16_t length);
    bool readFlashData(uint16_t wBlockNum, uint8_t* data, uint16_t length);
    bool readUnprotect(void);
};

#endif // DFU_BOOTLOADER_H
