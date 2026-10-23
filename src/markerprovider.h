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

#ifndef MARKERPROVIDER_H
#define MARKERPROVIDER_H

#include <QString>
#include <QtGlobal>

/**
 * Read only access to the markers of a sample source.
 *
 * Markers are placed by an auxiliary device (a serial tagger) and belong to
 * a single sample. `DataRecorder` uses this interface to write the marker
 * column, `MarkerPlotItem` uses `Stream` directly.
 *
 * Sample indexes are absolute: they are counted from the moment the stream
 * was created and are not affected by the ring buffer wrapping around. This
 * makes them stable for a whole recording session.
 */
class MarkerProvider
{
public:
    virtual ~MarkerProvider() {}

    /// Total number of samples fed so far. This is also the index that the
    /// next sample will get.
    virtual quint64 totalSamples() const = 0;

    /**
     * Value that was received for the given absolute sample index.
     *
     * Returns an empty string when no marker was placed on that sample. The
     * value is whatever the tagging device sent, so the recorder can write it
     * into the marker column as is.
     */
    virtual QString markerValue(quint64 sampleIndex) const = 0;
};

#endif // MARKERPROVIDER_H
