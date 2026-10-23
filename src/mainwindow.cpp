/*
  Copyright © 2025 Hasan Yavuz Özderya

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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QByteArray>
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMenu>
#include <QDesktopServices>
#include <QMap>
#include <QtDebug>
#include <QCommandLineParser>
#include <QFileInfo>
#include <qwt_plot.h>
#include <limits.h>
#include <cmath>
#include <iostream>
#include <cstdlib>

#include <plot.h>
#include <barplot.h>

#include "framebufferseries.h"
#include "defines.h"
#include "version.h"
#include "setting_defines.h"

#if defined(Q_OS_WIN) && defined(QT_STATIC)
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

// TODO: depends on tab insertion order, a better solution would be to use object names
const QMap<int, QString> panelSettingMap({
        {0, "Port"},
        {1, "DataFormat"},
        {2, "Plot"},
        {3, "Commands"},
        {4, "Record"},
        {5, "TextView"},
        {6, "Log"}
    });

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    aboutDialog(this),
    bleDevice(this),
    markerPort(&markerSerialPort),
    portControl(&serialPort, &bleDevice, &markerSerialPort),
    secondaryPlot(NULL),
    snapshotMan(this, &stream),
    commandPanel(&serialPort),
    dataFormatPanel(&serialPort),
    recordPanel(&stream),
    textView(&stream),
    updateCheckDialog(this),
    bpsLabel(&portControl, &dataFormatPanel, this)
{
    ui->setupUi(this);

    // configuration files can be dropped onto the window
    setAcceptDrops(true);

    plotMan = new PlotManager(ui->plotArea, &plotMenu, &stream);

    ui->tabWidget->insertTab(0, &portControl, "Port");
    ui->tabWidget->insertTab(1, &dataFormatPanel, "Data Format");
    ui->tabWidget->insertTab(2, &plotControlPanel, "Plot");
    ui->tabWidget->insertTab(3, &commandPanel, "Commands");
    ui->tabWidget->insertTab(4, &recordPanel, "Record");
    ui->tabWidget->insertTab(5, &textView, "Text View");
    ui->tabWidget->setCurrentIndex(0);
    auto tbPortControl = portControl.toolBar();
    addToolBar(tbPortControl);
    addToolBar(recordPanel.toolbar());

    ui->plotToolBar->addAction(snapshotMan.takeSnapshotAction());
    menuBar()->insertMenu(ui->menuHelp->menuAction(), snapshotMan.menu());
    menuBar()->insertMenu(ui->menuHelp->menuAction(), commandPanel.menu());

    connect(&commandPanel, &CommandPanel::focusRequested, [this]()
            {
                this->ui->tabWidget->setCurrentWidget(&commandPanel);
                this->ui->tabWidget->showTabs();
            });

    tbPortControl->setObjectName("tbPortControl");
    ui->plotToolBar->setObjectName("tbPlot");

    setupAboutDialog();

    // init view menu
    ui->menuBar->insertMenu(ui->menuSecondary->menuAction(), &plotMenu);
    plotMenu.addSeparator();
    QMenu* tbMenu = plotMenu.addMenu("Toolbars");
    tbMenu->addAction(ui->plotToolBar->toggleViewAction());
    tbMenu->addAction(portControl.toolBar()->toggleViewAction());

    // init secondary plot menu
    auto group = new QActionGroup(this);
    group->addAction(ui->actionVertical);
    group->addAction(ui->actionHorizontal);

    // init UI signals

    // Secondary plot menu signals
    connect(ui->actionBarPlot, &QAction::triggered,
            this, &MainWindow::showBarPlot);

    connect(ui->actionVertical, &QAction::triggered,
            [this](bool checked)
            {
                if (checked) ui->splitter->setOrientation(Qt::Vertical);
            });

    connect(ui->actionHorizontal, &QAction::triggered,
            [this](bool checked)
            {
                if (checked) ui->splitter->setOrientation(Qt::Horizontal);
            });

    // Help menu signals
    QObject::connect(ui->actionHelpAbout, &QAction::triggered,
              &aboutDialog, &QWidget::show);

    QObject::connect(ui->actionCheckUpdate, &QAction::triggered,
              &updateCheckDialog, &QWidget::show);

    QObject::connect(ui->actionReportBug, &QAction::triggered,
                     [](){QDesktopServices::openUrl(QUrl(BUG_REPORT_URL));});

    // File menu signals
    QObject::connect(ui->actionExportCsv, &QAction::triggered,
                     this, &MainWindow::onExportCsv);

    QObject::connect(ui->actionExportSvg, &QAction::triggered,
                     this, &MainWindow::onExportSvg);

    QObject::connect(ui->actionSaveSettings, &QAction::triggered,
                     this, &MainWindow::onSaveSettings);

    QObject::connect(ui->actionLoadSettings, &QAction::triggered,
                     this, &MainWindow::onLoadSettings);

    ui->actionQuit->setShortcutContext(Qt::ApplicationShortcut);

    QObject::connect(ui->actionQuit, &QAction::triggered,
                     this, &MainWindow::close);

    // port control signals
    QObject::connect(&portControl, &PortControl::portToggled,
                     this, &MainWindow::onPortToggled);
    QObject::connect(&portControl, &PortControl::deviceChanged,
                     &commandPanel, &CommandPanel::setDevice);
    QObject::connect(&portControl, &PortControl::deviceChanged,
                     &dataFormatPanel, &DataFormatPanel::setDevice);
    QObject::connect(&portControl, &PortControl::deviceChanged,
                     [this](QIODevice* device)
                     {
                         QObject::disconnect(&serialPort, &QIODevice::aboutToClose,
                                             &recordPanel, &RecordPanel::onPortClose);
                         QObject::disconnect(&bleDevice, &QIODevice::aboutToClose,
                                             &recordPanel, &RecordPanel::onPortClose);
                         QObject::disconnect(portControl.tcpDevice(), &QIODevice::aboutToClose,
                                             &recordPanel, &RecordPanel::onPortClose);
                         QObject::connect(device, &QIODevice::aboutToClose,
                                          &recordPanel, &RecordPanel::onPortClose);
                     });

    // marker input (auxiliary tagger) signals
    QObject::connect(&markerPort, &MarkerPort::markerReceived,
                     &stream, &Stream::addMarker);

    QObject::connect(&portControl, &PortControl::markerPortToggled,
                     &stream, &Stream::setMarkersEnabled);

    // plot control signals
    connect(&plotControlPanel, &PlotControlPanel::numOfSamplesChanged,
            this, &MainWindow::onNumOfSamplesChanged);

    connect(&plotControlPanel, &PlotControlPanel::numOfSamplesChanged,
            plotMan, &PlotManager::setNumOfSamples);

    connect(&plotControlPanel, &PlotControlPanel::yScaleChanged,
            plotMan, &PlotManager::setYAxis);

    connect(&plotControlPanel, &PlotControlPanel::xScaleChanged,
            &stream, &Stream::setXAxis);

    connect(&plotControlPanel, &PlotControlPanel::xScaleChanged,
            plotMan, &PlotManager::setXAxis);

    connect(&plotControlPanel, &PlotControlPanel::plotWidthChanged,
            plotMan, &PlotManager::setPlotWidth);

    connect(&plotControlPanel, &PlotControlPanel::lineThicknessChanged,
            plotMan, &PlotManager::setLineThickness);

    // plot toolbar signals
    QObject::connect(ui->actionClear, SIGNAL(triggered(bool)),
                     this, SLOT(clearPlot()));

    QObject::connect(snapshotMan.takeSnapshotAction(), &QAction::triggered,
                     plotMan, &PlotManager::flashSnapshotOverlay);

    QObject::connect(ui->actionPause, &QAction::triggered,
                     &stream, &Stream::pause);

    QObject::connect(ui->actionPause, &QAction::triggered,
                     [this](bool enabled)
                     {
                         if (enabled && !recordPanel.recordPaused())
                         {
                             dataFormatPanel.pause(true);
                         }
                         else
                         {
                             dataFormatPanel.pause(false);
                         }
                     });

    QObject::connect(&recordPanel, &RecordPanel::recordPausedChanged,
                     [this](bool enabled)
                     {
                         if (ui->actionPause->isChecked() && enabled)
                         {
                             dataFormatPanel.pause(false);
                         }
                     });

    connect(portControl.activeDevice(), &QIODevice::aboutToClose,
            &recordPanel, &RecordPanel::onPortClose);

    // init plot
    numOfSamples = plotControlPanel.numOfSamples();
    stream.setNumSamples(numOfSamples);
    plotControlPanel.setChannelInfoModel(stream.infoModel());

    // init scales
    stream.setXAxis(plotControlPanel.xAxisAsIndex(),
                    plotControlPanel.xMin(), plotControlPanel.xMax());

    plotMan->setYAxis(plotControlPanel.autoScale(),
                      plotControlPanel.yMin(), plotControlPanel.yMax());
    plotMan->setXAxis(plotControlPanel.xAxisAsIndex(),
                      plotControlPanel.xMin(), plotControlPanel.xMax());
    plotMan->setNumOfSamples(numOfSamples);
    plotMan->setPlotWidth(plotControlPanel.plotWidth());

    // init bps (bits per second) counter
    ui->statusBar->addPermanentWidget(&bpsLabel);

    // Init sps (sample per second) counter
    spsLabel.setText("0sps");
    spsLabel.setToolTip(tr("samples per second (per channel)"));
    ui->statusBar->addPermanentWidget(&spsLabel);
    connect(&sampleCounter, &SampleCounter::spsChanged,
            this, &MainWindow::onSpsChanged);

    bpsLabel.setMinimumWidth(70);
    bpsLabel.setAlignment(Qt::AlignRight);
    spsLabel.setMinimumWidth(70);
    spsLabel.setAlignment(Qt::AlignRight);

    // init demo
    QObject::connect(ui->actionDemoMode, &QAction::toggled,
                     this, &MainWindow::enableDemo);

    QObject::connect(ui->actionDemoMode, &QAction::toggled,
                     plotMan, &PlotManager::showDemoIndicator);

    // init stream connections
    connect(&dataFormatPanel, &DataFormatPanel::sourceChanged,
            this, &MainWindow::onSourceChanged);
    onSourceChanged(dataFormatPanel.activeSource());

    // load default settings
    QSettings settings(PROGRAM_NAME, PROGRAM_NAME);
    loadAllSettings(&settings);

    handleCommandLineOptions(*QApplication::instance());

    // ensure command panel has 1 command if none loaded
    if (!commandPanel.numOfCommands())
    {
        commandPanel.newCommandAction()->trigger();
    }

    // Important: This should be after newCommandAction is triggered
    // (above) we don't want user to be greeted with command panel on
    // the very first run.
    connect(commandPanel.newCommandAction(), &QAction::triggered, [this]()
            {
                this->ui->tabWidget->setCurrentWidget(&commandPanel);
                this->ui->tabWidget->showTabs();
            });
}

MainWindow::~MainWindow()
{
    if (serialPort.isOpen())
    {
        serialPort.close();
    }
    if (markerSerialPort.isOpen())
    {
        markerSerialPort.close();
    }
    if (portControl.tcpDevice()->isOpen())
    {
        portControl.tcpDevice()->close();
    }
    bleDevice.disconnectFromDevice();

    delete plotMan;

    delete ui;
    ui = NULL; // we check if ui is deleted in messageHandler
}

void MainWindow::closeEvent(QCloseEvent * event)
{
    // save snapshots
    if (!snapshotMan.isAllSaved())
    {
        auto clickedButton = QMessageBox::warning(
            this, "Closing SerialPlot",
            "There are un-saved snapshots. If you close you will loose the data.",
            QMessageBox::Discard, QMessageBox::Cancel);
        if (clickedButton == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }
    }

    // save settings
    QSettings settings(PROGRAM_NAME, PROGRAM_NAME);
    saveAllSettings(&settings);
    settings.sync();

    if (settings.status() != QSettings::NoError)
    {
        QString errorText;

        if (settings.status() == QSettings::AccessError)
        {
            QString file = settings.fileName();
            errorText = QString("Serialplot cannot save settings due to access error. \
This happens if you have run serialplot as root (with sudo for ex.) previously. \
Try fixing the permissions of file: %1, or just delete it.").arg(file);
        }
        else
        {
            errorText = QString("Serialplot cannot save settings due to unknown error: %1").\
                arg(settings.status());
        }

        auto button = QMessageBox::critical(
            NULL,
            "Failed to save settings!", errorText,
            QMessageBox::Cancel | QMessageBox::Ok);

        if (button == QMessageBox::Cancel)
        {
            event->ignore();
            return;
        }
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::setupAboutDialog()
{
    Ui_AboutDialog uiAboutDialog;
    uiAboutDialog.setupUi(&aboutDialog);

    QObject::connect(uiAboutDialog.pbAboutQt, &QPushButton::clicked,
                     [](){ QApplication::aboutQt();});

    QString aboutText = uiAboutDialog.lbAbout->text();
    aboutText.replace("$VERSION_STRING$", VERSION_STRING);
    aboutText.replace("$VERSION_REVISION$", VERSION_REVISION);
    uiAboutDialog.lbAbout->setText(aboutText);
}

void MainWindow::onPortToggled(bool open)
{
    // make sure demo mode is disabled
    if (open && isDemoRunning()) enableDemo(false);
    ui->actionDemoMode->setEnabled(!open);

    if (!open)
    {
        spsLabel.setText("0sps");
    }
}

void MainWindow::onSourceChanged(Source* source)
{
    source->connectSink(&stream);
    source->connectSink(&sampleCounter);
}

void MainWindow::clearPlot()
{
    stream.clear();
    plotMan->replot();
}

void MainWindow::onNumOfSamplesChanged(int value)
{
    numOfSamples = value;
    stream.setNumSamples(value);
    plotMan->replot();
}

void MainWindow::onSpsChanged(float sps)
{
    int precision = sps < 1. ? 3 : 0;
    spsLabel.setText(QString::number(sps, 'f', precision) + "sps");
}

bool MainWindow::isDemoRunning()
{
    return ui->actionDemoMode->isChecked();
}

void MainWindow::enableDemo(bool enabled)
{
    if (enabled)
    {
        if (!portControl.activeDevice()->isOpen())
        {
            dataFormatPanel.enableDemo(true);
        }
        else
        {
            ui->actionDemoMode->setChecked(false);
        }
    }
    else
    {
        dataFormatPanel.enableDemo(false);
        ui->actionDemoMode->setChecked(false);
    }
}

void MainWindow::showSecondary(QWidget* wid)
{
    if (secondaryPlot != NULL)
    {
        secondaryPlot->deleteLater();
    }

    secondaryPlot = wid;
    ui->splitter->addWidget(wid);
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 0);
}

void MainWindow::hideSecondary()
{
    if (secondaryPlot == NULL)
    {
        qFatal("Secondary plot doesn't exist!");
    }

    secondaryPlot->deleteLater();
    secondaryPlot = NULL;
}

void MainWindow::showBarPlot(bool show)
{
    if (show)
    {
        auto plot = new BarPlot(&stream, &plotMenu);
        plot->setYAxis(plotControlPanel.autoScale(),
                       plotControlPanel.yMin(),
                       plotControlPanel.yMax());
        connect(&plotControlPanel, &PlotControlPanel::yScaleChanged,
                plot, &BarPlot::setYAxis);
        showSecondary(plot);
    }
    else
    {
        hideSecondary();
    }
}

void MainWindow::onExportCsv()
{
    bool wasPaused = ui->actionPause->isChecked();
    ui->actionPause->setChecked(true); // pause plotting

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export CSV File"));

    if (fileName.isNull())  // user canceled export
    {
        ui->actionPause->setChecked(wasPaused);
    }
    else
    {
        Snapshot* snapshot = snapshotMan.makeSnapshot();
        snapshot->save(fileName);
        delete snapshot;
    }
}

void MainWindow::onExportSvg()
{
    bool wasPaused = ui->actionPause->isChecked();
    ui->actionPause->setChecked(true); // pause plotting

    QString fileName = QFileDialog::getSaveFileName(
        this, tr("Export SVG File(s)"), QString(), "Images (*.svg)");

    if (fileName.isNull())  // user canceled export
    {
        ui->actionPause->setChecked(wasPaused);
    }
    else
    {
        plotMan->exportSvg(fileName);
    }
}

PlotViewSettings MainWindow::viewSettings() const
{
    return plotMenu.viewSettings();
}

void MainWindow::messageHandler(QtMsgType type,
                                const QString &logString,
                                const QString &msg)
{
    if (ui != NULL)
        ui->ptLog->appendPlainText(logString);

    if (type != QtDebugMsg && ui != NULL)
    {
        ui->statusBar->showMessage(msg, 5000);
    }
}

void MainWindow::saveAllSettings(QSettings* settings)
{
    saveMWSettings(settings);
    portControl.saveSettings(settings);
    dataFormatPanel.saveSettings(settings);
    stream.saveSettings(settings);
    plotControlPanel.saveSettings(settings);
    plotMenu.saveSettings(settings);
    commandPanel.saveSettings(settings);
    recordPanel.saveSettings(settings);
    textView.saveSettings(settings);
    updateCheckDialog.saveSettings(settings);
}

void MainWindow::loadAllSettings(QSettings* settings)
{
    loadMWSettings(settings);
    portControl.loadSettings(settings);
    dataFormatPanel.loadSettings(settings);
    stream.loadSettings(settings);
    plotControlPanel.loadSettings(settings);
    plotMenu.loadSettings(settings);
    commandPanel.loadSettings(settings);
    recordPanel.loadSettings(settings);
    textView.loadSettings(settings);
    updateCheckDialog.loadSettings(settings);
}

void MainWindow::saveMWSettings(QSettings* settings)
{
    // save window geometry
    settings->beginGroup(SettingGroup_MainWindow);
    settings->setValue(SG_MainWindow_Size, size());
    settings->setValue(SG_MainWindow_Pos, pos());
    // save active panel
    settings->setValue(SG_MainWindow_ActivePanel,
                       panelSettingMap.value(ui->tabWidget->currentIndex()));
    // save panel minimization
    settings->setValue(SG_MainWindow_HidePanels,
                       ui->tabWidget->hideAction.isChecked());
    // save window maximized state
    settings->setValue(SG_MainWindow_Maximized,
                       bool(windowState() & Qt::WindowMaximized));
    // save toolbar/dockwidgets state
    settings->setValue(SG_MainWindow_State, saveState());
    // directory of the last configuration file
    settings->setValue(SG_MainWindow_ConfigDir, configDir);
    settings->endGroup();
}

void MainWindow::loadMWSettings(QSettings* settings)
{
    settings->beginGroup(SettingGroup_MainWindow);
    // load window geometry
    resize(settings->value(SG_MainWindow_Size, size()).toSize());
    move(settings->value(SG_MainWindow_Pos, pos()).toPoint());

    // set active panel
    QString tabSetting =
        settings->value(SG_MainWindow_ActivePanel, QString()).toString();
    ui->tabWidget->setCurrentIndex(
        panelSettingMap.key(tabSetting, ui->tabWidget->currentIndex()));

    // hide panels
    ui->tabWidget->hideAction.setChecked(
        settings->value(SG_MainWindow_HidePanels,
                        ui->tabWidget->hideAction.isChecked()).toBool());

    // maximize window
    if (settings->value(SG_MainWindow_Maximized).toBool())
    {
        showMaximized();
    }

    // load toolbar/dockwidgets state
    restoreState(settings->value(SG_MainWindow_State).toByteArray());
    settings->setValue(SG_MainWindow_State, saveState());

    // directory of the last configuration file
    configDir = settings->value(SG_MainWindow_ConfigDir, QString()).toString();

    settings->endGroup();
}

QString MainWindow::configFileStartPath(const QString& fileName) const
{
    const QString dir = configDir.isEmpty() ? QDir::homePath() : configDir;
    return QDir(dir).filePath(fileName);
}

bool MainWindow::applyConfiguration(const QString& fileName)
{
    QFileInfo info(fileName);
    if (!info.exists() || !info.isFile())
    {
        qCritical() << "Configuration file not found:" << fileName;
        return false;
    }

    QSettings settings(fileName, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError)
    {
        qCritical() << "Failed to read configuration file:" << fileName;
        return false;
    }

    configDir = info.absolutePath();
    loadAllSettings(&settings);
    return true;
}

bool MainWindow::loadConfiguration(const QString& fileName)
{
    // No dialog here: this is also called from the command line and from the
    // OS file-open event. Callers that have a UI (the menu action) report the
    // failure themselves.
    if (!applyConfiguration(fileName))
    {
        return false;
    }

    // values that did not change emit nothing, redraw once anyway
    plotMan->replot();
    statusBar()->showMessage(tr("Configuration loaded from %1").arg(fileName), 5000);
    return true;
}

bool MainWindow::saveConfiguration(const QString& fileName)
{
    QSettings settings(fileName, QSettings::IniFormat);
    saveAllSettings(&settings);
    settings.sync();

    if (settings.status() != QSettings::NoError)
    {
        qCritical() << "Failed to write configuration file:" << fileName;
        return false;
    }

    configDir = QFileInfo(fileName).absolutePath();
    return true;
}

void MainWindow::onSaveSettings()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, tr("Save Configuration"),
        configFileStartPath("serialplot.ini"),
        tr("Configuration files (*.ini);;All files (*)"));

    if (fileName.isEmpty()) // user canceled
    {
        return;
    }

    if (saveConfiguration(fileName))
    {
        statusBar()->showMessage(tr("Configuration saved to %1").arg(fileName), 5000);
    }
    else
    {
        QMessageBox::warning(this, tr("Save Configuration"),
                             tr("Could not save the configuration file:\n%1")
                             .arg(fileName));
    }
}

void MainWindow::onLoadSettings()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Open Configuration"),
        configFileStartPath(QString()),
        tr("Configuration files (*.ini);;All files (*)"));

    if (fileName.isEmpty()) // user canceled
    {
        return;
    }

    if (!loadConfiguration(fileName))
    {
        QMessageBox::warning(this, tr("Open Configuration"),
                             tr("Could not load the configuration file:\n%1")
                             .arg(fileName));
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;

    for (const QUrl& url : event->mimeData()->urls())
    {
        if (url.isLocalFile() && url.toLocalFile().endsWith(".ini", Qt::CaseInsensitive))
        {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls())
    {
        if (url.isLocalFile() && url.toLocalFile().endsWith(".ini", Qt::CaseInsensitive))
        {
            loadConfiguration(url.toLocalFile());
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::handleCommandLineOptions(const QCoreApplication &app)
{
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsCompactedShortOptions);
    parser.setApplicationDescription("Small and simple software for plotting data from serial port in realtime.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOpt({"c", "config"}, "Load configuration from file.", "filename");
    QCommandLineOption portOpt({"p", "port"}, "Set port name.", "port name");
    QCommandLineOption baudrateOpt({"b" ,"baudrate"}, "Set port baud rate.", "baud rate");
    QCommandLineOption openPortOpt({"o", "open"}, "Open serial port.");

    parser.addOption(configOpt);
    parser.addOption(portOpt);
    parser.addOption(baudrateOpt);
    parser.addOption(openPortOpt);

    parser.addPositionalArgument("config",
                                 "Configuration file to load (same as --config).",
                                 "[config]");

    parser.process(app);

    // the configuration file can be given with `-c` or simply as the first
    // argument, so that the file can be opened with the application directly
    QString configFile;
    if (parser.isSet(configOpt))
    {
        configFile = parser.value(configOpt);
    }
    else if (!parser.positionalArguments().isEmpty())
    {
        configFile = parser.positionalArguments().first();
    }

    if (!configFile.isEmpty())
    {
        if (!applyConfiguration(configFile))
        {
            qCritical() << "Configuration file not found or not readable. Closing application.";
            std::exit(1);
        }
    }

    if (parser.isSet(portOpt))
    {
        portControl.selectPort(parser.value(portOpt));
    }

    if (parser.isSet(baudrateOpt))
    {
        portControl.selectBaudrate(parser.value(baudrateOpt));
    }

    if (parser.isSet(openPortOpt))
    {
        portControl.openPort();
    }
}
