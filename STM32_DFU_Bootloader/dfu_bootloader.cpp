#include "dfu_bootloader.h"
#include "dfu_misc.h"
#include <QIODevice>

/**
 * @brief Class constructor
 */
DFU_Bootloader::DFU_Bootloader()
{
    dfu = new DFU_LL(&isLL_inited);
}

/**
 * @brief Class destructor
 */
DFU_Bootloader::~DFU_Bootloader()
{
    flash_read_data.clear();
    delete dfu;
}

/**
 * @brief Function for getting USB DFU devices list
 * @param none
 * @return USB_DevInfo devices data list
 */
QList<USB_DevInfo*> DFU_Bootloader::GetUsbDevicesList(void)
{
    if(!isLL_inited)
    {
        emit sendError(LL_ISNT_INITED);
    }
    return dfu->GetDevicesList();
}

/**
 * @brief Open DFU device
 * @param dev - USB device data container
 * @return true - device was opened succesfully, false - device wasn't opened
 */
bool DFU_Bootloader::OpenDfuDevice(USB_DevInfo* dev)
{
    bool res = dfu->DFU_OpenDevice(dev);
    if(!res)
    {
        emit sendError(CANT_OPEN_DEVICE);
        delete dfu_attributes;
        dfu_attributes = Q_NULLPTR;
    }
    dfu_attributes = dfu->DFU_GetAttributes();
    return res;
}

/**
 * @brief Close current opened DFU device
 * @param none
 * @return true - device was closed succesfully, false - device wasn't closed
 */
bool DFU_Bootloader::CloseDfuDevice(void)
{
    bool res = dfu->DFU_CloseDevice();
    if(!res)
    {
        emit sendError(CANT_CLOSE_DEVICE);
    }
    return res;
}

/**
 * @brief Get DFU device attributes
 * @param none
 * @return dfu_attrs - DFU device attributes container
 */
DFU_Attributes* DFU_Bootloader::GetDfuAttributes(void)
{
    return dfu_attributes;
}

/**
 * @brief Get DFU bootloader error description
 * @param error_code: error code
 * @return description string of specified error code
 */
QString DFU_Bootloader::GetErrorDescription(ErrorCodes error_code)
{
    QString description;
    switch(error_code)
    {
        case LL_ISNT_INITED:
            description = tr("Low level wasn't inited");
            break;

        case CANT_OPEN_DEVICE:
            description = tr("Can't open DFU device");
            break;

        case CANT_CLOSE_DEVICE:
            description = tr("Can't close DFU device");
            break;

        case CANT_SET_ADDRESS:
            description = tr("Can't set address pointer");
            break;

        case CANT_ERASE_PAGE:
            description = tr("Can't erase flash page");
            break;

        case PROTECTED_MEMORY:
            description = tr("Can't do flash operations - protected memory");
            break;

        case CANT_REMOVE_READ_PROTECTION:
            description = tr("Can't remove read protection");
            break;

        case CANT_LEAVE_DFU_MODE:
            description = tr("Can't leave from DFU mode");
            break;

        case CANT_ERASE_MEMORY:
            description = tr("Can't erase flash memory");
            break;

        case CANT_WRITE_DATA_TO_FLASH:
            description = tr("Can't write data to flash memory");
            break;

        case FIRMWARE_DATA_DOESNT_MATCH_TARGET:
            description = tr("Firmware data doesn't match target memory");
            break;

        case CANT_READ_FLASH_MEMORY:
            description = tr("Can't read data from flash memory");
            break;

        case CANT_RESET_DFU_STATE:
            description = tr("Can't reset DFU state");
            break;
    }
    return description;
}

/**
 * @brief Perform device erase memory operation
 * @param targetIndex: target memory index from device memory map (get from DFU_Attributes)
 * @return none
 */
void DFU_Bootloader::EraseMemory(int targetIndex)
{
    if(dfu_attributes == Q_NULLPTR)
    {
        emit sendError(CANT_ERASE_MEMORY);
        return;
    }
    // send status and operation progress
    emit operationProgressChanged(0);
    emit sendStatus(tr("Erasing memory..."));

    // reset state to DFU_STATE_IDLE
    bool res = dfu->DFU_Abort();
    if(!res)
    {
        emit sendError(CANT_RESET_DFU_STATE);
        return;
    }

    // get flash sectors list
    QList<DFU_FlashSector> sectors = dfu_attributes->memoryMap->at(targetIndex).sectorsMap;
    int eraseableSectorsNum = 0;
    int currentSectorNum = 0;
    // get eraseable sectors number
    for(DFU_FlashSector sector : sectors)
    {
        if(sector.sector_type & DFU_FlashSector::Eraseable)
        {
            eraseableSectorsNum++;
        }
    }
    // emit error if memory haven't eraseable sectors
    if(eraseableSectorsNum == 0)
    {
        emit sendError(CANT_ERASE_MEMORY);
        return;
    }
    // erase flash sectors
    for(DFU_FlashSector sector : sectors)
    {
        if(sector.sector_type & DFU_FlashSector::Eraseable)
        {
            res = erasePage(sector.address);
            if(res)
            {
                currentSectorNum++;
                emit operationProgressChanged(100*currentSectorNum/eraseableSectorsNum);
            }
            else
            {
                return;
            }
        }
    }
    // send status and operation progress
    isTargetMemoryErased = true;
    emit operationProgressChanged(100);
    emit sendStatus(tr("Memory was erased succesfully"));
}

