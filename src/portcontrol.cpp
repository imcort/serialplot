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

#include "portcontrol.h"
#include "ui_portcontrol.h"

#include <QFormLayout>
#include <QBluetoothAddress>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QVBoxLayout>
#include <QtDebug>

#include "setting_defines.h"

#define TBPORTLIST_MINWIDTH (200)

static const char* DEFAULT_BLE_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* DEFAULT_BLE_NOTIFY_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* DEFAULT_BLE_WRITE_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";

// setting mappings
const QMap<QSerialPort::Parity, QString> paritySettingMap({
        {QSerialPort::NoParity, "none"},
        {QSerialPort::OddParity, "odd"},
        {QSerialPort::EvenParity, "even"},
    });

PortControl::PortControl(QSerialPort* serialPort,
                         BleGattDevice* bleDevice,
                         QWidget* parent) :
    QWidget(parent),
    ui(new Ui::PortControl),
    portToolBar("Port Toolbar"),
    openAction("Open", this),
    loadPortListAction("↺", this),
    cbTransport(nullptr),
    cbBleDevice(nullptr),
    gbBle(nullptr),
    lbBleStatus(nullptr),
    leBleServiceUuid(nullptr),
    leBleNotifyUuid(nullptr),
    leBleWriteUuid(nullptr),
    scanBleAction("Scan BLE", this)
{
    ui->setupUi(this);

    this->serialPort = serialPort;
    this->bleDevice = bleDevice;
    transportMode = TransportMode::Serial;

    connect(this->serialPort, &QSerialPort::errorOccurred,
            this, &PortControl::onPortError);
    connect(this->bleDevice, &BleGattDevice::connectedChanged,
            this, &PortControl::onBleConnectedChanged);
    connect(this->bleDevice, &BleGattDevice::devicesChanged,
            this, &PortControl::refreshBleDeviceList);
    connect(this->bleDevice, &BleGattDevice::scanFinished,
            this, &PortControl::onBleScanFinished);
    connect(this->bleDevice, &BleGattDevice::scanError,
            [this](QString msg)
            {
                if (lbBleStatus != nullptr)
                {
                    lbBleStatus->setText(tr("Scan failed: %1").arg(msg));
                }
                qWarning() << "BLE scan error:" << msg;
            });
    connect(this->bleDevice, &BleGattDevice::errorOccurred,
            [this](QString msg)
            {
                if (lbBleStatus != nullptr)
                {
                    lbBleStatus->setText(tr("BLE error: %1").arg(msg));
                }
                qWarning() << "BLE error:" << msg;
            });

    // setup actions
    openAction.setCheckable(true);
    openAction.setShortcut(QKeySequence("Ctrl+O"));
    openAction.setToolTip("Open Port");
    QObject::connect(&openAction, &QAction::triggered,
                     this, &PortControl::openActionTriggered);

    loadPortListAction.setToolTip("Reload list");
    QObject::connect(&loadPortListAction, &QAction::triggered,
                     [this](bool checked)
                     {
                         Q_UNUSED(checked);
                         if (isSerialMode())
                         {
                             loadPortList();
                         }
                         else
                         {
                             scanBleDevices();
                         }
                     });

    scanBleAction.setToolTip("Scan BLE devices");
    connect(&scanBleAction, &QAction::triggered,
            this, &PortControl::scanBleDevices);

    // setup toolbar
    portToolBar.addWidget(&tbPortList);
    portToolBar.addAction(&loadPortListAction);
    portToolBar.addAction(&openAction);

    // setup port selection widgets
    tbPortList.setMinimumWidth(TBPORTLIST_MINWIDTH);
    tbPortList.setModel(&portList);
    ui->cbPortList->setModel(&portList);
    QObject::connect(ui->cbPortList, &QComboBox::activated,
                     this, &PortControl::onCbPortListActivated);
    QObject::connect(&tbPortList, &QComboBox::activated,
                     this, &PortControl::onTbPortListActivated);
    QObject::connect(ui->cbPortList, &QComboBox::textActivated,
                     this, &PortControl::selectListedPort);
    QObject::connect(&tbPortList, &QComboBox::textActivated,
                     this, &PortControl::selectListedPort);

    // setup buttons
    ui->pbOpenPort->setDefaultAction(&openAction);
    ui->pbReloadPorts->setDefaultAction(&loadPortListAction);

    // setup baud rate selection widget
    QObject::connect(ui->cbBaudRate, &QComboBox::textActivated,
                     this, &PortControl::_selectBaudRate);

    // setup parity selection buttons
    parityButtons.addButton(ui->rbNoParity, (int) QSerialPort::NoParity);
    parityButtons.addButton(ui->rbEvenParity, (int) QSerialPort::EvenParity);
    parityButtons.addButton(ui->rbOddParity, (int) QSerialPort::OddParity);
    QObject::connect(&parityButtons, &QButtonGroup::idClicked,
                     this, &PortControl::selectParity);

    // setup data bits selection buttons
    dataBitsButtons.addButton(ui->rb8Bits, (int) QSerialPort::Data8);
    dataBitsButtons.addButton(ui->rb7Bits, (int) QSerialPort::Data7);
    dataBitsButtons.addButton(ui->rb6Bits, (int) QSerialPort::Data6);
    dataBitsButtons.addButton(ui->rb5Bits, (int) QSerialPort::Data5);
    QObject::connect(&dataBitsButtons, &QButtonGroup::idClicked,
                     this, &PortControl::selectDataBits);

    // setup stop bits selection buttons
    stopBitsButtons.addButton(ui->rb1StopBit, (int) QSerialPort::OneStop);
    stopBitsButtons.addButton(ui->rb2StopBit, (int) QSerialPort::TwoStop);
    QObject::connect(&stopBitsButtons, &QButtonGroup::idClicked,
                     this, &PortControl::selectStopBits);

    // setup flow control selection buttons
    flowControlButtons.addButton(ui->rbNoFlowControl, (int) QSerialPort::NoFlowControl);
    flowControlButtons.addButton(ui->rbHardwareControl, (int) QSerialPort::HardwareControl);
    flowControlButtons.addButton(ui->rbSoftwareControl, (int) QSerialPort::SoftwareControl);
    QObject::connect(&flowControlButtons, &QButtonGroup::idClicked,
                     this, &PortControl::selectFlowControl);

    // initialize signal leds
    ui->ledDTR->setOn(true);
    ui->ledRTS->setOn(true);

    // connect output signals
    connect(ui->pbDTR, &QPushButton::clicked, [this]()
            {
                // toggle DTR
                ui->ledDTR->toggle();
                if (this->serialPort->isOpen())
                {
                    this->serialPort->setDataTerminalReady(ui->ledDTR->isOn());
                }
            });

    connect(ui->pbRTS, &QPushButton::clicked, [this]()
            {
                // toggle RTS
                ui->ledRTS->toggle();
                if (this->serialPort->isOpen())
                {
                    this->serialPort->setRequestToSend(ui->ledRTS->isOn());
                }
            });

    // setup pin update leds
    ui->ledDCD->setColor(Qt::yellow);
    ui->ledDSR->setColor(Qt::yellow);
    ui->ledRI->setColor(Qt::yellow);
    ui->ledCTS->setColor(Qt::yellow);

    pinUpdateTimer.setInterval(1000); // ms
    connect(&pinUpdateTimer, &QTimer::timeout, this, &PortControl::updatePinLeds);

    loadPortList();
    loadBaudRateList();
    ui->cbBaudRate->setCurrentIndex(ui->cbBaudRate->findText("9600"));

    // add transport and BLE widgets
    auto serialGrid = ui->gridLayout;
    auto transportLabel = new QLabel(tr("Transport:"), this);
    cbTransport = new QComboBox(this);
    cbTransport->addItem(tr("Serial"));
    cbTransport->addItem(tr("BLE"));
    connect(cbTransport, &QComboBox::currentIndexChanged,
            this, &PortControl::onTransportChanged);
    serialGrid->addWidget(transportLabel, 2, 0);
    serialGrid->addWidget(cbTransport, 2, 1);

    gbBle = new QGroupBox(tr("BLE (GATT)"), this);
    auto bleLayout = new QVBoxLayout(gbBle);
    auto deviceRow = new QHBoxLayout();
    cbBleDevice = new QComboBox(gbBle);
    cbBleDevice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto pbScan = new QPushButton(tr("Scan"), gbBle);
    connect(pbScan, &QPushButton::clicked,
            this, &PortControl::scanBleDevices);
    deviceRow->addWidget(cbBleDevice, 1);
    deviceRow->addWidget(pbScan);
    bleLayout->addLayout(deviceRow);

    auto form = new QFormLayout();
    leBleServiceUuid = new QLineEdit(DEFAULT_BLE_SERVICE_UUID, gbBle);
    leBleNotifyUuid = new QLineEdit(DEFAULT_BLE_NOTIFY_UUID, gbBle);
    leBleWriteUuid = new QLineEdit(DEFAULT_BLE_WRITE_UUID, gbBle);
    leBleServiceUuid->setPlaceholderText(DEFAULT_BLE_SERVICE_UUID);
    leBleNotifyUuid->setPlaceholderText(DEFAULT_BLE_NOTIFY_UUID);
    leBleWriteUuid->setPlaceholderText(DEFAULT_BLE_WRITE_UUID);
    form->addRow(tr("Service UUID:"), leBleServiceUuid);
    form->addRow(tr("Notify UUID:"), leBleNotifyUuid);
    form->addRow(tr("Write UUID:"), leBleWriteUuid);
    bleLayout->addLayout(form);

    lbBleStatus = new QLabel(tr("Idle"), gbBle);
    bleLayout->addWidget(lbBleStatus);

    static_cast<QVBoxLayout*>(ui->verticalLayout_3)->addWidget(gbBle);

    applyModeUi();
    emit deviceChanged(activeDevice());
}

