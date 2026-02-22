/*
  Copyright © 2026 Hasan Yavuz Özderya

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

#ifndef BLEGATTDEVICE_H
#define BLEGATTDEVICE_H

#include <QIODevice>
#include <QList>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyCharacteristic>

class QBluetoothDeviceDiscoveryAgent;
class QLowEnergyController;
class QLowEnergyService;

class BleGattDevice : public QIODevice
{
    Q_OBJECT
public:
    explicit BleGattDevice(QObject* parent = 0);
    ~BleGattDevice();

    QList<QBluetoothDeviceInfo> scannedDevices() const;
    bool isScanning() const;

    void setServiceUuid(const QBluetoothUuid& uuid);
    void setNotifyUuid(const QBluetoothUuid& uuid);
    void setWriteUuid(const QBluetoothUuid& uuid);

    QBluetoothUuid serviceUuid() const;
    QBluetoothUuid notifyUuid() const;
    QBluetoothUuid writeUuid() const;

    bool connectToDevice(const QBluetoothDeviceInfo& info);
    void disconnectFromDevice();

    bool isSequential() const override { return true; }

public slots:
    void startScan();
    void stopScan();

signals:
    void scanFinished();
    void scanError(QString message);
    void devicesChanged();
    void connectedChanged(bool connected);
    void errorOccurred(QString message);

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    QBluetoothDeviceDiscoveryAgent* discoveryAgent;
    QList<QBluetoothDeviceInfo> discovered;

    QLowEnergyController* controller;
    QLowEnergyService* service;
    QBluetoothDeviceInfo activeDevice;
    QLowEnergyCharacteristic notifyChar;
    QLowEnergyCharacteristic writeChar;
    QBluetoothUuid _serviceUuid;
    QBluetoothUuid _notifyUuid;
    QBluetoothUuid _writeUuid;
    QByteArray readBuffer;

    bool ready;
    bool scanning;

    void clearConnection();
    void setConnected(bool connected);
    void initService(QLowEnergyService* srv);
};

#endif // BLEGATTDEVICE_H
