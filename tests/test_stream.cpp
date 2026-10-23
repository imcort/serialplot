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

#include "stream.h"

#include <QSignalSpy>

#include "catch.hpp"
#include "test_helpers.h"

TEST_CASE("construction of stream with default values", "[memory, stream]")
{
    // default values are an empty stream with no channels
    Stream s;

    REQUIRE(s.numChannels() == 1);
    REQUIRE(!s.hasX());
    REQUIRE(s.numSamples() == 2);
}

TEST_CASE("construction of stream with parameters", "[memory, stream]")
{
    Stream s(4, false, 100);

    REQUIRE(s.numChannels() == 4);
    REQUIRE(!s.hasX());
    REQUIRE(s.numSamples() == 100);

    for (unsigned i = 0; i < 4; i++)
    {
        const StreamChannel* c = s.channel(i);
        REQUIRE(c != NULL);
        REQUIRE(c->index() == i);
    }
}

TEST_CASE("changing stream number of channels via sink", "[memory, stream, sink]")
{
    Stream s;
    TestSource so(3, false);
    so.connectSink(&s);

    // nc=3, x= false
    REQUIRE(s.numChannels() == 3);
    REQUIRE(!s.hasX());
    for (unsigned i = 0; i < 3; i++)
    {
        const StreamChannel* c = s.channel(i);
        REQUIRE(c != NULL);
        REQUIRE(c->index() == i);
    }

// TODO: enable test when `Stream` supports X channel
#if 0
    // increase nc value, add X
    so._setNumChannels(5, true);

    REQUIRE(s.numChannels() == 5);
    REQUIRE(s.hasX());

    for (unsigned i = 0; i < 5; i++)
    {
        const StreamChannel* c = s.channel(i);
        REQUIRE(c != NULL);
        REQUIRE(c->index() == i);
    }
#endif

    // reduce nc value, remove X
    so._setNumChannels(1, false);

    REQUIRE(s.numChannels() == 1);
    REQUIRE(!s.hasX());

    for (unsigned i = 0; i < 1; i++)
    {
        const StreamChannel* c = s.channel(i);
        REQUIRE(c != NULL);
        REQUIRE(c->index() == i);
    }
}

TEST_CASE("adding data to a stream with no X", "[memory, stream, data, sink]")
{
    Stream s(3, false, 10);

    // prepare data
    SamplePack pack(5, 3, false);
    for (unsigned ci = 0; ci < 3; ci++)
    {
        for (unsigned i = 0; i < 5; i++)
        {
            pack.data(ci)[i] = i;
        }
    }

    TestSource so(3, false);
    so.connectSink(&s);

    // test
    so._feed(pack);

    for (unsigned ci = 0; ci < 3; ci++)
    {
        const StreamChannel* c = s.channel(ci);
        const FrameBuffer* y = c->yData();

        for (unsigned i = 0; i < 5; i++)
        {
            REQUIRE(y->sample(i) == 0);
        }
        for (unsigned i = 5; i < 10; i++)
        {
            REQUIRE(y->sample(i) == i-5);
        }
    }
}

// TODO: enable test when `Stream` supports X channel
#if 0
TEST_CASE("adding data to a stream with X", "[memory, stream, data, sink]")
{
    Stream s(3, false, 10);

    // prepare data
    SamplePack pack(5, 3, true);
    for (unsigned ci = 0; ci < 3; ci++)
    {
        for (unsigned i = 0; i < 5; i++)
        {
            pack.data(ci)[i] = i;
        }
    }
    // x data
    for (unsigned i = 0; i < 5; i++)
    {
        pack.xData()[i] = i+10;
    }

    TestSource so(3, true);
    REQUIRE_THROWS(so.connectSink(&s));

    // test
    so._feed(pack);

    for (unsigned ci = 0; ci < 3; ci++)
    {
        const StreamChannel* c = s.channel(ci);
        const FrameBuffer* y = c->yData();

        for (unsigned i = 0; i < 5; i++)
        {
            REQUIRE(y->sample(i) == 0);
        }
        for (unsigned i = 5; i < 10; i++)
        {
            REQUIRE(y->sample(i) == i-5);
        }
    }

    // check x
    const FrameBuffer* x = s.channel(0)->xData();
    for (unsigned i = 0; i < 5; i++)
    {
        REQUIRE(x->sample(i) == 0);
    }
    for (unsigned i = 5; i < 10; i++)
    {
        REQUIRE(x->sample(i) == (i-5)+10);
    }
}
#endif

TEST_CASE("paused stream shouldn't store data", "[memory, stream, pause]")
{
    Stream s(3, false, 10);

    // prepare data
    SamplePack pack(5, 3, false);
    for (unsigned ci = 0; ci < 3; ci++)
    {
        for (unsigned i = 0; i < 5; i++)
        {
            pack.data(ci)[i] = i;
        }
    }

    TestSource so(3, false);
    so.connectSink(&s);

    // test
    s.pause(true);
    so._feed(pack);

    for (unsigned ci = 0; ci < 3; ci++)
    {
        const StreamChannel* c = s.channel(ci);
        const FrameBuffer* y = c->yData();

        for (unsigned i = 0; i < 10; i++)
        {
            REQUIRE(y->sample(i) == 0);
        }
    }
}

