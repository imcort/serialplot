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

#ifndef RECORDPANEL_H
#define RECORDPANEL_H

#include <QWidget>
#include <QString>
#include <QToolBar>
#include <QAction>

#include "datarecorder.h"
#include "stream.h"

namespace Ui {
class RecordPanel;
}

class RecordPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RecordPanel(Stream* stream, QWidget* parent = 0);
    ~RecordPanel();

    QToolBar* toolbar();

    /// Saves the current (complete) stream buffer to a CSV file.
    /// @return true on success
    bool saveBufferToFile(const QString& fileName);

    /// Loads a CSV file into the current buffers.
    /// @return true on success
    bool loadBufferFromFile(const QString& fileName);

    /// Stores settings into a `QSettings`
    void saveSettings(QSettings* settings);
    /// Loads settings from a `QSettings`.
    void loadSettings(QSettings* settings);

signals:
    void recordStarted();
    void recordStopped();

public slots:
    /// Must be called when port is closed
    void onPortClose();

    /// Asks for a file name and saves the current stream buffer
    void saveBuffer();

    /// Asks for a CSV file and loads it into the current buffers
    void loadBuffer();

private:
    Ui::RecordPanel *ui;
    QToolBar recordToolBar;
    QAction recordAction;
    bool overwriteSelected;
    DataRecorder recorder;
    Stream* _stream;

    /// Appends a `.csv` extension if the given file name has no extension.
    QString ensureCsvSuffix(QString fileName) const;

    /**
     * @brief Appends the current date and time to the file name.
     *
     * The timestamp is added to the end of the base name, before the
     * extension, in the form `_yyyy-MM-dd_HH-mm-ss`. A previously appended
     * timestamp is removed first so that repeated recordings don't accumulate
     * dates. A numeric suffix is added only in the unlikely event that two
     * recordings start within the same second.
     *
     * @return generated file name
     */
    QString autoDateFileName(QString fileName) const;

    /**
     * @brief Used to ask user confirmation if auto generated file
     * name exists.
     *
     * If user confirms overwrite, `selectedFile` is set to
     * `fileName`. User is also given option to select file and is
     * shown a file select dialog in this case.
     *
     * @param fileName auto generated file name.
     * @return false if user cancels
     */
    bool confirmOverwrite(QString fileName);

    /// Returns filename in edit box. May be invalid!
    QString selectedFile() const;
    /// Sets the filename in edit box.
    void setSelectedFile(QString f);

    /**
     * Tries to get a valid file name by handling user interactions and
     * automatic naming (date suffix, strftime placeholders, extension etc).
     *
     * Returned file name can be used immediately. File name box should also be
     * set to selected file name.
     *
     * @return empty if failure otherwise valid filename
     */
    QString getSelectedFile();

    /// Formats timestamp in given text
    QString formatTimeStamp(QString t) const;

    bool startRecording(QString fileName);
    void stopRecording(void);

    /// Returns separator text from ui. "\t" is converted to TAB
    /// character.
    QString getSeparator() const;

private slots:
    /**
     * @brief Opens up the file select dialog
     *
     * If you cancel the selection operation, currently selected file is not
     * changed.
     *
     * @return true if file selected, false if user cancels
     */
    bool selectFile();

    void onRecord(bool start);

};

#endif // RECORDPANEL_H
