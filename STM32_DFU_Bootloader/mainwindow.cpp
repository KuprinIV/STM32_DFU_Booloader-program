#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "memorymapdialog.h"
#include <QHeaderView>
#include <QFileDialog>

/**
 * @brief Class constructor
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // define DFU bootloader class object
    dfu_boot = new DFU_Bootloader();
    connect(dfu_boot, &DFU_Bootloader::sendError, this, &MainWindow::onBootloaderError);
    connect(dfu_boot, &DFU_Bootloader::operationProgressChanged, this, &MainWindow::onProgressChanged);
    connect(dfu_boot, &DFU_Bootloader::sendStatus, this, &MainWindow::onBooloaderStatusUpdated);

    // connect slots for handling asyncronous operations result
    connect(&bootOperationWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::onBootOperationFinished);
    connect(&saveHexFileWatcher, &QFutureWatcher<bool>::finished, this, &MainWindow::onSaveReadHexFileFinished);

    // fill usb devices list combo box
    on_usbDevListRefreshBtn_clicked();
}

/**
 * @brief Class destructor
 */
MainWindow::~MainWindow()
{
    if(isDfuDeviceOpened)
    {
        isDfuDeviceOpened = false;
        dfu_boot->CloseDfuDevice();
    }

    this->disconnect();
    delete dfu_boot;
    if(hex_parser != Q_NULLPTR)
        delete hex_parser;
    delete ui;
}

/**
 * @brief Bootloader error handling slot
 * @param error_code - error code, received from bootloader operation
 */
void MainWindow::onBootloaderError(DFU_Bootloader::ErrorCodes error_code)
{
    showStatus(true, dfu_boot->GetErrorDescription(error_code));
    unlockUI();
}

/**
 * @brief Bootloader operation progress change handling slot
 * @param progress - operation progress from 0 to 100 %
 */
void MainWindow::onProgressChanged(int progress)
{
    if(progress < 0) progress = 0;
    if(progress > 100) progress = 100;

    ui->dfuOperationProgressPB->setValue(progress);
}

/**
 * @brief Bootloader operation status update handling slot
 * @param status - operation status description
 */
void MainWindow::onBooloaderStatusUpdated(QString status)
{
    showStatus(false, status);
}

/**
 * @brief QtConcurrent bootloader operation finished() event handling slot
 */
void MainWindow::onBootOperationFinished(void)
{
    unlockUI();
    switch(currentOperation)
    {
        case DFU_Bootloader::NO_OPERATION:
        case DFU_Bootloader::ERASE_MEMORY:
        default:
            break;

        case DFU_Bootloader::WRITE_FW_DATA:
            hexFileData.clear();
            // delete hex parser
            if(hex_parser != Q_NULLPTR)
            {
                delete hex_parser;
                hex_parser = Q_NULLPTR;
            }
            break;

        case DFU_Bootloader::READ_FW_DATA:
            hex_parser = new hexParser(outHexFileName, true);
            // start saving hex file task
            saveReadHexFile = QtConcurrent::run(&hexParser::createHexFile, hex_parser, &hexFileData, outHexFileName);
            saveHexFileWatcher.setFuture(saveReadHexFile);
            break;

        case DFU_Bootloader::LEAVE_DFU_MODE:
            // close DFU device
            isDfuDeviceOpened = false;
            dfu_boot->CloseDfuDevice();
            lockUI(false);
            break;
    }
    currentOperation = DFU_Bootloader::NO_OPERATION;
}

/**
 * @brief QtConcurrent saving hex file operation finished() event handling slot
 */
void MainWindow::onSaveReadHexFileFinished(void)
{
    if(saveReadHexFile.result())
    {
        showStatus(false, tr("Output hex file was saved succesfully"));
    }
    else
    {
        showStatus(true, tr("Error was occured during hex file saving"));
    }
    hexFileData.clear();
    if(hex_parser != Q_NULLPTR)
    {
        delete hex_parser;
        hex_parser = Q_NULLPTR;
    }
}

/**
 * @brief USB devices list comboBox item selection event handling slot
 * @param index: index of combo box selected item
 */