/**
 * @brief Perform write firmware data to device memory operation
 * @param targetIndex: target memory index from device memory map (get from DFU_Attributes)
 * @param fw_data: parsed from hex-file firmware data structure
 * @return none
 */
void DFU_Bootloader::WriteFwData(int targetIndex, QMap<int, QByteArray*>* fw_data)
{
    if(dfu_attributes == Q_NULLPTR)
    {
        emit sendError(CANT_WRITE_DATA_TO_FLASH);
        return;
    }
    // erase flash memory if it hasn't erased yet
    if(!isTargetMemoryErased)
    {
        EraseMemory(targetIndex);
    }
    // reset state to DFU_STATE_IDLE
    bool res = dfu->DFU_Abort();
    if(!res)
    {
        emit sendError(CANT_RESET_DFU_STATE);
        return;
    }
    // send status and operation progress
    emit operationProgressChanged(0);
    emit sendStatus(tr("Program flash memory..."));
    // get target memory map
    DFU_TargetMemoryMap map = dfu_attributes->memoryMap->at(targetIndex);
    QList<DFU_FlashSector> sectors = map.sectorsMap;
    // read firmware data containers
    QList<int> fw_data_block_addresses = fw_data->keys();
    // check of firmware data addresses to target memory map
    int targetStartAddress = 0;
    int targetEndAddress = 0;
    uint32_t firmwareDataLength = 0;

    for(DFU_FlashSector sector : sectors)
    {
        if(targetStartAddress == 0 && (sector.sector_type & DFU_FlashSector::Writable))
        {
            targetStartAddress = sector.address;
        }
        if(sector.sector_type & DFU_FlashSector::Writable)
        {
            targetEndAddress = sector.address + sector.size;
        }
    }

    bool isMatch = true;
    for(int block_addr : fw_data_block_addresses)
    {
        firmwareDataLength += fw_data->value(block_addr)->length();
        if(block_addr < targetStartAddress ||  block_addr >= targetEndAddress)
        {
            isMatch = false;
        }
    }
    if(!isMatch || firmwareDataLength == 0 || fw_data_block_addresses.isEmpty())
    {
        emit sendError(FIRMWARE_DATA_DOESNT_MATCH_TARGET);
        return;
    }
    // write data to target memory
    uint32_t transferSize = dfu_attributes->transferSize;
    uint32_t numOfTranferredBytes = 0;
    for(int block_addr : fw_data_block_addresses)
    {
        res = setAddressPointer(block_addr);
        if(!res) return;
        // write block data to flash
        uint32_t currentAddress = block_addr;
        QByteArray* block_data = fw_data->value(block_addr);
        uint32_t block_size = block_data->length();
        uint16_t wBlockNum;
        uint16_t dataLength;
        uint16_t dataOffset;
        // calc wBlockNum and data length
        while(currentAddress <  block_addr + block_size)
        {
            wBlockNum = (uint16_t)((currentAddress - block_addr)/transferSize);
            dataLength = ((block_addr + block_size - currentAddress) >= transferSize) ? (transferSize) : (block_addr + block_size - currentAddress);
            dataOffset = currentAddress - block_addr;
            res = writeFlashData(wBlockNum, (uint8_t*)block_data->data() + dataOffset, dataLength);
            if(res)
            {
                if(isTargetMemoryErased)
                    isTargetMemoryErased = false;
                currentAddress += dataLength;
                numOfTranferredBytes += dataLength;
                emit operationProgressChanged(100*(numOfTranferredBytes-1)/firmwareDataLength);
            }
            else
            {
                return;
            }
        }
    }
    emit operationProgressChanged(100);
    emit sendStatus(tr("Flash memory was programmed succesfully"));
}

/**
 * @brief Perform read firmware data from device memory operation
 * @param targetIndex: target memory index from device memory map (get from DFU_Attributes)
 * @param fw_data: firmware data structure that may be parsed to hex-file
 * @return none
 */