PortControl::~PortControl()
{
    delete ui;
}

void PortControl::loadPortList()
{
    QString currentSelection = ui->cbPortList->currentData(PortNameRole).toString();
    portList.loadPortList();
    int index = portList.indexOf(currentSelection);
    if (index >= 0)
    {
        ui->cbPortList->setCurrentIndex(index);
        tbPortList.setCurrentIndex(index);
    }

    if (portList.rowCount() == 0)
    {
        ui->cbPortList->lineEdit()->setPlaceholderText(tr("No port found - enter name"));
    }
    else
    {
        ui->cbPortList->lineEdit()->setPlaceholderText(tr("Select port or enter name"));
    }
}

void PortControl::loadBaudRateList()
{
    ui->cbBaudRate->clear();

    for (auto baudRate : QSerialPortInfo::standardBaudRates())
    {
        ui->cbBaudRate->addItem(QString::number(baudRate));
    }
}

void PortControl::_selectBaudRate(QString baudRate)
{
    if (!isSerialMode())
    {
        return;
    }

    if (serialPort->isOpen())
    {
        if (!serialPort->setBaudRate(baudRate.toInt()))
        {
            qCritical() << "Can't set baud rate!";
        }
    }
}

void PortControl::selectParity(int parity)
{
    if (isSerialMode() && serialPort->isOpen())
    {
        if(!serialPort->setParity((QSerialPort::Parity) parity))
        {
            qCritical() << "Can't set parity option!";
        }
    }
}

