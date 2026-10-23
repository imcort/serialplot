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
#include <QTcpSocket>
#include <QIODevice>
#include <QStringList>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSettings>
#include <QTimer>
#include <QList>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>

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
                         QSerialPort* markerPort,
                         QWidget* parent = 0);
    ~PortControl();

    QSerialPort* serialPort;
    BleGattDevice* bleDevice;
    /// Auxiliary device that only places markers (see `MarkerPort`)
    QSerialPort* markerPort;
    QIODevice* tcpDevice();
    QToolBar* toolBar();
    QIODevice* activeDevice();

    void selectPort(QString portName);
    void selectBaudrate(QString baudRate);
    void openPort();
    /// Returns maximum bit rate for current baud rate
    unsigned maxBitRate() const;

    /// Stores port settings into a `QSettings`
    void saveSettings(QSettings* settings);
    /// Loads port settings from a `QSettings`. Any active connection is closed.
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
    PortList portList;
    enum class TransportMode
    {
        Serial = 0,
        BLE = 1,
        TCP = 2
    };
    TransportMode transportMode;
    QList<QBluetoothDeviceInfo> scannedBleDevices;

    QTcpSocket tcpSocket;
    /// True while the main port is open. The marker input is locked then,
    /// see `updateMarkerUi()`.
    bool mainPortOpen;
    /// Tracks whether `portToggled(true)` was emitted for the TCP socket,
    /// so that the matching `false` is emitted exactly once
    bool tcpReportedOpen;

    QComboBox* cbTransport;
    QComboBox* cbBleDevice;
    QGroupBox* gbBle;
    QLabel* lbBleStatus;
    QPushButton* pbBleScan;
    QLineEdit* leBleServiceUuid;
    QCheckBox* cbBleFilterByServiceUuid;
    QCheckBox* cbBleAutoReconnect;
    QLineEdit* leBleNotifyUuid;
    QLineEdit* leBleWriteUuid;
    QGroupBox* gbTcp;
    QLineEdit* leTcpHost;
    QSpinBox* sbTcpPort;
    QLabel* lbTcpStatus;
    QAction scanBleAction;

    // marker input widgets
    QGroupBox* gbMarker;
    QComboBox* cbMarkerPort;
    QComboBox* cbMarkerBaud;
    QToolButton* pbMarkerConnect;
    QToolButton* pbMarkerRefresh;
    QLabel* lbMarkerStatus;

    /// Used to refresh pinout signal leds periodically
    QTimer pinUpdateTimer;
    QTimer bleReconnectTimer;
    QBluetoothDeviceInfo bleReconnectDevice;
    bool bleConnectionDesired;
    bool bleAutoReconnectArmed;
    bool bleReconnectAttemptInProgress;

    /// Returns the currently selected (entered) "portName" in the UI
    QString selectedPortName();
    QBluetoothUuid parseUuid(const QString& text);
    void applyModeUi();
    bool isSerialMode() const;
    bool isBleMode() const;
    bool isTcpMode() const;
    void refreshBleDeviceList();
    void updateBleDeviceListStatus();
    void scheduleBleReconnect();
    void connectBle();
    void disconnectBle();
    void connectTcp();
    void disconnectTcp();
    void loadMarkerPortList();
    void toggleMarkerPort();
    void updateMarkerUi();
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
    void onPortError(QSerialPort::SerialPortError error);
    void updatePinLeds(void);
    void onTransportChanged(int index);
    void scanBleDevices();
    void onBleConnectedChanged(bool connected);
    void onBleScanFinished();
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError error);
    void onMarkerPortError(QSerialPort::SerialPortError error);

signals:
    void portToggled(bool open);
    void deviceChanged(QIODevice* device);
    /// Emitted when the auxiliary marker port is opened or closed
    void markerPortToggled(bool open);
};

#endif // PORTCONTROL_H
