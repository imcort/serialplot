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

#include <algorithm>

#include "stream.h"
#include "ringbuffer.h"
#include "indexbuffer.h"
#include "linindexbuffer.h"

Stream::Stream(unsigned nc, bool x, unsigned ns) :
    _infoModel(nc)
{
    _numSamples = ns;
    _paused = false;

    _markersEnabled = false;
    _totalSamples = 0;

    xAsIndex = true;
    xMin = 0;
    xMax = 1;

    // create xdata buffer
    _hasx = x;
    if (x)
    {
        // TODO: implement XRingBuffer (binary search)
        Q_ASSERT(false);
    }
    else
    {
        xData = makeXBuffer();
    }

    // create channels
    for (unsigned i = 0; i < nc; i++)
    {
        auto c = new StreamChannel(i, xData, new RingBuffer(ns), &_infoModel);
        channels.append(c);
    }
}

Stream::~Stream()
{
    for (auto ch : channels)
    {
        delete ch;
    }
    delete xData;
}

bool Stream::hasX() const
{
    return _hasx;
}

unsigned Stream::numChannels() const
{
    return channels.length();
}

unsigned Stream::numSamples() const
{
    return _numSamples;
}

const StreamChannel* Stream::channel(unsigned index) const
{
    Q_ASSERT(index < numChannels());
    return channels[index];
}

StreamChannel* Stream::channel(unsigned index)
{
    return const_cast<StreamChannel*>(static_cast<const Stream&>(*this).channel(index));
}

QVector<const StreamChannel*> Stream::allChannels() const
{
    QVector<const StreamChannel*> result(numChannels());
    for (unsigned ci = 0; ci < numChannels(); ci++)
    {
        result[ci] = channel(ci);
    }
    return result;
}

const ChannelInfoModel* Stream::infoModel() const
{
    return &_infoModel;
}

ChannelInfoModel* Stream::infoModel()
{
    return const_cast<ChannelInfoModel*>(static_cast<const Stream&>(*this).infoModel());
}

void Stream::setNumChannels(unsigned nc, bool x)
{
    unsigned oldNum = numChannels();
    if (oldNum == nc && x == _hasx) return;

    // adjust the number of channels
    if (nc > oldNum)
    {
        for (unsigned i = oldNum; i < nc; i++)
        {
            auto c = new StreamChannel(i, xData, new RingBuffer(_numSamples), &_infoModel);
            channels.append(c);
        }
    }
    else if (nc < oldNum)
    {
        for (unsigned i = oldNum-1; i > nc-1; i--)
        {
            delete channels.takeLast();
        }
    }

    // change the xdata
    if (x != _hasx)
    {
        XFrameBuffer* oldXData = nullptr;

        if (x)
        {
            // TODO: implement XRingBuffer (binary search)
            Q_ASSERT(false);
        }
        else
        {
            oldXData = xData;
            xData = makeXBuffer();
            for (auto c : channels)
            {
                c->setX(xData);
            }
        }

        _hasx = x;

        if (oldXData != nullptr)
        {
            emit xBufferChanged();
            delete oldXData;
        }
    }

    if (nc != oldNum)
    {
        _infoModel.setNumOfChannels(nc);
        // TODO: how about X change?
        emit numChannelsChanged(nc);
    }

    Sink::setNumChannels(nc, x);
}

XFrameBuffer* Stream::makeXBuffer() const
{
    if (xAsIndex)
    {
        return new IndexBuffer(_numSamples);
    }
    else
    {
        return new LinIndexBuffer(_numSamples, xMin, xMax);
    }
}

const SamplePack* Stream::applyGainOffset(const SamplePack& pack) const
{
    Q_ASSERT(infoModel()->gainOrOffsetEn());

    SamplePack* mPack = new SamplePack(pack);
    unsigned ns = pack.numSamples();

    for (unsigned ci = 0; ci < numChannels(); ci++)
    {
        // TODO: we could use some kind of map (int32, int64 would suffice) to speed things up
        bool gainEn = infoModel()->gainEn(ci);
        bool offsetEn = infoModel()->offsetEn(ci);
        if (gainEn || offsetEn)
        {
            double* mdata = mPack->data(ci);

            double gain = infoModel()->gain(ci);
            double offset = infoModel()->offset(ci);

            if (gainEn)
            {
                for (unsigned i = 0; i < ns; i++)
                {
                    mdata[i] *= gain;
                }
            }
            if (offsetEn)
            {
                for (unsigned i = 0; i < ns; i++)
                {
                    mdata[i] += offset;
                }
            }
        }
    }

    return mPack;
}