void PortControl::selectDataBits(int dataBits)
{
    if (isSerialMode() && serialPort->isOpen())
    {
        if(!serialPort->setDataBits((QSerialPort::DataBits) dataBits))
        {
            qCritical() << "Can't set numer of data bits!";
        }
    }
}

void PortControl::selectStopBits(int stopBits)
{
    if (isSerialMode() && serialPort->isOpen())
    {
        if(!serialPort->setStopBits((QSerialPort::StopBits) stopBits))
        {
            qCritical() << "Can't set number of stop bits!";
        }
    }
}

void PortControl::selectFlowControl(int flowControl)
{
    if (isSerialMode() && serialPort->isOpen())
    {
        if(!serialPort->setFlowControl((QSerialPort::FlowControl) flowControl))
        {
            qCritical() << "Can't set flow control option!";
        }
    }
}

void PortControl::togglePort()
{
    if (isSerialMode())
    {
        if (serialPort->isOpen())
        {
            pinUpdateTimer.stop();
            serialPort->close();
            qDebug() << "Closed port:" << serialPort->portName();
            emit portToggled(false);
        }
        else
        {
            QString portName;
            QString portText = ui->cbPortList->currentText().trimmed();

            if (portText.isEmpty())
            {
                qWarning() << "Select or enter a port name!";
                return;
            }

            int portIndex = portList.indexOf(portText);
            if (portIndex < 0) // not in list, add to model and update the selections
            {
                portList.appendRow(new PortListItem(portText));
                ui->cbPortList->setCurrentIndex(portList.rowCount()-1);
                tbPortList.setCurrentIndex(portList.rowCount()-1);
                portName = portText;
            }
            else
            {
                portName = static_cast<PortListItem*>(portList.item(portIndex))->portName();
            }

            serialPort->setPortName(portName);

            if (serialPort->open(QIODevice::ReadWrite))
            {
                _selectBaudRate(ui->cbBaudRate->currentText());
                selectParity((QSerialPort::Parity) parityButtons.checkedId());
                selectDataBits((QSerialPort::DataBits) dataBitsButtons.checkedId());
                selectStopBits((QSerialPort::StopBits) stopBitsButtons.checkedId());
                selectFlowControl((QSerialPort::FlowControl) flowControlButtons.checkedId());

                serialPort->setDataTerminalReady(ui->ledDTR->isOn());
                serialPort->setRequestToSend(ui->ledRTS->isOn());

                updatePinLeds();
                pinUpdateTimer.start();

                qDebug() << "Opened port:" << serialPort->portName();
                emit portToggled(true);
            }
        }
        openAction.setChecked(serialPort->isOpen());
        return;
    }

    if (bleDevice->isOpen())
    {
        disconnectBle();
    }
    else
    {
        connectBle();
    }
}

