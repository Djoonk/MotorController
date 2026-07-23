#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QBluetoothDeviceDiscoveryAgent>

#include <QBluetoothSocket>
#include <QBluetoothAddress>
#include <QBluetoothUuid>
#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>

#include <QTimer>
#include <cstdint>

enum class Command : uint8_t {
    Arm       = 0x01,
    Disarm    = 0x02,
    Stop      = 0x03,
    Throttle  = 0x04,
    Protocol  = 0x05
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;


private slots:

    void connectToESP32();
    void bluetoothConnected();
    void bluetoothDisconnected();
    void bluetoothErrorOccurred(QBluetoothSocket::SocketError error);
    void bluetoothReadyRead();
    void on_armButton_clicked();
    void on_stopButton_clicked();
    void on_connectButton_clicked();
    void deviceDiscovered(const QBluetoothDeviceInfo &device);
    void discoveryFinished();
    void on_comboBox_activated(int index);

private:
    Ui::MainWindow *ui;
    bool armState = false;

    QBluetoothLocalDevice *localDevice = nullptr;

    static constexpr const char *ESP32_NAME = "MotorTest_ESP32";

    QBluetoothDeviceDiscoveryAgent *discoveryAgent = nullptr;
    QBluetoothSocket *btSocket = nullptr;

    QTimer *throttleTimer = nullptr;
    int throttleValue = 0;
    int lastThrottleSent = -1;

    void sendCommand(Command command, uint8_t value = 0);
    void on_verticalSlider_valueChanged(int value);

    void on_comboBoxProtocol_currentIndexChanged(int index);
};
#endif // MAINWINDOW_H
