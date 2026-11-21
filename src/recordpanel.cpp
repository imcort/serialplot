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

#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QFileDialog>
#include <QRegularExpression>
#include <QCompleter>
#include <QFileSystemModel>
#include <ctime>

#include "recordpanel.h"
#include "ui_recordpanel.h"
#include "setting_defines.h"

RecordPanel::RecordPanel(Stream* stream, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RecordPanel),
    recordToolBar(tr("Record Toolbar")),
    recordAction(QIcon::fromTheme("media-record"), tr("Record"), this),
    recorder(this)
{
    overwriteSelected = false;
    _stream = stream;

    ui->setupUi(this);

    recordToolBar.setObjectName("tbRecord");

    recordAction.setCheckable(true);
    recordToolBar.addAction(&recordAction);
    ui->pbRecord->setDefaultAction(&recordAction);

    connect(ui->pbBrowse, &QPushButton::clicked,
            this, &RecordPanel::selectFile);
    connect(&recordAction, &QAction::triggered,
            this, &RecordPanel::onRecord);

    connect(ui->spDecimals, &QSpinBox::valueChanged,
            [this](int decimals)
            {
                recorder.setDecimals(decimals);
            });

    connect(&recordAction, &QAction::toggled, ui->leSeparator, &QWidget::setDisabled);
    connect(&recordAction, &QAction::toggled, ui->pbBrowse, &QWidget::setDisabled);

    QCompleter *completer = new QCompleter(this);
    auto fileSystemModel = new QFileSystemModel(completer);
    fileSystemModel->setRootPath(QDir::currentPath());
    completer->setModel(fileSystemModel);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    ui->leFileName->setCompleter(completer);
}

RecordPanel::~RecordPanel()
{
    delete ui;
}

QToolBar* RecordPanel::toolbar()
{
    return &recordToolBar;
}

bool RecordPanel::selectFile()
{
    const QString initial = selectedFile().isEmpty() ?
        QString("record.csv") : selectedFile();

    QString fileName = QFileDialog::getSaveFileName(
        parentWidget(), tr("Select recording file"), initial,
        tr("CSV files (*.csv);;All files (*)"));

    if (fileName.isEmpty())
    {
        return false;
    }
    else
    {
        fileName = ensureCsvSuffix(fileName);
        setSelectedFile(fileName);
        overwriteSelected = QFile::exists(fileName);
        return true;
    }
}

QString RecordPanel::selectedFile() const
{
    return ui->leFileName->text();
}

void RecordPanel::setSelectedFile(QString f)
{
    ui->leFileName->setText(f);
}

QString RecordPanel::getSelectedFile()
{
    if (selectedFile().isEmpty())
    {
        if (!selectFile()) return QString();
    }

    const bool hasTimestampTemplate = selectedFile().contains("%");
    QString fileName = selectedFile();

    // expand strftime placeholders such as data_%Y%m%d_%H%M%S
    if (hasTimestampTemplate)
    {
        fileName = formatTimeStamp(fileName);
    }

    fileName = ensureCsvSuffix(fileName);

    // Auto date mode generates a new file name for every recording
    if (ui->cbAutoDate->isChecked())
    {
        fileName = autoDateFileName(fileName);
        // Keep a strftime template in the edit box so that it is expanded
        // again on the next recording.
        if (!hasTimestampTemplate) setSelectedFile(fileName);
        return fileName;
    }

    // Without auto date an existing file needs confirmation, unless it was
    // explicitly selected by the user in the file dialog.
    if (!overwriteSelected && QFile::exists(fileName))
    {
        if (!confirmOverwrite(fileName)) return QString();
        // confirmOverwrite() may have opened the file dialog
        fileName = ensureCsvSuffix(selectedFile());
        if (fileName.isEmpty()) return QString();
    }

    if (!hasTimestampTemplate) setSelectedFile(fileName);

    return fileName;
}

QString RecordPanel::formatTimeStamp(QString t) const
{
    const auto maxSize = t.size() + 1024;
    QByteArray buffer(maxSize, 0);
    char* r = buffer.data();

    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime (&rawtime);
    strftime(r, maxSize, t.toLatin1().data(), timeinfo);

    return QString(r);
}

void RecordPanel::onRecord(bool start)
{
    if (!start)
    {
        stopRecording();
        return;
    }

    bool canceled = false;
    if (ui->leSeparator->text().isEmpty())
    {
        QMessageBox::critical(this, "Error",
                              "Column separator cannot be empty! Please select a separator.");
        ui->leSeparator->setFocus(Qt::OtherFocusReason);
        canceled = true;
    }

    // check file name
    QString fn;
    if (!canceled)
    {
        fn = getSelectedFile();
        canceled = fn.isEmpty();
    }

    if (canceled)
    {
        recordAction.setChecked(false);
    }
    else
    {
        overwriteSelected = false;
        // TODO: show more visible error message when recording fails
        if (!startRecording(fn))
            recordAction.setChecked(false);
    }
}