void PortControl::selectListedPort(QString portName)
{
    if (!isSerialMode())
    {
        return;
    }

    // portName may be coming from combobox
    portName = portName.split(" ")[0];

    QSerialPortInfo portInfo(portName);
    if (portInfo.isNull())
    {
        qWarning() << "Device doesn't exist:" << portName;
    }

    if (portName != serialPort->portName() && serialPort->isOpen())
    {
        togglePort();
        togglePort();
    }
}

QString PortControl::selectedPortName()
{
    QString portText = ui->cbPortList->currentText();
    int portIndex = portList.indexOf(portText);
    if (portIndex < 0)
    {
        return portText;
    }
    else
    {
        return static_cast<PortListItem*>(portList.item(portIndex))->portName();
    }
}

QToolBar* PortControl::toolBar()
{
    return &portToolBar;
}

QIODevice* PortControl::activeDevice()
{
    return isSerialMode() ? static_cast<QIODevice*>(serialPort)
                          : static_cast<QIODevice*>(bleDevice);
}

void PortControl::openActionTriggered(bool checked)
{
    Q_UNUSED(checked);
    togglePort();
}

void PortControl::onCbPortListActivated(int index)
{
    tbPortList.setCurrentIndex(index);
}

void PortControl::onTbPortListActivated(int index)
{
    ui->cbPortList->setCurrentIndex(index);
}

void PortControl::onPortError(QSerialPort::SerialPortError error)
{
#ifdef Q_OS_UNIX
    auto isPtsInvalidArgErr = [this] () -> bool {
        return serialPort->portName().contains("pts/") && serialPort->errorString().contains("Invalid argument");
    };
#endif

    switch(error)
    {
        case QSerialPort::NoError :
            break;
        case QSerialPort::ResourceError :
            qWarning() << "Port error: resource unavaliable; most likely device removed.";
            if (serialPort->isOpen())
            {
                qWarning() << "Closing port on resource error: " << serialPort->portName();
                togglePort();
            }
            loadPortList();
            break;
        case QSerialPort::DeviceNotFoundError:
            qCritical() << "Device doesn't exist: " << serialPort->portName();
            break;
        case QSerialPort::PermissionError:
            qCritical() << "Permission denied. Either you don't have \
required privileges or device is already opened by another process.";
            break;
        case QSerialPort::OpenError:
            qWarning() << "Device is already opened!";
            break;
        case QSerialPort::NotOpenError:
            qCritical() << "Device is not open!";
            break;
        case QSerialPort::WriteError:
            qCritical() << "An error occurred while writing data.";
            break;
        case QSerialPort::ReadError:
            qCritical() << "An error occurred while reading data.";
            break;
        case QSerialPort::UnsupportedOperationError:
#ifdef Q_OS_UNIX
            if (isPtsInvalidArgErr())
                break;
#endif
            qCritical() << "Operation is not supported.";
            break;
        case QSerialPort::TimeoutError:
            qCritical() << "A timeout error occurred.";
            break;
        case QSerialPort::UnknownError:
#ifdef Q_OS_UNIX
            if (isPtsInvalidArgErr())
                break;
#endif
            qCritical() << "Unknown error! Error: " << serialPort->errorString();
            break;
        default:
            qCritical() << "Unhandled port error: " << error;
            break;
    }
}

