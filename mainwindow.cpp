#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <QAbstractSocket>

#include <QCoreApplication>
#include <QPermissions>
#include <QBluetoothPermission>


#define ESP32_BT_ADDRESS "EC:62:60:9C:B9:2E"


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
                sendCommand(QString("throttle %1").arg(throttleValue));
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
        sendCommand("disarm");
        throttleTimer->stop();
        throttleValue = 0;
        ui->verticalSlider->setValue(0);
        ui->verticalSlider->setValue(0);
        ui->verticalSlider->setEnabled(false);
    }
    else
    {
        ui->armButton->setText("DISARM");
        armState = true;
        sendCommand("arm");
        throttleTimer->start();
        ui->verticalSlider->setEnabled(true);

        lastThrottleSent = -1;
    }
}


void MainWindow::on_stopButton_clicked()
{
    sendCommand("stop");

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
    qDebug() << "RX:" << data;

    if (data.startsWith("PROTOCOL "))
    {
        int index = data.mid(9).trimmed().toInt();

        ui->comboBox->blockSignals(true);
        ui->comboBox->setCurrentIndex(index);
        ui->comboBox->blockSignals(false);
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

void MainWindow::sendCommand(const QString &cmd)
{
    if (btSocket->state() != QBluetoothSocket::SocketState::ConnectedState)
    {
        qDebug() << "Bluetooth not connected";
        return;
    }

    QByteArray data = cmd.toUtf8();
    data.append('\n');

    btSocket->write(data);

    qDebug() << "TX:" << cmd;
}



void MainWindow::on_verticalSlider_valueChanged(int value)
{
    throttleValue = value;
    ui->throttleLabel->setText(QString("%1 %").arg(value));
}

// void MainWindow::on_comboBoxProtocol_currentIndexChanged(int index)
// {
//     if (!btSocket)
//         return;

//     if (btSocket->state() != QBluetoothSocket::SocketState::ConnectedState)
//         return;

//     QString command;

//     switch(index)
//     {
//     case 0:
//         command = "protocol pwm\n";
//         break;

//     case 1:
//         command = "protocol dshot300\n";
//         break;

//     case 2:
//         command = "protocol dshot600\n";
//         break;

//     case 3:
//         command = "protocol bdshot\n";
//         break;

//     default:
//         return;
//     }

//     btSocket->write(command.toUtf8());

//     qDebug() << "TX:" << command;
// }

void MainWindow::on_comboBox_activated(int index)
{
    if (!btSocket)
        return;

    if (btSocket->state() != QBluetoothSocket::SocketState::ConnectedState)
        return;

    QString command;

    switch(index)
    {
    case 0:
        command = "protocol pwm\n";
        break;

    case 1:
        command = "protocol dshot300\n";
        break;

    case 2:
        command = "protocol dshot600\n";
        break;

    case 3:
        command = "protocol bdshot\n";
        break;

    default:
        return;
    }

    btSocket->write(command.toUtf8());

    qDebug() << "TX:" << command;
}

