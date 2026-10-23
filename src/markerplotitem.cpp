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

#include <QPainter>
#include <qwt_scale_map.h>

#include "markerplotitem.h"
#include "stream.h"

MarkerPlotItem::MarkerPlotItem(const Stream* stream)
{
    _stream = stream;
    _pen = QPen(QColor(220, 60, 60), 1, Qt::DashLine);

    setItemAttribute(QwtPlotItem::Legend, false);
    setItemAttribute(QwtPlotItem::AutoScale, false);
    // draw above the curves
    setZ(1000);
}

int MarkerPlotItem::rtti() const
{
    return QwtPlotItem::Rtti_PlotUserItem;
}

QRectF MarkerPlotItem::boundingRect() const
{
    // markers must not influence autoscaling
    return QRectF();
}

void MarkerPlotItem::setPen(const QPen& pen)
{
    _pen = pen;
}

const QPen& MarkerPlotItem::pen() const
{
    return _pen;
}

void MarkerPlotItem::draw(QPainter* painter, const QwtScaleMap& xMap,
                          const QwtScaleMap& yMap, const QRectF& canvasRect) const
{
    Q_UNUSED(yMap);

    if (_stream == nullptr || _stream->numChannels() == 0) return;

    const QVector<quint64> markers = _stream->visibleMarkers();
    if (markers.isEmpty()) return;

    const XFrameBuffer* xData = _stream->channel(0)->xData();
    if (xData == nullptr) return;

    const quint64 start = _stream->windowStart();
    const double top = canvasRect.top();
    const double bottom = canvasRect.bottom();

    painter->save();
    painter->setPen(_pen);
    for (quint64 marker : markers)
    {
        const quint64 index = marker - start;
        if (index >= xData->size()) continue;

        const double x = xMap.transform(xData->sample(unsigned(index)));
        painter->drawLine(QPointF(x, top), QPointF(x, bottom));
    }
    painter->restore();
}