void PortControl::updatePinLeds(void)
{
    if (!isSerialMode())
    {
        ui->ledDCD->setOn(false);
        ui->ledDSR->setOn(false);
        ui->ledRI->setOn(false);
        ui->ledCTS->setOn(false);
        return;
    }

    auto pins = serialPort->pinoutSignals();
    ui->ledDCD->setOn(pins & QSerialPort::DataCarrierDetectSignal);
    ui->ledDSR->setOn(pins & QSerialPort::DataSetReadySignal);
    ui->ledRI->setOn(pins & QSerialPort::RingIndicatorSignal);
    ui->ledCTS->setOn(pins & QSerialPort::ClearToSendSignal);
}

QString PortControl::currentParityText()
{
    return paritySettingMap.value((QSerialPort::Parity) parityButtons.checkedId());
}

QString PortControl::currentFlowControlText()
{
    if (flowControlButtons.checkedId() == QSerialPort::HardwareControl)
    {
        return "hardware";
    }
    else if (flowControlButtons.checkedId() == QSerialPort::SoftwareControl)
    {
        return "software";
    }
    else
    {
        return "none";
    }
}

void PortControl::selectPort(QString portName)
{
    transportMode = TransportMode::Serial;
    cbTransport->setCurrentIndex((int)TransportMode::Serial);
    applyModeUi();

    int portIndex = portList.indexOfName(portName);
    if (portIndex < 0)
    {
        portList.appendRow(new PortListItem(portName));
        portIndex = portList.rowCount()-1;
    }

    ui->cbPortList->setCurrentIndex(portIndex);
    tbPortList.setCurrentIndex(portIndex);

    selectListedPort(portName);
}

void PortControl::selectBaudrate(QString baudRate)
{
    int baudRateIndex = ui->cbBaudRate->findText(baudRate);
    if (baudRateIndex < 0)
    {
        ui->cbBaudRate->setCurrentText(baudRate);
    }
    else
    {
        ui->cbBaudRate->setCurrentIndex(baudRateIndex);
    }
    _selectBaudRate(baudRate);
}

void PortControl::openPort()
{
    if (!activeDevice()->isOpen())
    {
        openAction.trigger();
    }
}

unsigned PortControl::maxBitRate() const
{
    if (!isSerialMode())
    {
        return 0;
    }

    float baud = serialPort->baudRate();
    float dataBits = serialPort->dataBits();
    float parityBits = serialPort->parity() == QSerialPort::NoParity ? 0 : 1;

    float stopBits;
    if (serialPort->stopBits() == QSerialPort::OneAndHalfStop)
    {
        stopBits = 1.5;
    }
    else
    {
        stopBits = serialPort->stopBits();
    }

    float frame_size = 1 + dataBits + parityBits + stopBits;
    return float(baud) / frame_size;
}

void PortControl::saveSettings(QSettings* settings)
{
    settings->beginGroup(SettingGroup_Port);
    settings->setValue(SG_Port_SelectedPort, selectedPortName());
    settings->setValue(SG_Port_BaudRate, ui->cbBaudRate->currentText());
    settings->setValue(SG_Port_Parity, currentParityText());
    settings->setValue(SG_Port_DataBits, dataBitsButtons.checkedId());
    settings->setValue(SG_Port_StopBits, stopBitsButtons.checkedId());
    settings->setValue(SG_Port_FlowControl, currentFlowControlText());
    settings->setValue(SG_Port_TransportMode, (int)transportMode);
    settings->setValue(SG_Port_BleServiceUuid, leBleServiceUuid->text().trimmed());
    settings->setValue(SG_Port_BleNotifyUuid, leBleNotifyUuid->text().trimmed());
    settings->setValue(SG_Port_BleWriteUuid, leBleWriteUuid->text().trimmed());
    settings->endGroup();
}