void DFU_Bootloader::ReadFwData(int targetIndex, QMap<int, QByteArray*>* fw_data)
{
    flash_read_data.clear();
    if(dfu_attributes == Q_NULLPTR)
    {
        emit sendError(CANT_READ_FLASH_MEMORY);
        return;
    }
    // send status and operation progress
    emit operationProgressChanged(0);
    emit sendStatus(tr("Reading flash memory..."));
    // reset state to DFU_STATE_IDLE
    bool res = dfu->DFU_Abort();
    if(!res)
    {
        emit sendError(CANT_RESET_DFU_STATE);
        return;
    }
    // get target memory map
    DFU_TargetMemoryMap map = dfu_attributes->memoryMap->at(targetIndex);
    QList<DFU_FlashSector> sectors = map.sectorsMap;
    // get start and end target memory addresses
    // check of firmware data addresses to target memory map
    uint32_t targetStartAddress = 0;
    uint32_t targetEndAddress = 0;
    uint32_t firmwareDataLength = 0;

    for(DFU_FlashSector sector : sectors)
    {
        if(targetStartAddress == 0 && (sector.sector_type & DFU_FlashSector::Writable) && (sector.sector_type & DFU_FlashSector::Readable))
        {
            targetStartAddress = sector.address;
        }
        if((sector.sector_type & DFU_FlashSector::Writable) && (sector.sector_type & DFU_FlashSector::Readable))
        {
            targetEndAddress = sector.address + sector.size;
        }
    }
    // add block to firmware data map
    uint32_t currentAddress = targetStartAddress;
    uint32_t transferSize = dfu_attributes->transferSize;
    QByteArray temp_buffer;
    uint16_t wBlockNum;
    uint16_t dataLength;

    firmwareDataLength = targetEndAddress - targetStartAddress;
    temp_buffer.resize(transferSize);
    temp_buffer.fill(0xFF, transferSize);
    fw_data->insert(targetStartAddress, &flash_read_data);
    // set start address, where flash will be started to read
    res = setAddressPointer(targetStartAddress);
    if(!res) return;
    // reset state to DFU_STATE_IDLE after setAddressPointer command
    res = dfu->DFU_Abort();
    if(!res)
    {
        emit sendError(CANT_RESET_DFU_STATE);
        return;
    }
    // read data from flash memory
    while(currentAddress < targetEndAddress)
    {
        wBlockNum = (uint16_t)((currentAddress - targetStartAddress)/transferSize);
        dataLength = ((targetEndAddress - currentAddress) >= transferSize) ? (transferSize) : (targetEndAddress - currentAddress);
        res = readFlashData(wBlockNum, (uint8_t*)temp_buffer.data(), dataLength);
        if(res)
        {
            flash_read_data.append(temp_buffer);
            currentAddress += transferSize;
            emit operationProgressChanged(100*(currentAddress - targetStartAddress - 1)/firmwareDataLength);
        }
        else
        {
            return;
        }
    }
    emit sendStatus(tr("Flash memory was read succesfully"));
    emit operationProgressChanged(100);
}

/**
 * @brief Perform leave from DFU mode operation
 * @param none
 * @return none
 */
void DFU_Bootloader::LeaveDfuMode(void)
{
    DFU_Status status;
    // send status and operation progress
    emit operationProgressChanged(0);
    emit sendStatus(tr("Leave from DFU mode..."));

    // reset state to DFU_STATE_IDLE after setAddressPointer command
    bool res = dfu->DFU_Abort();
    if(!res)
    {
        emit sendError(CANT_RESET_DFU_STATE);
        return;
    }

    // send leave DFU mode command
    res = dfu->DFU_Leave();
    emit operationProgressChanged(50);

    if(res)
    {
        res = dfu->DFU_GetStatus(&status);
        if(!res || (status.bState != DFU_STATE_MANIFEST && status.bState != DFU_STATE_MANIFEST_SYNC && status.bState != DFU_STATE_MANIFEST_WAIT_RESET))
        {
            emit sendError(CANT_LEAVE_DFU_MODE);
        }
        else
        {
            emit sendStatus(tr("DFU mode was left succesfully"));
        }
    }
    else
    {
        emit sendError(CANT_LEAVE_DFU_MODE);
        return;
    }

    emit operationProgressChanged(100);
}


/** private functions */

