#include "dfu_ll.h"

/**
 * @brief Class constructor
 * @param is_LL_inited returns libusb initialization result
 */
DFU_LL::DFU_LL(bool* is_LL_inited)
{
    if(libusb_init(NULL) >= 0)
    {
       isLibusbInited = true;
    }
    *is_LL_inited = isLibusbInited;
}

/**
 * @brief Class destructor
 */
DFU_LL::~DFU_LL()
{
    this->disconnect();
    libusb_exit(NULL);
    if(dfu_attrs != Q_NULLPTR)
    {
        delete dfu_attrs;
    }
}

/**
 * @brief Function for getting USB DFU devices list
 * @param none
 * @return USB_DevInfo devices data list
 */
QList<USB_DevInfo*> DFU_LL::GetDevicesList(void)
{
    libusb_device **devs;
    libusb_device *cur_dev;
    int dev_count = 0;
    QList<USB_DevInfo*> devs_list;

    if(isLibusbInited)
    {
        dev_count = (int)libusb_get_device_list(NULL,&devs);
        if(dev_count > 0)
        {
            int i = 0;
            while ((cur_dev = devs[i++]) != NULL)
            {
                struct libusb_device_descriptor desc;
                int res = libusb_get_device_descriptor(cur_dev, &desc);
                if(res < 0) continue;
                else
                {
                    // try to get DFU functional descriptor. Add in list only DFU devices
                    struct libusb_config_descriptor* conf_desc;
                    res = libusb_get_active_config_descriptor(cur_dev, &conf_desc);
                    if(res != LIBUSB_SUCCESS)
                    {
                        continue;
                    }
                    // check interface descriptor
                    uint8_t bInterfaceClass = conf_desc->interface->altsetting[0].bInterfaceClass;
                    uint8_t bInterfaceSubClass = conf_desc->interface->altsetting[0].bInterfaceSubClass;
                    uint8_t bInterfaceProtocol = conf_desc->interface->altsetting[0].bInterfaceProtocol;
                    // if device interface is DFU add it in devices list
                    if(bInterfaceClass == 0xFE && bInterfaceSubClass == 0x01 && bInterfaceProtocol == 0x02)
                    {
                        devs_list.append(new USB_DevInfo(desc.idVendor, desc.idProduct));
                    }
                    libusb_free_config_descriptor(conf_desc); // release configuration descriptor resources
                }
            }
        }
        libusb_free_device_list(devs, 1);
    }
    return devs_list;
}

/**
 * @brief Open DFU device
 * @param dev - USB device data container
 * @return true - device was opened succesfully, false - device wasn't opened
 */
