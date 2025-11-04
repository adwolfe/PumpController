#pragma once

#include <QDialog>
#include <QString>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>

#include "ui_comsdialog.h"
#include "condinterface.h"

class COMsDialog : public QDialog, private Ui::Dialog
{
    Q_OBJECT

public:
    explicit COMsDialog(QWidget *parent = nullptr);

signals:
    void coms(const QString &cond, const QString &pump, ConductivityMeterType meterType);

private slots:
    void updateCond();
    void updatePump();
    void accept() override;
};