void Stream::feedIn(const SamplePack& pack)
{
    Q_ASSERT(pack.numChannels() == numChannels() &&
             pack.hasX() == hasX());

    if (_paused) return;

    unsigned ns = pack.numSamples();

    // samples are numbered from the moment the stream was created, markers
    // are stored against these absolute indexes
    _totalSamples += ns;
    trimMarkers();

    if (_hasx)
    {
        // TODO: implement XRingBuffer (binary search)
        Q_ASSERT(false);
        // static_cast<RingBuffer*>(xData)->addSamples(pack.xData(), ns);
    }

    // modified pack that gain and offset is applied to
    const SamplePack* mPack = nullptr;
    if (infoModel()->gainOrOffsetEn())
        mPack = applyGainOffset(pack);

    for (unsigned ci = 0; ci < numChannels(); ci++)
    {
        auto buf = static_cast<RingBuffer*>(channels[ci]->yData());
        double* data = (mPack == nullptr) ? pack.data(ci) : mPack->data(ci);
        buf->addSamples(data, ns);
    }

    Sink::feedIn((mPack == nullptr) ? pack : *mPack);

    if (mPack != nullptr) delete mPack;
    emit dataAdded();
}

void Stream::pause(bool paused)
{
    _paused = paused;
}

void Stream::clear()
{
    for (auto c : channels)
    {
        static_cast<RingBuffer*>(c->yData())->clear();
    }

    if (!_markers.isEmpty())
    {
        _markers.clear();
        emit markersChanged();
    }
}

void Stream::setNumSamples(unsigned value)
{
    if (value == _numSamples) return;
    _numSamples = value;

    xData->resize(value);
    for (auto c : channels)
    {
        static_cast<RingBuffer*>(c->yData())->resize(value);
    }

    // a smaller buffer may have dropped the samples that the markers point to
    trimMarkers();
}

void Stream::setMarkersEnabled(bool enabled)
{
    _markersEnabled = enabled;
}

bool Stream::markersEnabled() const
{
    return _markersEnabled;
}

void Stream::addMarker(QChar value)
{
    if (!_markersEnabled) return;

    // Attach the marker to the *next* sample, not to the last one that was
    // already delivered: a marker arrives between two batches, and by the time
    // it arrives the previous samples have been stored and, if recording, also
    // written to the file. Using the next index keeps the plot and the
    // recording consistent, the line simply appears at the first sample that
    // follows the tag event.
    const quint64 index = _totalSamples;
    const QString text(value);

    // several characters can arrive between two samples, a sample can only
    // hold one marker so keep all of them in the same cell
    if (!_markers.isEmpty() && _markers.last().index == index)
    {
        _markers.last().value.append(text);
        emit markersChanged();
        return;
    }

    _markers.append({index, text});
    trimMarkers();
    emit markersChanged();
}

quint64 Stream::windowStart() const
{
    return _totalSamples > _numSamples ? _totalSamples - _numSamples : 0;
}

QVector<quint64> Stream::visibleMarkers() const
{
    const quint64 start = windowStart();
    QVector<quint64> visible;

    // `_markers` is sorted, skip the part that scrolled out
    auto it = std::lower_bound(_markers.cbegin(), _markers.cend(), start,
                               [](const Marker& m, quint64 index)
                               {
                                   return m.index < index;
                               });
    for (; it != _markers.cend(); ++it)
    {
        visible.append(it->index);
    }
    return visible;
}

quint64 Stream::totalSamples() const
{
    return _totalSamples;
}

QString Stream::markerValue(quint64 sampleIndex) const
{
    auto it = std::lower_bound(_markers.cbegin(), _markers.cend(), sampleIndex,
                               [](const Marker& m, quint64 index)
                               {
                                   return m.index < index;
                               });

    if (it != _markers.cend() && it->index == sampleIndex)
    {
        return it->value;
    }
    return QString();
}

void Stream::trimMarkers()
{
    const quint64 start = windowStart();

    int drop = 0;
    while (drop < _markers.size() && _markers[drop].index < start)
    {
        drop++;
    }

    if (drop > 0)
    {
        _markers.remove(0, drop);
    }
}

void Stream::setXAxis(bool asIndex, double min, double max)
{
    xAsIndex = asIndex;
    xMin = min;
    xMax = max;

    // Note that x axis scaling is ignored when X is provided from source as data
    // TODO: assert (UI options for x axis should be disabled)
    if (!hasX())
    {
        XFrameBuffer* oldXData = xData;
        xData = makeXBuffer();
        for (auto c : channels)
        {
            c->setX(xData);
        }
        // let curve series drop their pointer to the old buffer before it dies
        emit xBufferChanged();
        delete oldXData;
    }
}

void Stream::saveSettings(QSettings* settings) const
{
    _infoModel.saveSettings(settings);
}

void Stream::loadSettings(QSettings* settings)
{
    _infoModel.loadSettings(settings);
}
