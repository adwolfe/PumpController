#include "condinterface.h"
#include <QDebug>
#include <QDateTime>
#include <QMetaEnum>

CondInterface::CondInterface(ConductivityMeterType meterType, QObject* parent)
    : QObject(parent),
      serial(new QSerialPort(this)),
      meterType(meterType),
      epu357SamplingMode(false) {

    const char* meterName = (meterType == ConductivityMeterType::eDAQ_EPU357) ? "eDAQ EPU357" : "Thermo Orion EC112";
    qDebug() << "Creating CondInterface for" << meterName;

    connect(serial, &QSerialPort::readyRead, this, &CondInterface::handleReadyRead);
    connect(serial, &QSerialPort::errorOccurred, this, &CondInterface::handleError);

    workerThread = new QThread;
    condWorker = new CondWorker(this, nullptr);
    condWorker->moveToThread(workerThread);
    workerThread->start();
    QMetaObject::invokeMethod(condWorker, "initialize", Qt::QueuedConnection); //starts timer after moved

    connect(this, &CondInterface::sendCommand, condWorker, &CondWorker::enqueueCommand, Qt::QueuedConnection);
    connect(condWorker, &CondWorker::condCommandReady, this, &CondInterface::handleCommand, Qt::QueuedConnection);

}

void CondInterface::shutdown() {
    if (workerThread) {
        workerThread->quit();
        workerThread->wait();
        delete workerThread;
        workerThread = nullptr;
    }
    // this is equivalent to closePort()
    if (serial->isOpen()) {
        serial->close();
    }
}

CondInterface::~CondInterface() {
    shutdown();
}

void CondInterface::handleCommand(const QString& cmd)
{
    sendToMeter(cmd);
}


bool CondInterface::connectToMeter(const QString &portName) {
    if (serial->isOpen()) {
        serial->close();
    }

    // Configure serial port based on meter type
    qint32 baudRate = (meterType == ConductivityMeterType::eDAQ_EPU357) ?
                      QSerialPort::Baud115200 : QSerialPort::Baud9600;

    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred("Failed to open port: " + serial->errorString());
        return false;
    }

    serial->setDataTerminalReady(true);
    serial->setRequestToSend(true);

    qDebug() << "Meter port opened successfully:" << serial->portName() << "at" << baudRate << "baud";

    // Meter-specific initialization
    if (meterType == ConductivityMeterType::eDAQ_EPU357) {
        // EPU357 initialization sequence
        // Wait a moment for device to be ready
        QThread::msleep(100);

        emit sendCommand("\r");

        QThread::msleep(100);

        // Set range to auto (20 mS is good for most applications)
        emit sendCommand("set range 20 auto\r");

        // Optional: Set cell constant if known (uncomment and adjust if needed)
        emit sendCommand("set k 1.0\r");

        // Enter on-demand sampling mode
        emit sendCommand("sample ascii #\r");
        epu357SamplingMode = true;

        qDebug() << "EPU357 initialized"; // in on-demand sampling mode";
    } else {
        // Thermo Orion EC112 - legacy meter
        // Optional: Set RTC (currently commented out)
        QDateTime now = QDateTime::currentDateTime();
        QString command = QString("SETRTC %1-%2-%3-%4-%5-%6-3\r")
                              .arg(now.date().year(), 4, 10, QChar('0'))
                              .arg(now.date().month(), 2, 10, QChar('0'))
                              .arg(now.date().day(), 2, 10, QChar('0'))
                              .arg(now.time().hour(), 2, 10, QChar('0'))
                              .arg(now.time().minute(), 2, 10, QChar('0'))
                              .arg(now.time().second(), 2, 10, QChar('0'));
        emit sendCommand(command);

        //qDebug() << "Thermo Orion EC112 connected";
    }

    return true;
}

void CondInterface::getMeasurement()
{
    //qDebug() << "CONDINTERFACE: Emitting measurement request";
    QString cmd;
    if (meterType == ConductivityMeterType::eDAQ_EPU357) {
        // EPU357: Send '#' to trigger a reading in on-demand sampling mode
        cmd = "#\r";
    } else {
        // Thermo Orion EC112: Send GETMEAS command
        cmd = "GETMEAS\r";
    }
    emit sendCommand(cmd);
}

bool CondInterface::sendToMeter(const QString &cmd)
{
    QByteArray packet = cmd.toUtf8();
    //packet = packet.append("\r");
    qint64 bytesWritten = serial->write(packet);
    //qDebug() << "CondInterface sending to serial port" << packet;
    return bytesWritten == packet.size();

}

