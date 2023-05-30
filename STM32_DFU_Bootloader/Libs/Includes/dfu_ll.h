#ifndef DFU_LL_H
#define DFU_LL_H

#include <QObject>
#include "libusb.h"
#include "dfu_misc.h"

#define DFU_BMREQUEST               0x21
#define USB_DFU_DESC_SIZ               9
#define DFU_DESCRIPTOR_TYPE         0x21
#define USBD_IDX_INTERFACE_STR      0x05

#define ATTR_DNLOAD_CAPABLE         0x01
#define ATTR_UPLOAD_CAPABLE         0x02
#define ATTR_MANIFESTATION_TOLERANT 0x04
#define ATTR_WILL_DETACH            0x08
#define ATTR_ST_CAN_ACCELERATE      0x80

/**************************************************/
/* DFU Requests  DFU states                       */
/**************************************************/
#define APP_STATE_IDLE                 0        /** Device is running its normal application */
#define APP_STATE_DETACH               1        /** Device is running its normal application, has received the DFU_DETACH request, and is waiting for a USB reset */
#define DFU_STATE_IDLE                 2        /** Device is operating in the DFU mode and is waiting for requests */
#define DFU_STATE_DNLOAD_SYNC          3        /** Device has received a block and is waiting for the Host to solicit the status via DFU_GETSTATUS */
#define DFU_STATE_DNLOAD_BUSY          4        /** Device is programming a control-write block into its non volatile memories */
#define DFU_STATE_DNLOAD_IDLE          5        /** Device is processing a download operation. Expecting DFU_DNLOAD requests */
#define DFU_STATE_MANIFEST_SYNC        6        /** Device has received the final block of firmware from the Host and is waiting for
                                                    receipt of DFU_GETSTATUS to begin the Manifestation phase
                                                    or device has completed the Manifestation phase and is waiting for receipt of DFU_GETSTATUS */
#define DFU_STATE_MANIFEST             7        /** Device is in the Manifestation phase. */
#define DFU_STATE_MANIFEST_WAIT_RESET  8        /** Device has programmed its memories and is waiting for a USB reset or a power on reset. */
#define DFU_STATE_UPLOAD_IDLE          9        /** The device is processing an upload operation. Expecting DFU_UPLOAD requests. */
#define DFU_STATE_ERROR                10       /** An error has occurred. Awaiting the DFU_CLRSTATUS request. */

/**************************************************/
/* DFU errors                                     */
/**************************************************/

#define DFU_ERROR_NONE              0x00        /** No error condition is present */
#define DFU_ERROR_TARGET            0x01        /** File is not targeted for use by this device */
#define DFU_ERROR_FILE              0x02        /** File is for this device but fails some vendor-specific verification test */
#define DFU_ERROR_WRITE             0x03        /** Device id unable to write memory */
#define DFU_ERROR_ERASE             0x04        /** Memory erase function failed */
#define DFU_ERROR_CHECK_ERASED      0x05        /** Memory erase check failed */
#define DFU_ERROR_PROG              0x06        /** Program memory function failed */
#define DFU_ERROR_VERIFY            0x07        /** Programmed memory failed verification */
#define DFU_ERROR_ADDRESS           0x08        /** Cannot program memory due to received address that is out of range */
#define DFU_ERROR_NOTDONE           0x09        /** Received DFU_DNLOAD with wLength = 0, but device does not think it has all the data yet */
#define DFU_ERROR_FIRMWARE          0x0A        /** Device’s firmware is corrupted. It cannot return to run-time operations */
#define DFU_ERROR_VENDOR            0x0B        /** iString indicates a vendor-specific error */
#define DFU_ERROR_USB               0x0C        /** Device detected unexpected USB reset signaling */
#define DFU_ERROR_POR               0x0D        /** Device detected unexpected power on reset */
#define DFU_ERROR_UNKNOWN           0x0E        /** Something went wrong, but the device does not know what it was */
#define DFU_ERROR_STALLEDPKT        0x0F        /** Device stalled an unexpected request */

class DFU_LL : public QObject
{
    Q_OBJECT
public:
    DFU_LL(bool *is_LL_inited);
    ~DFU_LL();

    QList<USB_DevInfo*> GetDevicesList(void);
    bool DFU_OpenDevice(USB_DevInfo *dev);
    bool DFU_CloseDevice(void);
    DFU_Attributes* DFU_GetAttributes(void);
    bool DFU_GetDeviceName(QString* dev_name);

