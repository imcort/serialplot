/*
  Copyright © 2020 Hasan Yavuz Özderya

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

#include <QApplication>
#include <QFileOpenEvent>
#include <QtGlobal>
#include <QIcon>
#include <QCoreApplication>
#include <QPermission>
#include <QBluetoothPermission>
#include <iostream>

#include "mainwindow.h"
#include "tooltipfilter.h"
#include "version.h"

MainWindow* pMainWindow = nullptr;

/**
 * Application object that forwards files opened by the OS (double click,
 * "Open With", drag onto the dock icon) to the main window.
 *
 * No signal is used here on purpose: a file can be requested before the
 * window exists, in that case it is kept and picked up after construction.
 */
class SerialPlotApplication : public QApplication
{
public:
    SerialPlotApplication(int& argc, char** argv) :
        QApplication(argc, argv),
        _window(nullptr) {}

    /// Window that receives the files, may be `nullptr`
    void setMainWindow(MainWindow* window) { _window = window; }

    /// File that was requested before a window was set
    const QString& pendingFile() const { return _pendingFile; }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::FileOpen)
        {
            const QString file = static_cast<QFileOpenEvent*>(event)->file();
            if (!file.isEmpty())
            {
                if (_window != nullptr)
                {
                    _window->loadConfiguration(file);
                }
                else
                {
                    _pendingFile = file;
                }
            }
            return true;
        }

        return QApplication::event(event);
    }

private:
    MainWindow* _window;
    QString _pendingFile;
};
void messageHandler(QtMsgType type, const QMessageLogContext &context,
                    const QString &msg)
{
    QString logString;

    switch (type)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
        case QtInfoMsg:
            logString = "[Info] " + msg;
            break;
#endif
        case QtDebugMsg:
            logString = "[Debug] " + msg;
            break;
        case QtWarningMsg:
            logString = "[Warning] " + msg;
            break;
        case QtCriticalMsg:
            logString = "[Error] " + msg;
            break;
        case QtFatalMsg:
            logString = "[Fatal] " + msg;
            break;
    }

    std::cerr << logString.toStdString() << std::endl;

    if (pMainWindow != nullptr)
    {
        // TODO: don't call MainWindow::messageHandler if window is destroyed
        pMainWindow->messageHandler(type, logString, msg);
    }

    if (type == QtFatalMsg)
    {
        __builtin_trap();
    }
}

int main(int argc, char *argv[])
{
    SerialPlotApplication a(argc, argv);
    QApplication::setApplicationName(PROGRAM_NAME);
    QApplication::setApplicationVersion(VERSION_STRING);

#ifdef Q_OS_WIN
    QIcon::setFallbackSearchPaths(QIcon::fallbackSearchPaths() << ":icons");
    QIcon::setThemeName("tango");
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QBluetoothPermission btPermission;
    btPermission.setCommunicationModes(QBluetoothPermission::Access);
    auto permissionStatus = a.checkPermission(btPermission);
    if (permissionStatus == Qt::PermissionStatus::Undetermined)
    {
        a.requestPermission(
            btPermission, &a, [](const QPermission&) {});
    }
#endif

    qInstallMessageHandler(messageHandler);
    MainWindow w;
    pMainWindow = &w;

    // files opened by the OS (or dropped on the dock icon) are treated as
    // configuration files
    a.setMainWindow(&w);
    if (!a.pendingFile().isEmpty())
    {
        w.loadConfiguration(a.pendingFile());
    }

    ToolTipFilter ttf;
    a.installEventFilter(&ttf);

    // log application information
    qDebug() << "SerialPlot" << VERSION_STRING;
    qDebug() << "Revision" << VERSION_REVISION;

    w.show();

    return a.exec();
}