bool DFU_LL::DFU_OpenDevice(USB_DevInfo* dev)
{
    int res;

    if(isLibusbInited)
    {
        dfu_device = libusb_open_device_with_vid_pid(NULL, dev->VID, dev->PID);
        if(dfu_device == NULL)
        {
            return false;
        }
        res = libusb_claim_interface(dfu_device, 0);
        if(res != LIBUSB_SUCCESS)
        {
            libusb_close(dfu_device);
            return false;
        }
        // read DFU configuration and interface descriptors
        struct libusb_config_descriptor* conf_desc;
        libusb_device* dev = libusb_get_device(dfu_device);
        res = libusb_get_active_config_descriptor(dev, &conf_desc);
        if(res != LIBUSB_SUCCESS)
        {
            libusb_close(dfu_device);
            return false;
        }
        // try to get DFU functional descriptor
        dfu_attrs = new DFU_Attributes();

        if(conf_desc->interface->altsetting[0].extra_length == USB_DFU_DESC_SIZ && conf_desc->interface->altsetting[0].extra[1] == DFU_DESCRIPTOR_TYPE)
        {
            // fill DFU attributes
            dfu_attrs->isCanDnload = (conf_desc->interface->altsetting[0].extra[2] & ATTR_DNLOAD_CAPABLE) != 0;
            dfu_attrs->isCanUpload = (conf_desc->interface->altsetting[0].extra[2] & ATTR_UPLOAD_CAPABLE) != 0;
            dfu_attrs->isManifestationTolerant = (conf_desc->interface->altsetting[0].extra[2] & ATTR_MANIFESTATION_TOLERANT) != 0;
            dfu_attrs->willDetach = (conf_desc->interface->altsetting[0].extra[2] & ATTR_WILL_DETACH) != 0;
            dfu_attrs->isAcceleratedST = (conf_desc->interface->altsetting[0].extra[2] & ATTR_ST_CAN_ACCELERATE) != 0;
            dfu_attrs->detachTimeout = conf_desc->interface->altsetting[0].extra[3];
            dfu_attrs->transferSize = (uint16_t)((conf_desc->interface->altsetting[0].extra[6]<<8) | conf_desc->interface->altsetting[0].extra[5]);
            dfu_attrs->bcdDfuVersion = (uint16_t)((conf_desc->interface->altsetting[0].extra[8]<<8) | conf_desc->interface->altsetting[0].extra[7]);
        }
        else
        {
            // if we can't to read functional descriptor, fill DFU attributes by default values
            dfu_attrs->isCanDnload = true;
            dfu_attrs->isCanUpload = true;
            dfu_attrs->isManifestationTolerant = false;
            dfu_attrs->willDetach = true;
            dfu_attrs->isAcceleratedST = false;
            dfu_attrs->detachTimeout = 0xFF;
            dfu_attrs->transferSize = 1024;
            dfu_attrs->bcdDfuVersion = 0x011A;
        }
        libusb_free_config_descriptor(conf_desc); // release configuration descriptor resources

        // read memory mapping string descriptor
        uint8_t usrStringDesc[255] = {0};
        res = libusb_get_string_descriptor_ascii(dfu_device, USBD_IDX_INTERFACE_STR+1, usrStringDesc, 255);
        if(res < 0)
        {
            libusb_close(dfu_device);
            return false;
        }
        QString memoryMapStr;
        memoryMapStr.append((const char*)usrStringDesc);

        memory_map = getDeviceMemoryMap(memoryMapStr);
        dfu_attrs->memoryMap = &memory_map;
    }
    else
    {
        return false;
    }
    return true;
}

/**
 * @brief Close current opened DFU device
 * @param none
 * @return true - device was closed succesfully, false - device wasn't closed
 */
bool DFU_LL::DFU_CloseDevice(void)
{
    if(isLibusbInited)
    {
        libusb_release_interface(dfu_device, 0);
        libusb_close(dfu_device);
        dfu_device = Q_NULLPTR;
    }
    else
    {
        return false;
    }
    return true;
}

/**
 * @brief Get DFU device attributes
 * @param none
 * @return dfu_attrs - DFU device attributes container
 */
DFU_Attributes* DFU_LL::DFU_GetAttributes(void)
{
    return dfu_attrs;
}