/**
 * @brief Perform set address pointer DFU command
 * @param address: address pointer value
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_Bootloader::setAddressPointer(uint32_t address)
{
    DFU_Status status;
    bool res = dfu->DFU_SetAddressPointer(address);
    if(res)
    {
        res = dfu->DFU_GetStatus(&status);
        if(!res || status.bState != DFU_STATE_DNLOAD_BUSY)
        {
            emit sendError(CANT_SET_ADDRESS);
            res = false;
        }
        else
        {
            res = dfu->DFU_GetStatus(&status);
            if(!res || (status.bState == DFU_STATE_ERROR && status.bStatus == DFU_ERROR_TARGET))
            {
                emit sendError(CANT_SET_ADDRESS);
                res = false;
            }
        }
    }
    else
    {
        emit sendError(CANT_SET_ADDRESS);
    }
    return res;
}

/**
 * @brief Perform erase flash memory page DFU command
 * @param address: flash memory page address
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_Bootloader::erasePage(uint32_t pageAddress)
{
    DFU_Status status;
    bool res = dfu->DFU_Erase(false, pageAddress);
    if(res)
    {
        res = dfu->DFU_GetStatus(&status);
        if(!res || status.bState != DFU_STATE_DNLOAD_BUSY)
        {
            emit sendError(CANT_ERASE_PAGE);
            res = false;
        }
        else
        {
            res = dfu->DFU_GetStatus(&status);
            if(!res || (status.bState == DFU_STATE_ERROR && status.bStatus == DFU_ERROR_TARGET))
            {
                emit sendError(CANT_ERASE_PAGE);
                res = false;
            }
            else if(status.bState == DFU_STATE_ERROR && status.bStatus == DFU_ERROR_VENDOR)
            {
                emit sendError(PROTECTED_MEMORY);
                res = false;
            }
        }
    }
    else
    {
        emit sendError(CANT_ERASE_PAGE);
    }
    return res;
}

/**
 * @brief Perform writing firmware data packet into device flash memory
 * @param wBlockNum: set target memory address pointer by formula ((wBlockNum – 2) × wTransferSize) + Address_Pointer
 * @param data: firmware data packet
 * @param length: firmware data packet length
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_Bootloader::writeFlashData(uint16_t wBlockNum, uint8_t* data, uint16_t length)
{
    DFU_Status status;
    bool res = dfu->DFU_Dnload(wBlockNum+2, data, length);
    if(res)
    {
        res = dfu->DFU_GetStatus(&status);
        if(!res || status.bState != DFU_STATE_DNLOAD_BUSY)
        {
            emit sendError(CANT_WRITE_DATA_TO_FLASH);
            res = false;
        }
        else
        {
            res = dfu->DFU_GetStatus(&status);
            if(!res || (status.bState == DFU_STATE_ERROR && status.bStatus == DFU_ERROR_TARGET))
            {
                emit sendError(CANT_WRITE_DATA_TO_FLASH);
                res = false;
            }
            else if(status.bState == DFU_STATE_ERROR && status.bStatus == DFU_ERROR_VENDOR)
            {
                emit sendError(PROTECTED_MEMORY);
                res = false;
            }
        }
    }
    else
    {
        emit sendError(CANT_WRITE_DATA_TO_FLASH);
    }
    return res;
}

/**
 * @brief Perform reading firmware data packet from device flash memory
 * @param wBlockNum: set target memory address pointer by formula ((wBlockNum – 2) × wTransferSize) + Address_Pointer
 * @param data: firmware data packet
 * @param length: firmware data packet length
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_Bootloader::readFlashData(uint16_t wBlockNum, uint8_t* data, uint16_t length)
{
    DFU_Status status;
    bool res = dfu->DFU_Upload(wBlockNum+2, data, length);
    if(res)
    {
        res = dfu->DFU_GetStatus(&status);
        if(!res)
        {
            emit sendError(CANT_READ_FLASH_MEMORY);
        }
        else if(status.bState == DFU_STATE_ERROR && status.bStatus == DFU_ERROR_VENDOR)
        {
            emit sendError(PROTECTED_MEMORY);
            res = false;
        }
    }
    else
    {
        emit sendError(CANT_READ_FLASH_MEMORY);
    }
    return res;
}

/**
 * @brief Perform remove read protection of device flash memory if supported
 * @param none
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_Bootloader::readUnprotect(void)
{
   DFU_Status status;
   bool res = dfu->DFU_ReadUnprotect();
   if(res)
   {
       res = dfu->DFU_GetStatus(&status);
       if(!res || status.bState != DFU_STATE_DNLOAD_BUSY)
       {
           emit sendError(CANT_REMOVE_READ_PROTECTION);
           res = false;
       }
       else
       {
           res = dfu->DFU_GetStatus(&status);
           if(!res || status.bState == DFU_STATE_ERROR)
           {
               emit sendError(CANT_REMOVE_READ_PROTECTION);
               res = false;
           }
       }
   }
   else
   {
       emit sendError(CANT_REMOVE_READ_PROTECTION);
   }
   return res;
}
