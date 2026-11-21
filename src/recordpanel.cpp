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
#include <QSaveFile>
#include <QTextStream>
#include <QVector>
#include <QStringList>
#include <QRegularExpression>
#include <QCompleter>
#include <QFileSystemModel>
#include <ctime>

#include "recordpanel.h"
#include "ui_recordpanel.h"
#include "setting_defines.h"
#include "csvutils.h"

namespace {

/// Picks the most likely column separator from the CSV header line.
QChar detectSeparator(const QString& line)
{
    int commas = 0;
    int semicolons = 0;
    int tabs = 0;
    bool quoted = false;

    for (const QChar c : line)
    {
        if (c == '"')
        {
            quoted = !quoted;
        }
        else if (!quoted)
        {
            if (c == ',') commas++;
            else if (c == ';') semicolons++;
            else if (c == '\t') tabs++;
        }
    }

    if (tabs > 0 && tabs >= commas && tabs >= semicolons) return '\t';
    if (semicolons > 0 && semicolons > commas) return ';';
    return ',';
}

/// Parses one CSV line, honoring quoted fields.
QStringList parseCsvLine(const QString& line, QChar separator)
{
    QStringList fields;
    QString field;
    bool quoted = false;

    for (int i = 0; i < line.size(); i++)
    {
        const QChar c = line[i];
        if (quoted)
        {
            if (c == '"')
            {
                if (i + 1 < line.size() && line[i + 1] == '"')
                {
                    field += '"';
                    i++;
                }
                else
                {
                    quoted = false;
                }
            }
            else
            {
                field += c;
            }
        }
        else if (c == '"')
        {
            quoted = true;
        }
        else if (c == separator)
        {
            fields.append(field);
            field.clear();
        }
        else
        {
            field += c;
        }
    }

    fields.append(field);
    return fields;
}

} // namespace

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
    connect(ui->pbSaveBuffer, &QPushButton::clicked,
            this, &RecordPanel::saveBuffer);
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

void RecordPanel::saveBuffer()
{
    // The RecordPanel can be hidden when the toolbar button is used, so use
    // the top-level window as the dialog parent.
    QWidget* dialogParent = window();

    if (_stream->numChannels() == 0 || _stream->usedSamples() == 0)
    {
        QMessageBox::warning(dialogParent, tr("Save Current Buffer"),
                             tr("There is no buffer data to save."));
        return;
    }

    const QString defaultName = QString("buffer_%1.csv").arg(
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));

    QString fileName = QFileDialog::getSaveFileName(
        dialogParent, tr("Save Current Buffer"), defaultName,
        tr("CSV files (*.csv);;All files (*)"));

    if (fileName.isEmpty()) return;

    fileName = ensureCsvSuffix(fileName);
    if (!saveBufferToFile(fileName))
    {
        QMessageBox::critical(dialogParent, tr("Save Current Buffer"),
                              tr("Could not save the buffer to:\n%1").arg(fileName));
    }
    else
    {
        qInfo() << "Current buffer saved to:" << fileName;
    }
}

bool RecordPanel::saveBufferToFile(const QString& fileName)
{
    const unsigned numChannels = _stream->numChannels();
    const unsigned capacity = _stream->numSamples();
    const unsigned used = _stream->usedSamples();
    if (numChannels == 0 || used == 0) return false;

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCritical() << "Couldn't open buffer file for writing:" << file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setRealNumberNotation(QTextStream::FixedNotation);
    stream.setRealNumberPrecision(ui->spDecimals->value());

    const QString sep = getSeparator();
    const bool withMarkers = _stream->markersEnabled();

    // header
    for (unsigned ci = 0; ci < numChannels; ci++)
    {
        if (ci != 0) stream << sep;
        stream << _stream->infoModel()->name(ci);
    }
    if (withMarkers)
    {
        if (numChannels != 0) stream << sep;
        stream << tr("Marker") << sep << tr("Marker Value");
    }
    stream << '\n';

    // Rows, oldest valid sample first. When the buffer isn't full the valid
    // samples are at the end of the ring buffer, the leading part still holds
    // the initial fill value.
    const unsigned startIndex = capacity - used;
    const quint64 firstAbsoluteIndex = _stream->totalSamples() - used;
    for (unsigned i = startIndex; i < capacity; i++)
    {
        for (unsigned ci = 0; ci < numChannels; ci++)
        {
            if (ci != 0) stream << sep;
            stream << _stream->channel(ci)->yData()->sample(i);
        }

        if (withMarkers)
        {
            const QString value = _stream->markerValue(
                firstAbsoluteIndex + (i - startIndex));
            if (numChannels != 0) stream << sep;
            stream << (value.isEmpty() ? QLatin1Char('0') : QLatin1Char('1')) << sep;
            if (!value.isEmpty()) stream << csvField(value, sep);
        }
        stream << '\n';
    }

    stream.flush();
    if (!file.commit())
    {
        qCritical() << "File save error during buffer save:" << file.error();
        return false;
    }

    return true;
}