/**
 * @brief DFU detach command: requests the device to leave DFU mode and enter the application
 * @param timeout - timeout for waiting USB reset
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_Detach(uint16_t timeout)
{
    return controlTransfer(DFU_BMREQUEST|LIBUSB_ENDPOINT_OUT, DFU_DETACH, timeout, 0, NULL, 0);
}

/**
 * @brief DFU download request: requests data transfer from Host to the device in order to load them
into device internal Flash memory. Includes also erase commands
 * @param wBlockNum: if wBlockNum > 1 - write data to memory; if wBlockNum = 0 - perform download command (selected by 1st data byte)
 * @param data: if wBlockNum > 1 - firmware data; if wBlockNum = 0 - 1st byte command selection, another bytes - command data
 * @param length: data array length
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_Dnload(uint16_t wBlockNum, uint8_t* data, uint16_t length)
{
   return controlTransfer(DFU_BMREQUEST|LIBUSB_ENDPOINT_OUT, DFU_DNLOAD, wBlockNum, 0, data, length);
}

/**
 * @brief DFU upload request: requests data transfer from device to Host in order to load content
of device internal Flash memory into a Host file
 * @param wBlockNum: if wBlockNum > 1 - read data from memory; if wBlockNum = 0 - perform upload command (selected by 1st data byte)
 * @param data: if wBlockNum > 1 - firmware data; if wBlockNum = 0 - 1st byte command selection, another bytes - command data
 * @param length: data array length
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_Upload(uint16_t wBlockNum, uint8_t* data, uint16_t length)
{
    return controlTransfer(DFU_BMREQUEST|LIBUSB_ENDPOINT_IN, DFU_UPLOAD, wBlockNum, 0, data, length);
}

/**
 * @brief Requests device to send status report to the Host (including status
resulting from the last request execution and the state the device enters immediately after this request)
 * @param status: 6 bytes of status data
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_GetStatus(DFU_Status* status)
{
    uint8_t status_data[6] = {DFU_ERROR_UNKNOWN, 0, 0, 0, DFU_STATE_ERROR, 0};
    bool res = controlTransfer(DFU_BMREQUEST|LIBUSB_ENDPOINT_IN, GET_STATUS, 0, 0, status_data, 6);
    if(res)
    {
        status->bStatus = status_data[0];
        status->bwPollTimeout = (uint32_t)((status_data[3]<<16)|(status_data[2]<<8)|status_data[1]);
        status->bState = status_data[4];
        status->iString = status_data[5];
    }
    return res;
}

/**
 * @brief Requests device to clear error status and move to next step
 * @param none
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_ClearStatus(void)
{
    return controlTransfer(DFU_BMREQUEST|LIBUSB_ENDPOINT_OUT, DFU_CLRSTATUS, 0, 0, NULL, 0);
}

/**
 * @brief Requests the device to send only the state it enters immediately after this request.
 * @param state: received state from device
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_GetState(uint8_t* state)
{
   return controlTransfer(DFU_BMREQUEST|LIBUSB_ENDPOINT_IN, DFU_GETSTATE, 0, 0, state, 1);
}

/**
 * @brief Requests device to exit the current state/operation and enter idle state immediately.
 * @param none
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_Abort(void)
{
    return controlTransfer(DFU_BMREQUEST|LIBUSB_ENDPOINT_OUT, DFU_ABORT, 0, 0, NULL, 0);
}

/**
 * @brief Upload request command: the host requests to read the commands supported by the bootloader. After receiving this
command, the device returns N bytes representing the command codes.
 * @param commands_list: N bytes representing the command codes
 * @param length: commands_list array length
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_GetCommands(uint8_t* commands_list, uint16_t length)
{
    return DFU_Upload(Get_Command, commands_list, length > 4 ? 4 : length);
}

/**
 * @brief Download request command: the host sends a DFU_DNLOAD request to set the
address pointer value used for computing the start address for Read and Write memory operations.
 * @param address: address pointer value
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_SetAddressPointer(uint32_t address)
{
    uint8_t cmd_data[5];

    cmd_data[0] = Set_Address_PointerFB;
    cmd_data[1] = (uint8_t)(address & 0x000000FF);
    cmd_data[2] = (uint8_t)((address>>8) & 0x000000FF);
    cmd_data[3] = (uint8_t)((address>>16) & 0x000000FF);
    cmd_data[4] = (uint8_t)((address>>24) & 0x000000FF);

    return DFU_Dnload(0, cmd_data, 5);
}

/**
 * @brief Download request command: the host sends a DFU_DNLOAD request to erase one page of
the internal Flash memory or to mass erase it.
 * @param isMassErase: true - erase full chip memory, false - erase only one page at mentioned address
 * @param pageAddress: page address needed to erase
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_Erase(bool isMassErase, uint32_t pageAddress)
{
    uint8_t cmd_data[5];

    cmd_data[0] = EraseFB;
    cmd_data[1] = (uint8_t)(pageAddress & 0x000000FF);
    cmd_data[2] = (uint8_t)((pageAddress>>8) & 0x000000FF);
    cmd_data[3] = (uint8_t)((pageAddress>>16) & 0x000000FF);
    cmd_data[4] = (uint8_t)((pageAddress>>24) & 0x000000FF);

    if(isMassErase)
    {
        return DFU_Dnload(0, cmd_data, 1);
    }
    return DFU_Dnload(0, cmd_data, 5);
}

/**
 * @brief Download request command: the host sends a DFU_DNLOAD request to remove the read
protection of the internal Flash memory.
 * @param none
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_ReadUnprotect(void)
{
    uint8_t cmd_data = Read_UnprotectFB;
    return DFU_Dnload(0, &cmd_data, 1);
}

/**
 * @brief Download request command: the host sends a DFU_DNLOAD request to inform the device that it has to exit DFU mode
 * @param none
 * @return true - command was sent succesfully, false - command wasn't send or error was occured during transfer
 */
bool DFU_LL::DFU_Leave(void)
{
    return DFU_Dnload(0, NULL, 0);
}

