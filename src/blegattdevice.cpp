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

#include "blegattdevice.h"

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QDebug>
#include <QLowEnergyController>
#include <QLowEnergyDescriptor>
#include <QLowEnergyService>
#include <cstring>

BleGattDevice::BleGattDevice(QObject* parent)
    : QIODevice(parent),
      discoveryAgent(new QBluetoothDeviceDiscoveryAgent(this)),
      controller(nullptr),
      service(nullptr),
      _serviceUuid(QBluetoothUuid("{6E400001-B5A3-F393-E0A9-E50E24DCCA9E}")),
      _notifyUuid(QBluetoothUuid("{6E400003-B5A3-F393-E0A9-E50E24DCCA9E}")),
      _writeUuid(QBluetoothUuid("{6E400002-B5A3-F393-E0A9-E50E24DCCA9E}")),
      ready(false),
      scanning(false)
{
    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            [this](const QBluetoothDeviceInfo& info)
            {
                for (int i = 0; i < discovered.size(); i++)
                {
                    if (discovered[i].address() == info.address() &&
                        discovered[i].deviceUuid() == info.deviceUuid())
                    {
                        discovered[i] = info;
                        emit devicesChanged();
                        return;
                    }
                }

                discovered << info;
                emit devicesChanged();
            });

    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            [this]()
            {
                scanning = false;
                emit scanFinished();
            });

    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::canceled,
            [this]()
            {
                scanning = false;
                emit scanFinished();
            });

    connect(discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            [this](QBluetoothDeviceDiscoveryAgent::Error error)
            {
                Q_UNUSED(error);
                scanning = false;
                emit scanError(discoveryAgent->errorString());
            });
}

BleGattDevice::~BleGattDevice()
{
    disconnectFromDevice();
}

QList<QBluetoothDeviceInfo> BleGattDevice::scannedDevices() const
{
    return discovered;
}

bool BleGattDevice::isScanning() const
{
    return scanning;
}

void BleGattDevice::setServiceUuid(const QBluetoothUuid& uuid)
{
    _serviceUuid = uuid;
}

void BleGattDevice::setNotifyUuid(const QBluetoothUuid& uuid)
{
    _notifyUuid = uuid;
}

void BleGattDevice::setWriteUuid(const QBluetoothUuid& uuid)
{
    _writeUuid = uuid;
}

QBluetoothUuid BleGattDevice::serviceUuid() const
{
    return _serviceUuid;
}

QBluetoothUuid BleGattDevice::notifyUuid() const
{
    return _notifyUuid;
}

QBluetoothUuid BleGattDevice::writeUuid() const
{
    return _writeUuid;
}

void BleGattDevice::startScan()
{
    if (scanning)
    {
        return;
    }

    auto methods = discoveryAgent->supportedDiscoveryMethods();
    if (!methods.testFlag(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod))
    {
        emit scanError(tr("Low energy scan is not supported on this system."));
        return;
    }

    discovered.clear();
    emit devicesChanged();

    scanning = true;
    discoveryAgent->setLowEnergyDiscoveryTimeout(8000);
    discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    if (!discoveryAgent->isActive())
    {
        scanning = false;
        emit scanError(tr("Failed to start BLE scan."));
    }
}

void BleGattDevice::stopScan()
{
    if (!scanning)
    {
        return;
    }

    discoveryAgent->stop();
}

bool BleGattDevice::connectToDevice(const QBluetoothDeviceInfo& info)
{
    disconnectFromDevice();
    activeDevice = info;

    if (!activeDevice.isValid())
    {
        emit errorOccurred(tr("Invalid BLE device."));
        return false;
    }
    if (!activeDevice.coreConfigurations().testFlag(QBluetoothDeviceInfo::LowEnergyCoreConfiguration))
    {
        emit errorOccurred(tr("Selected device does not advertise BLE GATT."));
        return false;
    }

    controller = QLowEnergyController::createCentral(activeDevice, this);

    connect(controller, &QLowEnergyController::connected, this,
            [this]()
            {
                controller->discoverServices();
            });

    connect(controller, &QLowEnergyController::disconnected, this,
            [this]()
            {
                clearConnection();
                setConnected(false);
            });

    connect(controller, &QLowEnergyController::serviceDiscovered, this,
            [this](const QBluetoothUuid& serviceUuid)
            {
                if (serviceUuid == _serviceUuid)
                {
                    qDebug() << "BLE service discovered:" << serviceUuid;
                }
            });

    connect(controller, &QLowEnergyController::discoveryFinished, this,
            [this]()
            {
                if (!controller)
                {
                    return;
                }

                auto srv = controller->createServiceObject(_serviceUuid, this);
                if (!srv)
                {
                    emit errorOccurred(tr("BLE service not found: %1").arg(_serviceUuid.toString()));
                    controller->disconnectFromDevice();
                    return;
                }
                initService(srv);
                srv->discoverDetails();
            });

    connect(controller, &QLowEnergyController::errorOccurred, this,
            [this](QLowEnergyController::Error error)
            {
                Q_UNUSED(error);
                emit errorOccurred(controller->errorString());
                clearConnection();
                setConnected(false);
            });

    controller->connectToDevice();
    return true;
}