void RecordPanel::loadBuffer()
{
    QWidget* dialogParent = window();
    const QString fileName = QFileDialog::getOpenFileName(
        dialogParent, tr("Load CSV into Buffer"), QString(),
        tr("CSV files (*.csv);;All files (*)"));

    if (fileName.isEmpty()) return;

    if (loadBufferFromFile(fileName))
    {
        qInfo() << "Loaded CSV into buffer:" << fileName;
    }
}

bool RecordPanel::loadBufferFromFile(const QString& fileName)
{
    QWidget* dialogParent = window();
    auto fail = [&](const QString& message)
    {
        qCritical().noquote() << message;
        QMessageBox::critical(dialogParent, tr("Load to Buffer"), message);
        return false;
    };

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return fail(tr("Could not open file:\n%1\n%2")
                    .arg(fileName, file.errorString()));
    }

    QTextStream stream(&file);
    QString line;
    unsigned lineNo = 0;

    if (!stream.readLineInto(&line))
    {
        return fail(tr("The file is empty."));
    }
    lineNo++;

    if (line.trimmed().isEmpty())
    {
        return fail(tr("The file has no header line."));
    }

    const QChar separator = detectSeparator(line);
    const QStringList header = parseCsvLine(line.trimmed(), separator);

    // Older recordings can contain a leading timestamp column. Marker columns
    // are not data channels and are skipped as well.
    const bool hasTimestamp = header.size() > 1 &&
        header[0].trimmed().compare("timestamp", Qt::CaseInsensitive) == 0;

    const QString markerName = tr("Marker");
    const QString markerValueName = tr("Marker Value");

    QVector<int> dataColumns;
    QStringList names;
    for (int i = 0; i < header.size(); i++)
    {
        if (hasTimestamp && i == 0) continue;

        const QString name = header[i].trimmed();
        if (name.compare(markerName, Qt::CaseInsensitive) == 0 ||
            name.compare(markerValueName, Qt::CaseInsensitive) == 0)
        {
            continue;
        }

        dataColumns.append(i);
        names.append(name.isEmpty() ? tr("Channel %1").arg(i + 1) : name);
    }

    const int numDataColumns = dataColumns.size();
    if (numDataColumns == 0)
    {
        return fail(tr("No data columns found in the CSV header."));
    }

    if (numDataColumns != int(_stream->numChannels()))
    {
        return fail(tr("The CSV file has %1 data columns, but the current data "
                       "format has %2 channels.\n"
                       "Set the channel count in the Data Format tab to match "
                       "the file.")
                    .arg(numDataColumns).arg(_stream->numChannels()));
    }

    // First pass: validate every value and count the rows. This also makes
    // sure that a broken file doesn't clear the current buffer.
    unsigned rowCount = 0;
    while (stream.readLineInto(&line))
    {
        lineNo++;
        if (line.trimmed().isEmpty()) continue;

        const QStringList fields = parseCsvLine(line.trimmed(), separator);
        for (int ci = 0; ci < numDataColumns; ci++)
        {
            const int column = dataColumns[ci];
            if (column >= fields.size())
            {
                return fail(tr("Line %1: expected at least %2 columns.")
                            .arg(lineNo).arg(column + 1));
            }

            bool ok;
            fields[column].trimmed().toDouble(&ok);
            if (!ok)
            {
                return fail(tr("Line %1, column %2: \"%3\" is not a number.")
                            .arg(lineNo).arg(column + 1)
                            .arg(fields[column].trimmed()));
            }
        }
        rowCount++;
    }

    if (rowCount == 0)
    {
        return fail(tr("The CSV file has no data rows."));
    }

    const unsigned capacity = _stream->numSamples();
    const unsigned skip = rowCount > capacity ? rowCount - capacity : 0;
    const unsigned storedRows = rowCount - skip;

    if (skip > 0)
    {
        qWarning() << "CSV file has" << rowCount << "rows, buffer capacity is"
                   << capacity << "- loading the last" << storedRows << "rows.";
    }

    // Second pass: read only the rows that fit into the buffer.
    file.close();
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return fail(tr("Could not reopen file:\n%1\n%2")
                    .arg(fileName, file.errorString()));
    }

    QTextStream stream2(&file);
    if (!stream2.readLineInto(&line))
    {
        return fail(tr("Could not read the CSV header again."));
    }

    QVector<QVector<double>> data(numDataColumns);
    for (auto& column : data) column.reserve(storedRows);

    unsigned rowIndex = 0;
    lineNo = 1;
    while (stream2.readLineInto(&line))
    {
        lineNo++;
        if (line.trimmed().isEmpty()) continue;

        if (rowIndex < skip)
        {
            rowIndex++;
            continue;
        }

        const QStringList fields = parseCsvLine(line.trimmed(), separator);
        for (int ci = 0; ci < numDataColumns; ci++)
        {
            const int column = dataColumns[ci];
            bool ok;
            const double value = fields.value(column).trimmed().toDouble(&ok);
            if (!ok)
            {
                return fail(tr("Line %1, column %2: invalid number.")
                            .arg(lineNo).arg(column + 1));
            }
            data[ci].append(value);
        }
        rowIndex++;
    }

    if (!_stream->replaceBuffer(data, names))
    {
        return fail(tr("Could not replace the buffer contents."));
    }

    qInfo() << "Loaded" << storedRows << "of" << rowCount
            << "rows from" << fileName;
    return true;
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
