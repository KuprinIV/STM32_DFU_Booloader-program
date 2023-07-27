#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtConcurrent/QtConcurrent>
#include "dfu_bootloader.h"
#include "hexparser.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBootloaderError(DFU_Bootloader::ErrorCodes error_code);
    void onProgressChanged(int progress);
    void onBooloaderStatusUpdated(QString status);
    void onBootOperationFinished(void);
    void onSaveReadHexFileFinished(void);
    void on_deviceListCB_activated(int index);
    void on_uploadBtn_clicked();
    void on_eraseBtn_clicked();
    void on_downloadBtn_clicked();
    void on_returnBtn_clicked();
    void on_memoryMapTable_cellClicked(int row, int column);
    void on_memoryMapTable_cellDoubleClicked(int row, int column);
    void on_usbDevListRefreshBtn_clicked();
    void on_stm32targetRB_clicked(bool checked);
    void on_esp32targetRB_clicked(bool checked);

private:
    enum DFU_TargetDevice
    {
        STM32_DFU,
        ESP32_DFU
    };
    // members
    Ui::MainWindow *ui;
    DFU_Bootloader *dfu_boot;
    DFU_Attributes* dfu_attrs = Q_NULLPTR;
    hexParser* hex_parser  = Q_NULLPTR;
    QMap<int, QByteArray*> hexFileData;
    QString outHexFileName;
    DFU_Bootloader::DFU_BootloaderOperations currentOperation = DFU_Bootloader::NO_OPERATION;
    QFuture<void> bootOpreation;
    QFutureWatcher<void> bootOperationWatcher;
    QFuture<bool> saveReadHexFile;
    QFutureWatcher<bool> saveHexFileWatcher;

    bool isDfuDeviceOpened = false;
    int targetIndex = -1;
    DFU_TargetDevice currentTargetDev = STM32_DFU;

    // functions
    void fillMemoryMapTable(QList<DFU_TargetMemoryMap> *map);
    void showStatus(bool isError, QString text);
    void lockUI(bool isDevListDisabled);
    void unlockUI(void);
};
#endif // MAINWINDOW_H
