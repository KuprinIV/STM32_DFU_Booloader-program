#ifndef USB_DEVINFO_H
#define USB_DEVINFO_H

#include <QObject>
#include "STM32_DFU_Bootloader_Lib_global.h"

class STM32_DFU_BOOTLOADER_LIB_EXPORT USB_DevInfo : public QObject
{
    Q_OBJECT
public:
    USB_DevInfo(ushort vid, ushort pid);

    ushort PID;
    ushort VID;
};

#endif // USB_DEVINFO_H
