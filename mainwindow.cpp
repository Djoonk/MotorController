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
    QByteArray data = btSocket->readAll();

    if (data.size() < 4)
        return;

    qDebug() << "RX:" << data;

    const uint8_t *packet = reinterpret_cast<const uint8_t *>(data.constData());

    // Перевірка початку пакета
    if (packet[0] != 0xAA)
        return;

    // Перевірка CRC
    uint8_t crc = packet[0] ^ packet[1] ^ packet[2];
    if (crc != packet[3])
        return;

    if (data.size() >= 4)
    {
        const uint8_t *packet = reinterpret_cast<const uint8_t *>(data.constData());

        if (packet[0] == 0xAA && packet[1] == CMD_PROTOCOL)
        {
            uint8_t protocol = packet[2];

            ui->comboBox->blockSignals(true);
            ui->comboBox->setCurrentIndex(protocol);
            ui->comboBox->blockSignals(false);
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