void PortControl::loadSettings(QSettings* settings)
{
    if (serialPort->isOpen() || bleDevice->isOpen()) togglePort();

    settings->beginGroup(SettingGroup_Port);

    QString portName = settings->value(SG_Port_SelectedPort, QString()).toString();
    if (!portName.isEmpty())
    {
        int index = portList.indexOfName(portName);
        if (index > -1) ui->cbPortList->setCurrentIndex(index);
    }

    QString baudSetting = settings->value(SG_Port_BaudRate, ui->cbBaudRate->currentText()).toString();
    int baudIndex = ui->cbBaudRate->findText(baudSetting);
    if (baudIndex > -1)
    {
        ui->cbBaudRate->setCurrentIndex(baudIndex);
    }
    else
    {
        bool ok;
        int r = baudSetting.toUInt(&ok);
        if (ok && r > 0)
        {
            ui->cbBaudRate->insertItem(0, baudSetting);
            ui->cbBaudRate->setCurrentIndex(0);
        }
        else
        {
            qCritical() << "Invalid baud setting: " << baudSetting;
        }
    }

    QString parityText = settings->value(SG_Port_Parity, currentParityText()).toString();
    QSerialPort::Parity paritySetting = paritySettingMap.key(
        parityText, (QSerialPort::Parity) parityButtons.checkedId());
    parityButtons.button(paritySetting)->setChecked(true);

    int dataBits = settings->value(SG_Port_DataBits, dataBitsButtons.checkedId()).toInt();
    if (dataBits >=5 && dataBits <= 8)
    {
        dataBitsButtons.button((QSerialPort::DataBits) dataBits)->setChecked(true);
    }

    int stopBits = settings->value(SG_Port_StopBits, stopBitsButtons.checkedId()).toInt();
    if (stopBits == QSerialPort::OneStop)
    {
        ui->rb1StopBit->setChecked(true);
    }
    else if (stopBits == QSerialPort::TwoStop)
    {
        ui->rb2StopBit->setChecked(true);
    }

    QString flowControlSetting = settings->value(SG_Port_FlowControl, currentFlowControlText()).toString();
    if (flowControlSetting == "hardware")
    {
        ui->rbHardwareControl->setChecked(true);
    }
    else if (flowControlSetting == "software")
    {
        ui->rbSoftwareControl->setChecked(true);
    }
    else
    {
        ui->rbNoFlowControl->setChecked(true);
    }

    leBleServiceUuid->setText(settings->value(SG_Port_BleServiceUuid, DEFAULT_BLE_SERVICE_UUID).toString());
    leBleNotifyUuid->setText(settings->value(SG_Port_BleNotifyUuid, DEFAULT_BLE_NOTIFY_UUID).toString());
    leBleWriteUuid->setText(settings->value(SG_Port_BleWriteUuid, DEFAULT_BLE_WRITE_UUID).toString());

    int modeValue = settings->value(SG_Port_TransportMode, (int)TransportMode::Serial).toInt();
    if (modeValue != (int)TransportMode::BLE)
    {
        modeValue = (int)TransportMode::Serial;
    }
    cbTransport->setCurrentIndex(modeValue);
    onTransportChanged(modeValue);

    settings->endGroup();
}

QBluetoothUuid PortControl::parseUuid(const QString& text)
{
    QString t = text.trimmed();
    QBluetoothUuid uuid(t);
    if (uuid.isNull())
    {
        qWarning() << "Invalid UUID:" << t;
    }
    return uuid;
}

void PortControl::applyModeUi()
{
    bool serial = isSerialMode();

    // Serial area (mutually exclusive with BLE area)
    ui->label->setVisible(serial);
    ui->label_2->setVisible(serial);
    ui->cbPortList->setVisible(serial);
    ui->cbBaudRate->setVisible(serial);
    ui->pbReloadPorts->setVisible(serial);

    ui->frame->setVisible(serial);
    ui->frame_2->setVisible(serial);
    ui->frame_3->setVisible(serial);
    ui->frame_4->setVisible(serial);
    ui->pbDTR->setVisible(serial);
    ui->pbRTS->setVisible(serial);
    ui->ledDTR->setVisible(serial);
    ui->ledRTS->setVisible(serial);
    ui->ledDCD->setVisible(serial);
    ui->ledDSR->setVisible(serial);
    ui->ledRI->setVisible(serial);
    ui->ledCTS->setVisible(serial);
    ui->labDCD->setVisible(serial);
    ui->labDSR->setVisible(serial);
    ui->labRI->setVisible(serial);
    ui->labCTS->setVisible(serial);

    if (gbBle != nullptr)
    {
        gbBle->setVisible(!serial);
    }

    cbBleDevice->setEnabled(!serial);
    leBleServiceUuid->setEnabled(!serial);
    leBleNotifyUuid->setEnabled(!serial);
    leBleWriteUuid->setEnabled(!serial);
    scanBleAction.setEnabled(!serial);

    loadPortListAction.setToolTip(serial ? "Reload port list" : "Scan BLE devices");
    openAction.setText(serial ? tr("Open") : tr("Connect"));
    openAction.setChecked(activeDevice()->isOpen());
}

