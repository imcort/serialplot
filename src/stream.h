/*
  Copyright © 2018 Hasan Yavuz Özderya

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

#ifndef STREAM_H
#define STREAM_H

#include <QObject>
#include <QModelIndex>
#include <QVector>
#include <QSettings>

#include "sink.h"
#include "source.h"
#include "channelinfomodel.h"
#include "streamchannel.h"
#include "framebuffer.h"
#include "markerprovider.h"

/**
 * Main waveform storage class. It consists of channels. Channels are
 * synchronized with each other.
 *
 * Implements `Sink` class for data entry. It's expected to be
 * connected to a `Device` source.
 *
 * Also keeps the markers placed by an auxiliary input (see `MarkerPort`).
 * A marker belongs to the next sample that is going to be received, and is
 * stored as an absolute sample index so that it stays valid while the ring
 * buffers rotate. Implements `MarkerProvider` so that the recorder can write
 * them out.
 */
class Stream : public QObject, public Sink, public MarkerProvider
{
    Q_OBJECT

public:
    /**
     * @param nc number of channels
     * @param x has X data input
     * @param ns number of samples
     */
    Stream(unsigned nc = 1, bool x = false, unsigned ns = 2);
    ~Stream();

    bool hasX() const;
    unsigned numChannels() const;

    unsigned numSamples() const;
    const StreamChannel* channel(unsigned index) const;
    StreamChannel* channel(unsigned index);
    QVector<const StreamChannel*> allChannels() const;
    const ChannelInfoModel* infoModel() const;
    ChannelInfoModel* infoModel();

    /// Saves channel information
    void saveSettings(QSettings* settings) const;
    /// Load channel information
    void loadSettings(QSettings* settings);

    // markers

    /**
     * Enables/disables the marker input. Markers are only accepted while
     * enabled, markers that are already present are kept.
     */
    void setMarkersEnabled(bool enabled);
    bool markersEnabled() const;

    /**
     * Places a marker carrying `value` on the next sample to be received.
     *
     * Because the marker always arrives *between* two sample batches, the
     * first sample after the event is the closest one to the tag time that
     * can still be stored in the recording.
     *
     * Does nothing when markers are disabled. Values that land on the same
     * sample are concatenated, a sample carries a single marker.
     */
    void addMarker(QChar value);

    /// Absolute index of the oldest sample that is still in the buffers
    quint64 windowStart() const;

    /// Markers that are currently inside the buffers, in ascending order
    QVector<quint64> visibleMarkers() const;

    // implementation of `MarkerProvider`
    quint64 totalSamples() const override;
    QString markerValue(quint64 sampleIndex) const override;

protected:
    // implementations for `Sink`
    virtual void setNumChannels(unsigned nc, bool x);
    virtual void feedIn(const SamplePack& pack);

signals:
    void numChannelsChanged(unsigned value);
    void numSamplesChanged(unsigned value);
    void channelAdded(const StreamChannel* chan);
    void channelNameChanged(unsigned channel, QString name); // TODO: does it stay?
    void dataAdded(); ///< emitted when data added to channel man.
    /// Emitted when a marker is added or dropped from the buffers
    void markersChanged();
    /**
     * Emitted when the X buffer object itself is replaced, i.e. when the
     * `XFrameBuffer` pointers returned by `StreamChannel::xData()` change.
     *
     * Anyone holding such a pointer (plot curves etc.) must re-read it,
     * the previous buffer is deleted right after this signal is emitted.
     */
    void xBufferChanged();

public slots:
    /// Change number of samples (buffer size)
    void setNumSamples(unsigned value);

    /// Change X axis style
    /// @note Ignored when X is provided by source (hasX == true)
    void setXAxis(bool asIndex, double min, double max);

    /// When paused data feed is ignored
    void pause(bool paused);

    /// Clears buffer data (fills with 0)
    void clear();

private:
    unsigned _numSamples;
    bool _paused;

    bool _hasx;
    XFrameBuffer* xData;
    QList<StreamChannel*> channels;

    ChannelInfoModel _infoModel;

    bool xAsIndex;
    double xMin, xMax;

    // markers

    /// A marker placed on a sample. `index` is absolute.
    struct Marker
    {
        quint64 index;
        QString value;
    };

    bool _markersEnabled;
    quint64 _totalSamples;         ///< number of samples fed so far
    QVector<Marker> _markers;      ///< ascending by `index`

    /// Drops markers that scrolled out of the buffers
    void trimMarkers();

    /**
     * Applies gain and offset to given pack.
     *
     * Caller is responsible for deleting returned `SamplePack`.
     *
     * @note Should be called only when gain or offset is enabled. Guard with
     * `ChannelInfoModel::gainOrOffsetEn()`.
     *
     * @param pack input data
     * @return modified data
     */
    const SamplePack* applyGainOffset(const SamplePack& pack) const;

    /// Returns a new virtual X buffer for settings
    XFrameBuffer* makeXBuffer() const;
};


#endif // STREAM_H