TEST_CASE("clear stream data", "[memory, stream, pause]")
{
    Stream s(3, false, 10);

    // prepare data
    SamplePack pack(5, 3, false);
    for (unsigned ci = 0; ci < 3; ci++)
    {
        for (unsigned i = 0; i < 5; i++)
        {
            pack.data(ci)[i] = i;
        }
    }

    TestSource so(3, false);
    so.connectSink(&s);

    // test
    so._feed(pack);
    s.clear();

    for (unsigned ci = 0; ci < 3; ci++)
    {
        const StreamChannel* c = s.channel(ci);
        const FrameBuffer* y = c->yData();

        for (unsigned i = 0; i < 10; i++)
        {
            REQUIRE(y->sample(i) == 0);
        }
    }
}

TEST_CASE("changing X axis replaces the X buffer", "[memory, stream, x]")
{
    Stream s(3, false, 10);

    const XFrameBuffer* oldX = s.channel(0)->xData();
    for (unsigned ci = 1; ci < 3; ci++)
    {
        REQUIRE(s.channel(ci)->xData() == oldX);
    }

    // the old buffer is deleted, whoever holds its pointer must be notified
    QSignalSpy spy(&s, SIGNAL(xBufferChanged()));

    s.setXAxis(false, 0., 100.);

    REQUIRE(spy.count() == 1);

    const XFrameBuffer* newX = s.channel(0)->xData();
    REQUIRE(newX != oldX);
    for (unsigned ci = 1; ci < 3; ci++)
    {
        REQUIRE(s.channel(ci)->xData() == newX);
    }

    // new scale is in effect
    REQUIRE(newX->sample(0) == Approx(0.));
    REQUIRE(newX->sample(9) == Approx(100.));
}

TEST_CASE("markers carry the received value", "[stream, marker]")
{
    Stream s(1, false, 10);
    s.setMarkersEnabled(true);

    TestSource so(1, false);
    so.connectSink(&s);

    SamplePack pack(4, 1);
    so._feed(pack); // samples 0..3

    REQUIRE(s.totalSamples() == 4);
    REQUIRE(s.visibleMarkers().isEmpty());

    // the marker goes to the first sample that still has to be stored, the
    // previous ones are already written to the recording by now
    s.addMarker('A');
    REQUIRE(s.visibleMarkers().size() == 1);
    REQUIRE(s.visibleMarkers().first() == 4);
    REQUIRE(s.markerValue(4) == "A");
    REQUIRE(s.markerValue(3).isEmpty());

    // several characters between two samples end up in the same cell
    s.addMarker('B');
    REQUIRE(s.visibleMarkers().size() == 1);
    REQUIRE(s.markerValue(4) == "AB");

    so._feed(pack); // samples 4..7
    s.addMarker('7');
    REQUIRE(s.visibleMarkers().size() == 2);
    REQUIRE(s.visibleMarkers().last() == 8);
    REQUIRE(s.markerValue(8) == "7");
    REQUIRE(s.markerValue(5).isEmpty());
}

TEST_CASE("markers are ignored while disabled", "[stream, marker]")
{
    Stream s(1, false, 10);
    TestSource so(1, false);
    so.connectSink(&s);

    SamplePack pack(4, 1);
    so._feed(pack);

    s.addMarker('A'); // disabled by default
    REQUIRE(s.visibleMarkers().isEmpty());

    s.setMarkersEnabled(true);
    s.addMarker('B');
    REQUIRE(s.visibleMarkers().size() == 1);
    REQUIRE(s.markerValue(4) == "B");

    s.setMarkersEnabled(false);
    s.addMarker('C');
    REQUIRE(s.visibleMarkers().size() == 1);
    REQUIRE(s.markerValue(4) == "B");

    // already placed markers survive, they are data
    so._feed(pack);
    REQUIRE(s.visibleMarkers().size() == 1);
}

TEST_CASE("markers scroll out with the sample window", "[stream, marker]")
{
    Stream s(1, false, 10);
    s.setMarkersEnabled(true);

    TestSource so(1, false);
    so.connectSink(&s);

    SamplePack pack(4, 1);
    so._feed(pack);      // 0..3
    s.addMarker('X');    // marker at 4
    REQUIRE(s.visibleMarkers().size() == 1);

    so._feed(pack);      // 4..7   window starts at 0
    so._feed(pack);      // 8..11  window starts at 2
    REQUIRE(s.windowStart() == 2);
    REQUIRE(s.visibleMarkers().size() == 1);
    REQUIRE(s.markerValue(4) == "X");

    so._feed(pack);      // 12..15 window starts at 6, marker at 4 is gone
    REQUIRE(s.windowStart() == 6);
    REQUIRE(s.visibleMarkers().isEmpty());
    REQUIRE(s.markerValue(4).isEmpty());
}

TEST_CASE("clear drops the markers", "[stream, marker]")
{
    Stream s(1, false, 10);
    s.setMarkersEnabled(true);

    TestSource so(1, false);
    so.connectSink(&s);

    SamplePack pack(4, 1);
    so._feed(pack);
    s.addMarker('A');
    REQUIRE(s.visibleMarkers().size() == 1);

    QSignalSpy spy(&s, SIGNAL(markersChanged()));
    s.clear();
    REQUIRE(s.visibleMarkers().isEmpty());
    REQUIRE(s.markerValue(4).isEmpty());
    REQUIRE(spy.count() == 1);
}
