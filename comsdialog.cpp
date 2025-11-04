#include "comsdialog.h"
#include <QSerialPortInfo>
#include <QLabel>
#include <QVBoxLayout>

COMsDialog::COMsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    combo_com_pump->addItem("None");
    combo_com_cond->addItem("None");
    //combo_com_pump->addItem("TEST");
    //combo_com_cond->addItem("TEST");

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        QString port = info.portName();
        combo_com_pump->addItem(port);
        combo_com_cond->addItem(port);
    }

    // Add meter type selector
    QComboBox *combo_meter_type = new QComboBox(this);
    combo_meter_type->setObjectName("combo_meter_type");
    combo_meter_type->addItem("eDAQ EPU357 (Preferred)", static_cast<int>(ConductivityMeterType::eDAQ_EPU357));
    combo_meter_type->addItem("Thermo Orion EC112 (Legacy)", static_cast<int>(ConductivityMeterType::ThermoOrion_EC112));
    combo_meter_type->setCurrentIndex(0); // Default to EPU357

    QLabel *label_meter = new QLabel("Conductivity Meter Type:", this);

    // Add to the form layout (assuming there's a formLayout in the UI)
    // If not, we'll add it to the main layout
    if (formLayout) {
        formLayout->addRow(label_meter, combo_meter_type);
    }

    connect(combo_com_cond, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &COMsDialog::updatePump);
    connect(combo_com_pump, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &COMsDialog::updateCond);
}

void COMsDialog::updateCond()
{
    if (combo_com_pump->currentIndex() == combo_com_cond->currentIndex() && combo_com_pump->currentIndex() > 0) {
        combo_com_cond->setCurrentIndex(combo_com_cond->currentIndex() - 1);
    }
}

void COMsDialog::updatePump()
{
    if (combo_com_pump->currentIndex() == combo_com_cond->currentIndex() && combo_com_pump->currentIndex() > 0) {
        combo_com_pump->setCurrentIndex(combo_com_pump->currentIndex() - 1);
    }
}

void COMsDialog::accept()
{
    // Get the meter type from the combo box
    QComboBox *combo_meter_type = findChild<QComboBox*>("combo_meter_type");
    ConductivityMeterType meterType = ConductivityMeterType::eDAQ_EPU357; // Default

    if (combo_meter_type) {
        int meterTypeInt = combo_meter_type->currentData().toInt();
        meterType = static_cast<ConductivityMeterType>(meterTypeInt);
    }

    emit coms(combo_com_cond->currentText(), combo_com_pump->currentText(), meterType);
    QDialog::accept();
}