    // DFU commands
    bool DFU_Detach(uint16_t timeout);
    bool DFU_Dnload(uint16_t wBlockNum, uint8_t* data, uint16_t length);
    bool DFU_Upload(uint16_t wBlockNum, uint8_t* data, uint16_t length);
    bool DFU_GetStatus(DFU_Status *status);
    bool DFU_ClearStatus(void);
    bool DFU_GetState(uint8_t *state);
    bool DFU_Abort(void);
    bool DFU_GetCommands(uint8_t* commands_list, uint16_t length);
    bool DFU_SetAddressPointer(uint32_t address);
    bool DFU_Erase(bool isMassErase, uint32_t pageAddress);
    bool DFU_ReadUnprotect(void);
    bool DFU_Leave(void);
    /** extended DFU requests */
    bool DFU_LockFlashRead(void);
    bool DFU_SaveVerifyData(void);
    bool DFU_SendChecksum(uint32_t checksum);

private:
    // members
    libusb_device_handle *dfu_device = Q_NULLPTR;     //указатель на устройство
    bool isLibusbInited = false;
    // enums
    /**
     * @brief The wValueCommand enum
     * Набор команд поля wValue в Control Transfer
     * @anchor wValueCommand
     */
    enum wValueCommand : int {
        Read_Memory = 0x02,             /**< Команда чтения памяти МК в режиме DFU_UPLOAD (режим UPLOAD)  */
        Get_Command = 0x00,             /**< Команда для получения разрешенного набора комманд от МК (режим UPLOAD)*/
        Write_Memory = 0x02,            /**< Команда записи в память (режим DNLOAD) */
        Set_Address_Pointer = 0x07 ,    /**< Виртуальная команда установки адресса (режим DNLOAD) @anchor SetAddress*/
        Erase = 0x00,                   /**< Виртуальная команда стирания (режим DNLOAD) @anchor Erase */
        Read_Unprotect = 0x00,          /**< Виртуальная команда чтения (не использовать)(режим DNLOAD) */
        Leave_DFU = 0x010,              /**< Виртуальная команда выхода из бутлодера (режим DNLOAD) */
    };

    /**
     * @brief The firstByteCommand enum
     * Набор команд первых байт поля data в Control Transfer
     * @anchor firstByteCommand
     */
    enum firstByteCommand : int {

        Write_MemoryFB = 0x00,          /**< Виртуальная команда начала записи в память (не используется) (режим DNLOAD) */
        Set_Address_PointerFB = 0x21,   /**< Команда установки указателя на аддресс (режим DNLOAD) */
        EraseFB = 0x41,                 /**< Команда стирания (режим DNLOAD) */
        Read_UnprotectFB = 0x92,        /**< Команда чтения (не используется) (режим DNLOAD) */
        Leave_DFUFB = 0x00,             /**< Команда выхода из бутлодера (не используется) (режим DNLOAD) */
        //****************

    };
    /**
     * @brief The bRequestCommand enum
     * Набор команд поля bRequest в Control Transfer
     * @anchor bRequestCommand
     */
    enum bRequestCommand : int {
        DFU_DETACH = 0x00,              /**< Команда выхода из бутлодера */
        DFU_DNLOAD = 0x01,              /**< Команда входа в режим DNLOAD */
        DFU_UPLOAD = 0x02,              /**< Команда входа в режим UPLOAD */
        GET_STATUS = 0x03,              /**< Команда получения текущего состояния */
        DFU_CLRSTATUS = 0x04,           /**< Команда удаления текущего состояния (не использовать) */
        DFU_GETSTATE = 0x05,            /**< Команда получения набора состояний, включая предыдущую ошибку */
        DFU_ABORT = 0x06,               /**< Команда прерывания предыдущей операции (выхода из режимов DNLOAD и UPLOAD) */
        /** extended DFU requests */
        DFU_READ_LOCK = 0x07,           /**< Команда установки защиты flash-памяти от чтения */
        DFU_SAVE_VERIFY_DATA = 0x0A,    /**< Команда сохранения чек-суммы и параметров образа прошивки */
        DFU_SEND_CHECKSUM = 0x0B        /**< Команда передачи чек-суммы */
    };
    // functions
    bool controlTransfer(uint8_t requestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength);
    DFU_Attributes* dfu_attrs = Q_NULLPTR;
    QList<DFU_TargetMemoryMap> getDeviceMemoryMap(QString flashDescStr);
    QList<DFU_TargetMemoryMap> memory_map;
    QString deviceName = QString("");
};

#endif // DFU_LL_H
