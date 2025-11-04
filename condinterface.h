#ifndef CONDINTERFACE_H
#define CONDINTERFACE_H

#include "condworker.h"
#include <QObject>
#include <QSerialPort>
#include <QThread>

// Supports two conductivity meters:
// 1. Thermo Orion Lab Star EC112 - legacy meter with minimal USB connectivity
// 2. eDAQ EPU357 USB Conductivity isoPod - preferred meter with full command set

// Simple struct to hold a conductivity measurement

enum class ConductivityMeterType {
    ThermoOrion_EC112,  // Legacy meter: 9600 baud, limited commands
    eDAQ_EPU357         // Preferred meter: 115200 baud, rich command set
};



class CondInterface : public QObject {
    Q_OBJECT
public:
    explicit CondInterface(ConductivityMeterType meterType = ConductivityMeterType::eDAQ_EPU357, QObject* parent = nullptr);
    ~CondInterface();

    bool connectToMeter(const QString &portName);
    void getMeasurement();
    void shutdown();
    ConductivityMeterType getMeterType() const { return meterType; }

public slots:
    void handleCommand(const QString &cmd);


signals:
    void messageReceived(const QString &data);
    void measurementReceived(CondReading reading);
    void errorOccurred(const QString &message);
    void sendCommand(const QString& cmd);

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);

private:
    bool sendToMeter(const QString &cmd);
    void parseOrionResponse(const QString &response);
    void parseEPU357Response(const QString &response);

    QThread *workerThread;
    CondWorker *condWorker;
    QSerialPort *serial;
    QByteArray serialBuffer;
    ConductivityMeterType meterType;
    bool epu357SamplingMode;  // Track if EPU357 is in on-demand sampling mode

};

#endif // CONDINTERFACE_H