bool DFU_LL::DFU_SendTargetParams(uint32_t start_addr, uint32_t size)
{
    // form transfer data array
    uint8_t data[8] = {0};
    memcpy(data, &start_addr, 4);
    memcpy(data+4, &size, 4);
    // send request
    return controlTransfer(LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_ENDPOINT_OUT, DFU_SEND_TARGET_PARAMS, 0, 0, data, sizeof(data));
}

bool DFU_LL::DFU_Verify(uint8_t* is_verified)
{
   return controlTransfer(LIBUSB_REQUEST_TYPE_VENDOR|LIBUSB_ENDPOINT_IN, DFU_VERIFY, 0, 0, is_verified, 1);
}

/** private functions */

/**
 * @brief Realize synchronous control transfer via libusb
 * @return true - transfer was  succesfully, false - error was occured during transfer
 */
bool DFU_LL::controlTransfer(uint8_t requestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char* data, uint16_t wLength)
{
    int res = libusb_control_transfer(dfu_device, requestType, bRequest, wValue, wIndex, data, wLength, 1000);
    if(res < 0)
    {
        return false;
    }
    return true;
}

/**
 * @brief Parse device flash memory string descriptor and get device memory map
 * @param device flash memory string descriptor
 * @return device memory map
 */
QList<DFU_TargetMemoryMap> DFU_LL::getDeviceMemoryMap(QString flashDescStr)
{
    QList<DFU_TargetMemoryMap> memMap;
    QStringList str_targets = flashDescStr.split('@', Qt::SkipEmptyParts);
    for(int i = 0; i < str_targets.size(); i++)
    {
        DFU_TargetMemoryMap tempMemMap;
        DFU_FlashSector temp_sector;
        bool isConverted = false;

        QStringList str_parts = str_targets.at(i).split('/', Qt::SkipEmptyParts);
        tempMemMap.memoryName = str_parts.at(0);
        tempMemMap.startAddress = str_parts.at(1).sliced(2, str_parts.at(1).size()-2).toInt(&isConverted, 16);
        if(!isConverted) continue;
        uint32_t currentSectorAddress = tempMemMap.startAddress;
        // get sectors data
        QStringList sectorsDataStr = str_parts.at(2).split(',', Qt::SkipEmptyParts);
        for(int j = 0; j < sectorsDataStr.size(); j++)
        {
            QStringList sectorsDataParts = sectorsDataStr.at(j).split('*', Qt::SkipEmptyParts);
            uint32_t sectorsNum = sectorsDataParts.at(0).toInt(&isConverted, 10);
            uint32_t sectorSize = sectorsDataParts.at(1).sliced(0, sectorsDataParts.at(1).size()-2).toInt(&isConverted, 10);
            int sec_type = 0;
            if(!isConverted) continue;
            QChar multiplier = sectorsDataParts.at(1).at(sectorsDataParts.at(1).size()-2);
            QChar sector_type = sectorsDataParts.at(1).at(sectorsDataParts.at(1).size()-1);
            // get sector size
            if(multiplier == 'B') sectorSize *= 1;
            else if(multiplier == 'K') sectorSize *= 1024;
            else if(multiplier == 'M') sectorSize *= 1048576;
            // get sector type
            if(sector_type == 'a') sec_type = DFU_FlashSector::SectorType::Readable;
            else if(sector_type == 'b') sec_type = DFU_FlashSector::SectorType::Eraseable;
            else if(sector_type == 'c') sec_type = DFU_FlashSector::SectorType::Readable|DFU_FlashSector::SectorType::Eraseable;
            else if(sector_type == 'd') sec_type = DFU_FlashSector::SectorType::Writable;
            else if(sector_type == 'e') sec_type = DFU_FlashSector::SectorType::Writable|DFU_FlashSector::SectorType::Readable;
            else if(sector_type == 'f') sec_type = DFU_FlashSector::SectorType::Writable|DFU_FlashSector::SectorType::Eraseable;
            else if(sector_type == 'g') sec_type = DFU_FlashSector::SectorType::Writable|DFU_FlashSector::SectorType::Readable|DFU_FlashSector::SectorType::Eraseable;
            // add sectors to map
            for(uint32_t k = 0; k < sectorsNum; k++)
            {
                temp_sector.address = currentSectorAddress;
                temp_sector.size = sectorSize;
                temp_sector.sector_type = sec_type;
                tempMemMap.sectorsMap.append(temp_sector);
                // increase current sector address
                currentSectorAddress += sectorSize;
            }
        }
        memMap.append(tempMemMap);
    }

    return memMap;
}