void CondInterface::handleReadyRead() {
    serialBuffer.append(serial->readAll());
    //qDebug() << "Ready to read";

    if (meterType == ConductivityMeterType::eDAQ_EPU357) {
        // EPU357 uses newline-terminated responses
        while (true) {
            int endIndex = serialBuffer.indexOf('\n');

            if (endIndex == -1)
                break; // No full response yet

            QByteArray frame = serialBuffer.left(endIndex + 1);
            serialBuffer.remove(0, endIndex + 1);

            QString response = QString::fromLatin1(frame).trimmed();
            if (!response.isEmpty()) {
                parseEPU357Response(response);
            }
        }
    } else {
        // Thermo Orion EC112 uses '>' as terminator
        while (true) {
            int endIndex = serialBuffer.indexOf('>');

            if (endIndex == -1)
                break; // No full response yet

            QByteArray frame = serialBuffer.left(endIndex + 1);  // Include '>'
            serialBuffer.remove(0, endIndex + 1);                // Remove from buffer

            QString response = QString::fromLatin1(frame).trimmed();
            if (!response.isEmpty()) {
                parseOrionResponse(response);
            }
        }
    }
}

void CondInterface::parseOrionResponse(const QString &response) {
    //qDebug() << "Parsed Orion response:" << response;

    if (response.contains("RTC updated")) {
        emit messageReceived(response);
    }
    else if (response.contains("Conductivity")) {
        //qDebug()<<"Measurement detected";
        QStringList fields = response.split(',');

        if (fields.size() >= 12) {
            CondReading reading;
            reading.value = fields[9].toDouble();         // e.g., "0.00"
            reading.units = fields[10].trimmed();          // e.g., "uS/cm"

            //qDebug() << "Conductivity reading:" << reading.value << reading.units;
            emit measurementReceived(reading);
        } else {
            emit errorOccurred("Malformed GETMEAS response");
        }
    }
    else {
        emit messageReceived("Unknown response: " + response);
    }
}

void CondInterface::parseEPU357Response(const QString &response) {
    qDebug() << "Parsed EPU357 response:" << response;

    // Ignore prompts
    if (response == "EPU357>") {
        return;
    }

    // Ignore initialization messages
    if (response.startsWith("EPU357 Sampling") || response.startsWith("EPU357 Range")) {
        emit messageReceived(response);
        return;
    }

    // Parse measurement responses
    // Format can be:
    // "12.398184 mS" (in on-demand sampling mode)
    // "EPU357 Reading 12.399171 mS" (from 'r' command)

    QString valueLine = response;

    // If it starts with "EPU357 Reading", extract the part after "Reading"
    if (response.startsWith("EPU357 Reading ")) {
        valueLine = response.mid(15).trimmed(); // Skip "EPU357 Reading "
    }

    // Now parse the value and units
    // Format: "12.398184 mS" or "12.398184 mS/cm" or "12.398184 ppm"
    QStringList parts = valueLine.split(' ', Qt::SkipEmptyParts);

    if (parts.size() >= 2) {
        bool ok;
        double value = parts[0].toDouble(&ok);

        if (ok) {
            CondReading reading;
            reading.value = value;
            reading.units = parts[1]; // "mS", "mS/cm", "ppm", etc.

            // Convert units if needed for consistency with Orion meter
            // EPU357 reports in mS, but we might want mS/cm for specific conductivity
            // For now, keep as-is and let the application handle unit conversion if needed

            //qDebug() << "EPU357 Conductivity reading:" << reading.value << reading.units;
            emit measurementReceived(reading);
        } else {
            emit messageReceived("Could not parse value from: " + response);
        }
    } else if (parts.size() == 1) {
        // Might be a single numeric value (from 'v' command, though we're not using it)
        bool ok;
        double value = parts[0].toDouble(&ok);
        if (ok) {
            CondReading reading;
            reading.value = value;
            reading.units = "mS"; // Default unit
            emit measurementReceived(reading);
        } else {
            emit messageReceived("EPU357: " + response);
        }
    } else {
        // Some other message (confirmation, error, etc.)
        emit messageReceived("EPU357: " + response);
    }
}

void CondInterface::handleError(QSerialPort::SerialPortError error) {
    if (error != QSerialPort::NoError) {
        // Get error name as string
        const QMetaObject &mo = QSerialPort::staticMetaObject;
        int index = mo.indexOfEnumerator("SerialPortError");
        QMetaEnum metaEnum = mo.enumerator(index);
        QString errorStr = QString::fromLatin1(metaEnum.valueToKey(error));

        qWarning() << "Serial error occurred:" << errorStr << "(" << error << ")";

        if (error == QSerialPort::ResourceError) {
            serial->close();
            emit errorOccurred(errorStr);  // You might want to pass the string too
        }
    }
}
