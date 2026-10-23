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

#ifndef MARKERPLOTITEM_H
#define MARKERPLOTITEM_H

#include <QPen>
#include <qwt_plot_item.h>

class Stream;

/**
 * Draws a vertical line over the whole canvas for every marker that lies
 * inside the visible window of `stream`.
 *
 * The item reads the markers from the stream on every redraw, so it doesn't
 * have to be kept in sync by the owner, a `replot()` is enough.
 */
class MarkerPlotItem : public QwtPlotItem
{
public:
    explicit MarkerPlotItem(const Stream* stream);

    int rtti() const override;
    void draw(QPainter* painter, const QwtScaleMap& xMap, const QwtScaleMap& yMap,
              const QRectF& canvasRect) const override;
    QRectF boundingRect() const override;

    void setPen(const QPen& pen);
    const QPen& pen() const;

private:
    const Stream* _stream;
    QPen _pen;
};

#endif // MARKERPLOTITEM_H
