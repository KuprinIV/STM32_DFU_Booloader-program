#ifndef MEMORYMAPDIALOG_H
#define MEMORYMAPDIALOG_H

#include <QDialog>
#include "dfu_misc.h"

namespace Ui {
class MemoryMapDialog;
}

class MemoryMapDialog : public QDialog
{
    Q_OBJECT

public:
    MemoryMapDialog(const DFU_TargetMemoryMap* target_mem_map);
    ~MemoryMapDialog();

private slots:
    void on_buttonBox_accepted();

private:
    Ui::MemoryMapDialog *ui;
};

#endif // MEMORYMAPDIALOG_H