void BleGattDevice::disconnectFromDevice()
{
    if (controller)
    {
        controller->disconnectFromDevice();
    }
    clearConnection();
    setConnected(false);
}

qint64 BleGattDevice::readData(char* data, qint64 maxSize)
{
    if (maxSize <= 0 || readBuffer.isEmpty())
    {
        return 0;
    }

    qint64 bytesToRead = qMin(maxSize, (qint64)readBuffer.size());
    memcpy(data, readBuffer.constData(), (size_t)bytesToRead);
    readBuffer.remove(0, (int)bytesToRead);
    return bytesToRead;
}

qint64 BleGattDevice::writeData(const char* data, qint64 maxSize)
{
    if (!ready || !service || !writeChar.isValid() || maxSize <= 0)
    {
        return -1;
    }

    QByteArray payload(data, (int)maxSize);
    QLowEnergyService::WriteMode mode = QLowEnergyService::WriteWithoutResponse;
    if (writeChar.properties().testFlag(QLowEnergyCharacteristic::Write))
    {
        mode = QLowEnergyService::WriteWithResponse;
    }
    service->writeCharacteristic(writeChar, payload, mode);
    return maxSize;
}

void BleGattDevice::clearConnection()
{
    ready = false;
    if (QIODevice::isOpen())
    {
        close();
    }
    readBuffer.clear();

    if (service)
    {
        service->disconnect(this);
        service->deleteLater();
        service = nullptr;
    }

    notifyChar = QLowEnergyCharacteristic();
    writeChar = QLowEnergyCharacteristic();

    if (controller)
    {
        controller->disconnect(this);
        controller->deleteLater();
        controller = nullptr;
    }
}

void BleGattDevice::setConnected(bool connected)
{
    emit connectedChanged(connected);
}

void BleGattDevice::initService(QLowEnergyService* srv)
{
    service = srv;

    connect(service, &QLowEnergyService::stateChanged, this,
            [this](QLowEnergyService::ServiceState state)
            {
                if (state != QLowEnergyService::RemoteServiceDiscovered)
                {
                    return;
                }

                notifyChar = service->characteristic(_notifyUuid);
                writeChar = service->characteristic(_writeUuid);

                if (!notifyChar.isValid())
                {
                    emit errorOccurred(tr("Notify characteristic not found: %1").arg(_notifyUuid.toString()));
                    controller->disconnectFromDevice();
                    return;
                }
                if (!writeChar.isValid())
                {
                    emit errorOccurred(tr("Write characteristic not found: %1").arg(_writeUuid.toString()));
                    controller->disconnectFromDevice();
                    return;
                }

                auto cccd = notifyChar.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
                if (!cccd.isValid())
                {
                    emit errorOccurred(tr("Notify CCCD descriptor not found."));
                    controller->disconnectFromDevice();
                    return;
                }

                service->writeDescriptor(cccd, QByteArray::fromHex("0100"));
                open(QIODevice::ReadWrite | QIODevice::Unbuffered);
                ready = true;
                setConnected(true);
            });

    connect(service, &QLowEnergyService::characteristicChanged, this,
            [this](const QLowEnergyCharacteristic& characteristic, const QByteArray& value)
            {
                if (!ready)
                {
                    return;
                }

                if (characteristic.uuid() != _notifyUuid)
                {
                    return;
                }

                readBuffer.append(value);
                emit readyRead();
            });

    connect(service, &QLowEnergyService::errorOccurred, this,
            [this](QLowEnergyService::ServiceError error)
            {
                emit errorOccurred(tr("BLE service error: %1").arg((int)error));
                controller->disconnectFromDevice();
            });
}