QString RecordPanel::ensureCsvSuffix(QString fileName) const
{
    if (fileName.isEmpty()) return fileName;

    QFileInfo fileInfo(fileName);
    if (fileInfo.suffix().isEmpty())
    {
        if (fileName.endsWith('.')) fileName.chop(1);
        fileName += ".csv";
    }
    return fileName;
}

QString RecordPanel::autoDateFileName(QString fileName) const
{
    fileName = ensureCsvSuffix(fileName);

    QFileInfo fileInfo(fileName);
    QString base = fileInfo.completeBaseName();
    const QString suffix = fileInfo.suffix().isEmpty() ? "csv" : fileInfo.suffix();

    // Drop a timestamp added by a previous recording so that repeated
    // recordings don't accumulate dates in the file name.
    static const QRegularExpression dateSuffix(
        R"(_\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(?:_\d+)?$)");
    base.remove(dateSuffix);
    if (base.isEmpty()) base = "record";

    const QString stamp =
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    const QString dir = fileInfo.path();
    const QString prefix = (dir == ".") ? QString() : dir + "/";

    QString candidate = prefix + base + "_" + stamp + "." + suffix;
    int counter = 1;
    while (QFile::exists(candidate))
    {
        candidate = prefix + base + "_" + stamp + "_" +
            QString::number(counter++) + "." + suffix;
    }

    return candidate;
}

bool RecordPanel::confirmOverwrite(QString fileName)
{
    // prepare message box
    QMessageBox mb(parentWidget());
    mb.setWindowTitle(tr("File Already Exists"));
    mb.setIcon(QMessageBox::Warning);
    mb.setText(tr("File (%1) already exists. How to continue?").arg(fileName));

    auto bCancel    = mb.addButton(QMessageBox::Cancel);
    auto bOverwrite = mb.addButton(tr("Overwrite"), QMessageBox::DestructiveRole);
    mb.addButton(tr("Select Another File"), QMessageBox::YesRole);

    mb.setEscapeButton(bCancel);

    // show message box
    mb.exec();

    if (mb.clickedButton() == bCancel)
    {
        return false;
    }
    else if (mb.clickedButton() == bOverwrite)
    {
        setSelectedFile(fileName);
        return true;
    }
    else                    // select button
    {
        return selectFile();
    }
}

bool RecordPanel::startRecording(QString fileName)
{
    // A header is always written
    QStringList channelNames = _stream->infoModel()->channelNames();

    // markers get their own columns, the recorder appends the values itself
    const bool withMarkers = _stream->markersEnabled();
    if (withMarkers)
    {
        channelNames << tr("Marker") << tr("Marker Value");
    }

    recorder.setMarkerProvider(withMarkers ? _stream : nullptr);

    if (recorder.startRecording(fileName, getSeparator(), channelNames))
    {
        _stream->connectFollower(&recorder);
        return true;
    }
    else
    {
        return false;
    }
}

void RecordPanel::stopRecording(void)
{
    recorder.stopRecording();
    recorder.setMarkerProvider(nullptr);
    _stream->disconnectFollower(&recorder);
}

void RecordPanel::onPortClose()
{
    if (recordAction.isChecked())
    {
        stopRecording();
        recordAction.setChecked(false);
    }
}

QString RecordPanel::getSeparator() const
{
    QString sep = ui->leSeparator->text();
    sep.replace("\\t", "\t");
    return sep;
}

void RecordPanel::saveSettings(QSettings* settings)
{
    settings->beginGroup(SettingGroup_Record);
    settings->setValue(SG_Record_AutoDate, ui->cbAutoDate->isChecked());
    settings->setValue(SG_Record_Separator, ui->leSeparator->text());
    settings->setValue(SG_Record_Decimals, ui->spDecimals->text());
    settings->endGroup();
}

void RecordPanel::loadSettings(QSettings* settings)
{
    settings->beginGroup(SettingGroup_Record);
    ui->cbAutoDate->setChecked(
        settings->value(SG_Record_AutoDate, ui->cbAutoDate->isChecked()).toBool());
    ui->leSeparator->setText(settings->value(SG_Record_Separator, ui->leSeparator->text()).toString());
    ui->spDecimals->setValue(settings->value(SG_Record_Decimals, ui->spDecimals->value()).toInt());
    settings->endGroup();
}
