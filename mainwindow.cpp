#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <QAbstractSocket>

#include <QCoreApplication>
#include <QPermissions>
#include <QBluetoothPermission>

// #define ESP32_BT_ADDRESS "EC:62:60:9C:B9:2E"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->verticalSlider, &QSlider::valueChanged,
            this, &MainWindow::on_verticalSlider_valueChanged);
    ui->verticalSlider->setRange(0, 100);
    ui->verticalSlider->setValue(0);
    ui->verticalSlider->setEnabled(false);

    btSocket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);
    discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);

    throttleTimer = new QTimer(this);
    throttleTimer->setInterval(50);
    connect(throttleTimer, &QTimer::timeout, this, [this]()
            {
                if (!armState)return;

                lastThrottleSent = throttleValue;
                sendCommand(Command::Throttle, static_cast<uint8_t>(throttleValue));
            });

    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &MainWindow::deviceDiscovered);

    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &MainWindow::discoveryFinished);


    connect(btSocket, &QBluetoothSocket::connected,
            this, &MainWindow::bluetoothConnected);

    connect(btSocket, &QBluetoothSocket::disconnected,
            this, &MainWindow::bluetoothDisconnected);

    connect(btSocket, &QBluetoothSocket::errorOccurred,
            this, &MainWindow::bluetoothErrorOccurred);

    connect(btSocket, &QBluetoothSocket::readyRead,
            this, &MainWindow::bluetoothReadyRead);

    QBluetoothPermission permission;
    permission.setCommunicationModes(QBluetoothPermission::Access);

    qApp->requestPermission(permission, this,
                            [](const QPermission &permission)
                            {
                                if (permission.status() == Qt::PermissionStatus::Granted)
                                    qDebug() << "Bluetooth permission granted";
                                else
                                    qDebug() << "Bluetooth permission denied";
                            });

    localDevice = new QBluetoothLocalDevice(this);
}


MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_armButton_clicked()
{
    if(armState)
    {
        ui->armButton->setText("ARM");
        armState = false;
        sendCommand(Command::Disarm);
        throttleTimer->stop();
        throttleValue = 0;
        ui->verticalSlider->setValue(0);
        ui->verticalSlider->setEnabled(false);
    }
    else
    {
        ui->armButton->setText("DISARM");
        armState = true;
        sendCommand(Command::Arm);
        throttleTimer->start();
        ui->verticalSlider->setEnabled(true);

        lastThrottleSent = -1;
    }
}


void MainWindow::on_stopButton_clicked()
{
    sendCommand(Command::Stop);

    throttleValue = 0;
    ui->verticalSlider->setValue(0);

    lastThrottleSent = -1;
}


void MainWindow::on_connectButton_clicked()
{
    if (btSocket->state() == QBluetoothSocket::SocketState::ConnectedState)
        btSocket->disconnectFromService();
    else
        connectToESP32();
}

// **************************************************************************************************************************** //

void MainWindow::connectToESP32()
{
    ui->connectStatus->setText("Searching...");
    qDebug() << "Bluetooth discovery started";

    discoveryAgent->start();
}

void MainWindow::bluetoothConnected()
{
    qDebug() << "Bluetooth connected";

    ui->connectStatus->setText("CONNECTED");
    ui->connectStatus->setStyleSheet("color: #4ac42e;");
    ui->connectButton->setText("Disconnect");
}

void MainWindow::bluetoothDisconnected()
{
    qDebug() << "Bluetooth disconnected";

    ui->connectStatus->setText("DISCONNECTED");
    ui->connectStatus->setStyleSheet("color: #cc020c;");
    ui->connectButton->setText("Connect");

    ui->verticalSlider->setValue(0);
    ui->verticalSlider->setEnabled(false);

    throttleTimer->stop();

    throttleValue = 0;
    ui->verticalSlider->setValue(0);

    lastThrottleSent = -1;
}

void MainWindow::bluetoothErrorOccurred(QBluetoothSocket::SocketError error)
{
    qDebug() << "Socket error =" << error;
    qDebug() << "Error text =" << btSocket->errorString();

    ui->connectStatus->setText("Error");
    ui->connectButton->setText("Connect");
}