void MainWindow::on_deviceListCB_activated(int index)
{
    // close previous device
    if(isDfuDeviceOpened)
    {
        isDfuDeviceOpened = false;
        dfu_boot->CloseDfuDevice();
    }

    USB_DevInfo* dev = ui->deviceListCB->itemData(index).value<USB_DevInfo*>();
    isDfuDeviceOpened = dfu_boot->OpenDfuDevice(dev);
    if(isDfuDeviceOpened)
    {
        dfu_attrs = dfu_boot->GetDfuAttributes();
        if(dfu_attrs != Q_NULLPTR)
        {
            if(dfu_attrs->memoryMap->isEmpty())
            {
                showStatus(true, "Can't read device memory map");
                isDfuDeviceOpened = false;
                dfu_boot->CloseDfuDevice();
                return;
            }
            fillMemoryMapTable(dfu_attrs->memoryMap);
        }
        else
        {
            showStatus(true, "Can't read DFU attributes");
            isDfuDeviceOpened = false;
            dfu_boot->CloseDfuDevice();
            return;
        }

        // update UI
        showStatus(false, tr("Device opened"));
        ui->vidLE->setText(QString("0x%1").arg(dev->VID, 4, 16, QChar('0')));
        ui->pidLE->setText(QString("0x%1").arg(dev->PID, 4, 16, QChar('0')));
        ui->operationsGB->setEnabled(true);

        // disable control buttons before target memory selection
        lockUI(false);
    }
}

/**
 * @brief Upload button pressed event handling slot
 */
void MainWindow::on_uploadBtn_clicked()
{
    if(targetIndex == -1)
    {
        showStatus(true, tr("Please select target"));
        return;
    }
    outHexFileName = QFileDialog::getSaveFileName(this, tr("Save File"),"C:/Users", "(*.hex)");
    if(!outHexFileName.isEmpty())
    {
        hexFileData.clear();
        // save empty hex file
        QFile mFile(outHexFileName);
        if(!mFile.open(QFile::WriteOnly))
        {
            showStatus(true, tr("Can't create hex-file"));
            return;
        }
        mFile.close();
        // lock UI
        lockUI(true);
        // start upload task
        currentOperation = DFU_Bootloader::READ_FW_DATA;
        bootOpreation = QtConcurrent::run(&DFU_Bootloader::ReadFwData, dfu_boot, targetIndex, &hexFileData);
        bootOperationWatcher.setFuture(bootOpreation);
    }
}

/**
 * @brief Erase button pressed event handling slot
 */
void MainWindow::on_eraseBtn_clicked()
{
    if(targetIndex == -1)
    {
        showStatus(true, tr("Please select target"));
        return;
    }
    // lock UI
    lockUI(true);
    // start erase memory task
    currentOperation = DFU_Bootloader::ERASE_MEMORY;
    bootOpreation = QtConcurrent::run(&DFU_Bootloader::EraseMemory, dfu_boot, targetIndex);
    bootOperationWatcher.setFuture(bootOpreation);
}

/**
 * @brief Download button pressed event handling slot
 */
void MainWindow::on_downloadBtn_clicked()
{
    if(targetIndex == -1)
    {
        showStatus(true, tr("Please select target"));
        return;
    }
    QString hexPilePath = QFileDialog::getOpenFileName(this, tr("Open File"), tr("C:/Users"), "(*.hex)");
    if(!hexPilePath.isEmpty())
    {
        hexFileData.clear();
        hex_parser = new hexParser(hexPilePath, false);
        if(hex_parser->parseFile(&hexFileData))
        {
            // lock UI
            lockUI(true);
            // start program firmware data task
            currentOperation = DFU_Bootloader::WRITE_FW_DATA;
            bootOpreation = QtConcurrent::run(&DFU_Bootloader::WriteFwData, dfu_boot, targetIndex, &hexFileData);
            bootOperationWatcher.setFuture(bootOpreation);
        }
        else
        {
            showStatus(true, tr("Incorrect firmware file"));
            delete hex_parser;
            hex_parser = Q_NULLPTR;
        }
    }
}

/**
 * @brief Return button pressed event handling slot
 */
void MainWindow::on_returnBtn_clicked()
{
    // lock UI
    lockUI(true);
    // start leave from DFU mode task
    currentOperation = DFU_Bootloader::LEAVE_DFU_MODE;
    bootOpreation = QtConcurrent::run(&DFU_Bootloader::LeaveDfuMode, dfu_boot);
    bootOperationWatcher.setFuture(bootOpreation);
}

/**
 * @brief Device memory map table cell clicked event handling slot
 * @param row: clicked cell's row index
 * @param column: clicked cell's row column
 */
