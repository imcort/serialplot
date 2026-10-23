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

#include "markerport.h"

MarkerPort::MarkerPort(QIODevice* device, QObject* parent) :
    QObject(parent)
{
    _device = device;

    if (_device != nullptr)
    {
        connect(_device, &QIODevice::readyRead,
                this, &MarkerPort::onDataReady);
    }
}

void MarkerPort::onDataReady()
{
    if (_device == nullptr) return;

    // every character the tagger sends is one marker, keep the byte so that
    // it can be written into the recording
    const QByteArray data = _device->readAll();
    if (data.isEmpty()) return;

    for (char byte : data)
    {
        emit markerReceived(QChar(static_cast<uchar>(byte)));
    }
}