void MainWindow::bluetoothReadyRead()
{
    rxBuffer.append(btSocket->readAll());

    // Process all complete packets in the buffer
    while (rxBuffer.size() >= 4)
    {
        // Find SOF (0xAA)
        int sof = rxBuffer.indexOf(static_cast<char>(0xAA));
        if (sof < 0) {
            rxBuffer.clear();
            return;
        }
        if (sof > 0)
            rxBuffer.remove(0, sof); // discard bytes before SOF

        // Check if we have enough bytes for the packet type
        const uint8_t *raw = reinterpret_cast<const uint8_t *>(rxBuffer.constData());
        uint8_t cmd = raw[1];

        if (cmd == CMD_TELEMETRY)
        {
            // Telemetry packet: 8 bytes
            if (rxBuffer.size() < 8)
                return; // wait for more data

            // CRC check: XOR of bytes 0..6
            uint8_t crc = 0;
            for (int i = 0; i < 7; i++)
                crc ^= raw[i];

            if (crc != raw[7]) {
                rxBuffer.remove(0, 1);
                continue;
            }

            // Decode eRPM (12-bit raw value, phone converts to eRPM)
            uint16_t erpm_raw = static_cast<uint16_t>(raw[2] | (raw[3] << 8));
            double rpm = 0;
            if (erpm_raw != 0 && erpm_raw != 0x0FFF)
            {
                uint32_t period = (erpm_raw & 0x01FF) << ((erpm_raw & 0xFE00) >> 9);
                if (period > 0)
                    rpm = (600000.0 + period / 2.0) / period; // eRPM
            }

            // Temperature: 1 LSB = 1 °C
            uint8_t temp_raw = raw[4];

            // Voltage: 1 LSB = 0.25 V
            uint8_t voltage_raw = raw[5];
            double voltage = voltage_raw * 0.25;

            // Current: 1 LSB = 0.5 A
            uint8_t current_raw = raw[6];
            double current = current_raw * 0.5;

            // Update labels
            ui->l_rpm->setText(QString::number(static_cast<int>(rpm)));
            ui->l_voltage->setText(QString::number(voltage, 'f', 2) + " V");
            ui->l_current->setText(QString::number(current, 'f', 1) + " A");
            ui->l_temp->setText(QString::number(temp_raw) + " °C");

            rxBuffer.remove(0, 8);
        }
        else
        {
            // Command packet: 4 bytes
            if (rxBuffer.size() < 4)
                return;

            // CRC check
            uint8_t crc = raw[0] ^ raw[1] ^ raw[2];
            if (crc != raw[3]) {
                rxBuffer.remove(0, 1);
                continue;
            }

            if (cmd == CMD_PROTOCOL)
            {
                uint8_t protocol = raw[2];
                ui->comboBox->blockSignals(true);
                ui->comboBox->setCurrentIndex(protocol);
                ui->comboBox->blockSignals(false);
            }

            rxBuffer.remove(0, 4);
        }
    }
}

void MainWindow::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    qDebug() << device.name() << device.address().toString();

    if (device.name() == "MotorTest_ESP32")
    {
        qDebug() << "ESP32 found";

        discoveryAgent->stop();

        btSocket->connectToService(
            device.address(),
            QBluetoothUuid(QBluetoothUuid::ServiceClassUuid::SerialPort));
    }
}


void MainWindow::discoveryFinished()
{
    qDebug() << "Discovery finished";

    if (btSocket->state() != QBluetoothSocket::SocketState::ConnectedState)
    {
        ui->connectStatus->setText("ESP32 not found");
    }
}

void MainWindow::sendCommand(Command command, uint8_t value)
{
    qDebug() << "sendCommand() CALLED";
    // if (!btSocket || btSocket->state() != QBluetoothSocket::ConnectedState)
    // {
    //     qDebug() << "Bluetooth socket is not connected";
    //     return;
    // }

    const uint8_t cmd = static_cast<uint8_t>(command);
    const uint8_t crc = 0xAA ^ cmd ^ value;

    QByteArray packet;
    packet.reserve(4);
    packet.append(static_cast<char>(0xAA));
    packet.append(static_cast<char>(cmd));
    packet.append(static_cast<char>(value));
    packet.append(static_cast<char>(crc));

    qDebug() << "TX:" << packet.toHex(' ').toUpper();

    const qint64 written = btSocket->write(packet);
    if (written != packet.size())
        qDebug() << "Only" << written << "of" << packet.size() << "bytes queued";
}

void MainWindow::on_verticalSlider_valueChanged(int value)
{
    throttleValue = value;
    ui->throttleLabel->setText(QString("%1 %").arg(value));
}


void MainWindow::on_comboBox_activated(int index)
{
    if (!btSocket)
        return;

    if (btSocket->state() != QBluetoothSocket::SocketState::ConnectedState)
        return;

    uint8_t protocol;

    switch(index)
    {
    case 0:
        protocol = PWM;
        break;

    case 1:
        protocol = DSHOT300;
        break;

    case 2:
        protocol = DSHOT600;
        break;

    case 3:
        protocol = BIDIRECTIONAL_DSHOT;
        break;

    default:
        return;
    }

    sendCommand(Command::Protocol, protocol);
}