void MainWindow::on_memoryMapTable_cellClicked(int row, int column)
{
    if(column == 0)
    {
        targetIndex = row;
        showStatus(false, tr("Target %1 selected").arg(targetIndex));
        // enable control UI
        unlockUI();
    }
}

/**
 * @brief Device memory map table cell double-clicked event handling slot
 * @param row: double-clicked cell's row index
 * @param column: double-clicked cell's row column
 */
void MainWindow::on_memoryMapTable_cellDoubleClicked(int row, int column)
{
    if(column == 2)
    {
        MemoryMapDialog* mmd = new MemoryMapDialog(&(dfu_attrs->memoryMap->at(row)));
        mmd->setAttribute(Qt::WA_DeleteOnClose);
        mmd->exec();
    }
}

/**
 * @brief Refresh USB devices list button pressed event handling slot
 */
void MainWindow::on_usbDevListRefreshBtn_clicked()
{
    if(!isDfuDeviceOpened)
    {
        ui->deviceListCB->clear();
        for(USB_DevInfo* dev : dfu_boot->GetUsbDevicesList())
        {
            ui->deviceListCB->addItem(QString("VID: 0x%1; PID: 0x%2").arg(dev->VID, 4, 16, QChar('0')).arg(dev->PID, 4, 16, QChar('0')), QVariant::fromValue(dev));
        }
    }
}

/**
 * @brief Fill device memory map table in main window
 * @param map: device memory map, got from DFU_Attributes
 */
void MainWindow::fillMemoryMapTable(QList<DFU_TargetMemoryMap>* map)
{
    ui->memoryMapTable->setRowCount(map->length());
    ui->memoryMapTable->setColumnCount(3);

    QStringList mTableHeader;
    // define table columns
    mTableHeader<<tr("Target ID")<<tr("Target Name")<<tr("Available sectors");
    ui->memoryMapTable->setHorizontalHeaderLabels(mTableHeader);
    ui->memoryMapTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->memoryMapTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->memoryMapTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->memoryMapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->memoryMapTable->verticalHeader()->setVisible(false);

    // fill table
    for(int i = 0; i < map->length(); i++)
    {
        QTableWidgetItem* id = new QTableWidgetItem(tr("%1").arg(i));
        ui->memoryMapTable->setItem(i, 0, id);

        QTableWidgetItem* name = new QTableWidgetItem(tr("%1").arg(map->at(i).memoryName));
        ui->memoryMapTable->setItem(i, 1, name);

        QTableWidgetItem* sectors_num = new QTableWidgetItem(tr("%1").arg(map->at(i).sectorsMap.length()));
        ui->memoryMapTable->setItem(i, 2, sectors_num);
    }
}

/**
 * @brief Set text of status label in main window
 * @param isError: true - label text will be red, false -  label text will be green
 * @param: text: status or error description text
 */
void  MainWindow::showStatus(bool isError, QString text)
{
    QString styleSheet = "QLabel {color : green; }";
    if(isError)
    {
        styleSheet = "QLabel {color : red; }";
    }
    ui->statusLabel->setStyleSheet(styleSheet);
    ui->statusLabel->setText(text);
}

/**
 * @brief Lock UI during DFU bootloader operation
 * @param isDevListDisabled: true - USB devices list comboBox is disabled too, false - USB devices list comboBox stays enabled
 */
void MainWindow::lockUI(bool isDevListDisabled)
{
    ui->deviceListCB->setEnabled(!isDevListDisabled);
    ui->downloadBtn->setEnabled(false);
    ui->uploadBtn->setEnabled(false);
    ui->eraseBtn->setEnabled(false);
    ui->returnBtn->setEnabled(false);
}

/**
 * @brief Unlock UI before or after DFU bootloader operation
 */
void MainWindow::unlockUI(void)
{
    if(dfu_attrs != Q_NULLPTR)
    {
        ui->downloadBtn->setEnabled(dfu_attrs->isCanDnload);
        ui->uploadBtn->setEnabled(dfu_attrs->isCanUpload);
    }
    else
    {
        ui->downloadBtn->setEnabled(true);
        ui->uploadBtn->setEnabled(true);
    }
    ui->deviceListCB->setEnabled(true);
    ui->eraseBtn->setEnabled(true);
    ui->returnBtn->setEnabled(true);
}
