/*
  Copyright © 2019 Hasan Yavuz Özderya

  This file is part of serialplot.

  serialplot is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  serialplot is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with serialplot.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PORTCONTROL_H
#define PORTCONTROL_H

#include <QWidget>
#include <QButtonGroup>
#include <QSerialPort>
#include <QIODevice>
#include <QStringList>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QTimer>
#include <QList>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>

#include "portlist.h"
#include "blegattdevice.h"

namespace Ui {
class PortControl;
}

class PortControl : public QWidget
{
    Q_OBJECT

public:
    explicit PortControl(QSerialPort* serialPort,
                         BleGattDevice* bleDevice,
                         QWidget* parent = 0);
    ~PortControl();

    QSerialPort* serialPort;
    BleGattDevice* bleDevice;
    QToolBar* toolBar();
    QIODevice* activeDevice();

    void selectPort(QString portName);
    void selectBaudrate(QString baudRate);
    void openPort();
    /// Returns maximum bit rate for current baud rate
    unsigned maxBitRate() const;

    /// Stores port settings into a `QSettings`
    void saveSettings(QSettings* settings);
    /// Loads port settings from a `QSettings`. If open serial port is closed.
    void loadSettings(QSettings* settings);

private:
    Ui::PortControl *ui;

    QButtonGroup parityButtons;
    QButtonGroup dataBitsButtons;
    QButtonGroup stopBitsButtons;
    QButtonGroup flowControlButtons;

    QToolBar portToolBar;
    QAction openAction;
    QAction loadPortListAction;
    QComboBox tbPortList;
    PortList portList;
    enum class TransportMode
    {
        Serial = 0,
        BLE = 1
    };
    TransportMode transportMode;
    QList<QBluetoothDeviceInfo> scannedBleDevices;

    QComboBox* cbTransport;
    QComboBox* cbBleDevice;
    QGroupBox* gbBle;
    QLabel* lbBleStatus;
    QPushButton* pbBleScan;
    QLineEdit* leBleServiceUuid;
    QLineEdit* leBleNotifyUuid;
    QLineEdit* leBleWriteUuid;
    QAction scanBleAction;

    /// Used to refresh pinout signal leds periodically
    QTimer pinUpdateTimer;

    /// Returns the currently selected (entered) "portName" in the UI
    QString selectedPortName();
    QBluetoothUuid parseUuid(const QString& text);
    void applyModeUi();
    bool isSerialMode() const;
    void refreshBleDeviceList();
    void connectBle();
    void disconnectBle();
    QString bleDeviceDisplayName(const QBluetoothDeviceInfo& info) const;
    /// Returns currently selected parity as text to be saved in settings
    QString currentParityText();
    /// Returns currently selected flow control as text to be saved in settings
    QString currentFlowControlText();

private slots:
    void loadPortList();
    void loadBaudRateList();
    void togglePort();
    void selectListedPort(QString portName);

    void _selectBaudRate(QString baudRate);
    void selectParity(int parity); // parity must be one of QSerialPort::Parity
    void selectDataBits(int dataBits); // bits must be one of QSerialPort::DataBits
    void selectStopBits(int stopBits); // stopBits must be one of QSerialPort::StopBits
    void selectFlowControl(int flowControl); // flowControl must be one of QSerialPort::FlowControl

    void openActionTriggered(bool checked);
    void onCbPortListActivated(int index);
    void onTbPortListActivated(int index);
    void onPortError(QSerialPort::SerialPortError error);
    void updatePinLeds(void);
    void onTransportChanged(int index);
    void scanBleDevices();
    void onBleConnectedChanged(bool connected);
    void onBleScanFinished();

signals:
    void portToggled(bool open);
    void deviceChanged(QIODevice* device);
};

#endif // PORTCONTROL_H