bool PortControl::isSerialMode() const
{
    return transportMode == TransportMode::Serial;
}

void PortControl::refreshBleDeviceList()
{
    scannedBleDevices = bleDevice->scannedDevices();
    QString selectedAddress = cbBleDevice->currentData().toString();

    cbBleDevice->clear();
    for (const auto& info : scannedBleDevices)
    {
        QString key = info.address().toString();
        if (key.isEmpty())
        {
            key = info.deviceUuid().toString();
        }
        cbBleDevice->addItem(bleDeviceDisplayName(info), key);
    }

    int idx = cbBleDevice->findData(selectedAddress);
    if (idx >= 0)
    {
        cbBleDevice->setCurrentIndex(idx);
    }
}

void PortControl::connectBle()
{
    if (cbBleDevice->currentIndex() < 0 || cbBleDevice->currentIndex() >= scannedBleDevices.size())
    {
        qWarning() << "Select a BLE device first.";
        return;
    }

    auto serviceUuid = parseUuid(leBleServiceUuid->text());
    auto notifyUuid = parseUuid(leBleNotifyUuid->text());
    auto writeUuid = parseUuid(leBleWriteUuid->text());
    if (serviceUuid.isNull() || notifyUuid.isNull() || writeUuid.isNull())
    {
        qWarning() << "Invalid BLE UUID settings.";
        return;
    }

    bleDevice->setServiceUuid(serviceUuid);
    bleDevice->setNotifyUuid(notifyUuid);
    bleDevice->setWriteUuid(writeUuid);
    bleDevice->connectToDevice(scannedBleDevices[cbBleDevice->currentIndex()]);
}

void PortControl::disconnectBle()
{
    bleDevice->disconnectFromDevice();
}

QString PortControl::bleDeviceDisplayName(const QBluetoothDeviceInfo& info) const
{
    QString name = info.name().trimmed();
    if (name.isEmpty())
    {
        name = tr("(Unnamed)");
    }
    QString address = info.address().toString();
    if (address.isEmpty())
    {
        address = info.deviceUuid().toString();
    }
    return QString("%1 [%2]").arg(name, address);
}

void PortControl::onTransportChanged(int index)
{
    TransportMode newMode = index == (int)TransportMode::BLE ? TransportMode::BLE : TransportMode::Serial;
    if (newMode == transportMode)
    {
        applyModeUi();
        emit deviceChanged(activeDevice());
        return;
    }

    if (activeDevice()->isOpen())
    {
        togglePort();
    }

    transportMode = newMode;
    applyModeUi();

    if (!isSerialMode())
    {
        scanBleDevices();
    }

    emit deviceChanged(activeDevice());
}

void PortControl::scanBleDevices()
{
    if (isSerialMode())
    {
        return;
    }

    if (bleDevice->isScanning())
    {
        bleDevice->stopScan();
        if (lbBleStatus != nullptr)
        {
            lbBleStatus->setText(tr("Scan canceled."));
        }
        return;
    }

    cbBleDevice->clear();
    scannedBleDevices.clear();
    if (lbBleStatus != nullptr)
    {
        lbBleStatus->setText(tr("Scanning BLE devices..."));
    }
    bleDevice->startScan();
}

void PortControl::onBleConnectedChanged(bool connected)
{
    openAction.setChecked(connected);
    emit portToggled(connected);
}

void PortControl::onBleScanFinished()
{
    refreshBleDeviceList();
    if (lbBleStatus != nullptr)
    {
        lbBleStatus->setText(
            scannedBleDevices.isEmpty()
                ? tr("No BLE device found.")
                : tr("Found %1 BLE device(s).").arg(scannedBleDevices.size()));
    }
}
