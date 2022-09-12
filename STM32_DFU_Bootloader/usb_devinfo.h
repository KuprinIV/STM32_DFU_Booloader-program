#ifndef USB_DEVINFO_H
#define USB_DEVINFO_H

#include <QObject>

class USB_DevInfo : public QObject
{
    Q_OBJECT
public:
    USB_DevInfo(ushort vid, ushort pid/*, QString name*/)
    {
        this->VID = vid;
        this->PID = pid;
    //    this->devName = name;
    }

    ushort PID;
    ushort VID;
//    QString devName;

signals:

};

#endif // USB_DEVINFO_H
