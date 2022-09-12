#include "memorymapdialog.h"
#include "ui_memorymapdialog.h"

/**
 * @brief Class constructor - show target device memory map as a table: rows are matched by flash memory sectors, columns data is sector name, start address and
 *  readable/writabe/eraseabe attributes
 * @param target_mem_map: selected target flash memory device map structure
 */
MemoryMapDialog::MemoryMapDialog(const DFU_TargetMemoryMap *target_mem_map) : ui(new Ui::MemoryMapDialog)
{
    ui->setupUi(this);
    // fill target memory map table
    ui->targetMemoryMapTable->setRowCount(target_mem_map->sectorsMap.length());
    ui->targetMemoryMapTable->setColumnCount(6);

    QStringList mTableHeader;
    // define table columns
    mTableHeader<<tr("#")<<tr("Start address")<<tr("Size")<<tr("Readable")<<tr("Writable")<<tr("Eraseable");
    ui->targetMemoryMapTable->setHorizontalHeaderLabels(mTableHeader);
    ui->targetMemoryMapTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->targetMemoryMapTable->setSelectionMode(QAbstractItemView::NoSelection);
    ui->targetMemoryMapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->targetMemoryMapTable->verticalHeader()->setVisible(false);
    ui->targetMemoryMapTable->setStyleSheet("QHeaderView::section { border: 1px solid black; }");

    // fill table
    for(int i = 0; i < target_mem_map->sectorsMap.length(); i++)
    {
        // set sector number item
        QTableWidgetItem* sector_num = new QTableWidgetItem(tr("Sector %1").arg(i));
        sector_num->setTextAlignment(Qt::AlignCenter);
        ui->targetMemoryMapTable->setItem(i, 0, sector_num);

        // set sector start address item
        QTableWidgetItem* sector_start_address = new QTableWidgetItem(tr("0x%1").arg(target_mem_map->sectorsMap.at(i).address, 8, 16, QLatin1Char('0')));
        sector_start_address->setTextAlignment(Qt::AlignCenter);
        ui->targetMemoryMapTable->setItem(i, 1, sector_start_address);

        // set sector size item
        QString sectorSizeFormat;
        uint32_t ssize = target_mem_map->sectorsMap.at(i).size;
        // define sector size format
        if(ssize < 1024)
        {
            sectorSizeFormat = tr("%1 B").arg(ssize);
        }
        else if(ssize >= 1024 && ssize < 1048576)
        {
            sectorSizeFormat = tr("%1 kB").arg(ssize>>10);
        }
        else
        {
           sectorSizeFormat = tr("%1 MB").arg(ssize>>20);
        }
        QTableWidgetItem* sector_size = new QTableWidgetItem(sectorSizeFormat);
        sector_size->setTextAlignment(Qt::AlignCenter);
        ui->targetMemoryMapTable->setItem(i, 2, sector_size);

        QString ok_item = "X";
        QString no_item = "";

        // fill current sector readable property
        QTableWidgetItem* isReadableItem;
        if(target_mem_map->sectorsMap.at(i).sector_type & DFU_FlashSector::SectorType::Readable)
        {
            isReadableItem = new QTableWidgetItem(ok_item);
        }
        else
        {
            isReadableItem = new QTableWidgetItem(no_item);
        }
        isReadableItem->setTextAlignment(Qt::AlignCenter);
        ui->targetMemoryMapTable->setItem(i, 3, isReadableItem);

        // fill current sector writable property
        QTableWidgetItem* isWritableItem;
        if(target_mem_map->sectorsMap.at(i).sector_type & DFU_FlashSector::SectorType::Writable)
        {
            isWritableItem = new QTableWidgetItem(ok_item);
        }
        else
        {
            isWritableItem = new QTableWidgetItem(no_item);
        }
        isWritableItem->setTextAlignment(Qt::AlignCenter);
        ui->targetMemoryMapTable->setItem(i, 4, isWritableItem);

        // fill current sector eraseable property
        QTableWidgetItem* isEraseableItem;
        if(target_mem_map->sectorsMap.at(i).sector_type & DFU_FlashSector::SectorType::Eraseable)
        {
            isEraseableItem = new QTableWidgetItem(ok_item);
        }
        else
        {
            isEraseableItem = new QTableWidgetItem(no_item);
        }
        isEraseableItem->setTextAlignment(Qt::AlignCenter);
        ui->targetMemoryMapTable->setItem(i, 5, isEraseableItem);
    }
}

/**
 * @brief Class destructor
 */
MemoryMapDialog::~MemoryMapDialog()
{
    delete ui;
}

/**
 * @brief "Ok" button pressed event handle
 * @param none
 */
void MemoryMapDialog::on_buttonBox_accepted()
{
    this->close();
}

