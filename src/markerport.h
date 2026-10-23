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

#ifndef MARKERPORT_H
#define MARKERPORT_H

#include <QObject>
#include <QIODevice>
#include <QChar>

/**
 * Reads an auxiliary device (a serial "tagger") and reports every received
 * character as a marker event.
 *
 * The device itself is owned by the caller, the port is opened and configured
 * elsewhere (see `PortControl`), this class only consumes the incoming bytes.
 */
class MarkerPort : public QObject
{
    Q_OBJECT
public:
    explicit MarkerPort(QIODevice* device, QObject* parent = 0);

signals:
    /**
     * Emitted once for every received character.
     *
     * `value` is the character that was actually received, it is stored with
     * the marker so that it can be written to the recording.
     */
    void markerReceived(QChar value);

private slots:
    void onDataReady();

private:
    QIODevice* _device;
};

#endif // MARKERPORT_H
