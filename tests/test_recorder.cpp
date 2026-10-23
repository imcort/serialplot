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

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

#include <QDir>
#include <QMap>
#include "datarecorder.h"
#include "test_helpers.h"

#define TEST_FILE_NAME   "sp_test_recording.csv"

TEST_CASE("test recording single channel", "[recorder]")
{
    DataRecorder rec;
    TestSource source(1, false);

    // temporary file, remove if exists
    auto fileName = QDir::tempPath() + QString("/" TEST_FILE_NAME);
    if (QFile::exists(fileName)) QFile::remove(fileName);

    // connect source → sink
    source.connectSink(&rec);

    // prepare data
    QStringList channelNames({"Channel 1"});
    SamplePack samples(5, 1);
    for (int i = 0; i < 5; i++)
    {
        samples.data(0)[i] = i+1;
    }

    // test
    rec.setDecimals(0); // the file should contain whole numbers
    rec.startRecording(fileName, ",", channelNames,
                       DataRecorder::TimestampOption::disabled);
    source._feed(samples);
    rec.stopRecording();

    // read file contents back
    QFile recordFile(fileName);
    REQUIRE(recordFile.open(QIODevice::ReadOnly | QIODevice::Text));
    // NOTE: mind the extra parantheses, otherwise 'catch' macros fail to compile
    REQUIRE((recordFile.readLine() == "Channel 1\n"));
    for (int i = 0; i < 5; i++)
        REQUIRE((recordFile.readLine() == QString("%1\n").arg(i+1)));

    // cleanup
    if (QFile::exists(fileName)) QFile::remove(fileName);
}

TEST_CASE("test recording multiple channels", "[recorder]")
{
    DataRecorder rec;
    TestSource source(3, false);

    // temporary file, remove if exists
    auto fileName = QDir::tempPath() + QString("/" TEST_FILE_NAME);
    if (QFile::exists(fileName)) QFile::remove(fileName);

    // connect source → sink
    source.connectSink(&rec);

    // prepare data
    QStringList channelNames({"Channel 1", "Channel 2", "Channel 3"});
    SamplePack samples(5, 3);
    for (int ci = 0; ci < 3; ci++)
    {
        for (int i = 0; i < 5; i++)
        {
            samples.data(ci)[i] = (ci+1)*(i+1);
        }
    }

    // test
    rec.setDecimals(0); // the file should contain whole numbers
    rec.startRecording(fileName, ",", channelNames,
                       DataRecorder::TimestampOption::disabled);
    source._feed(samples);
    rec.stopRecording();

    // read file contents back
    QFile recordFile(fileName);
    REQUIRE(recordFile.open(QIODevice::ReadOnly | QIODevice::Text));
    // NOTE: mind the extra parantheses, otherwise 'catch' macros fail to compile
    REQUIRE((recordFile.readLine() == "Channel 1,Channel 2,Channel 3\n"));
    REQUIRE((recordFile.readLine() == "1,2,3\n"));
    REQUIRE((recordFile.readLine() == "2,4,6\n"));
    REQUIRE((recordFile.readLine() == "3,6,9\n"));
    REQUIRE((recordFile.readLine() == "4,8,12\n"));
    REQUIRE((recordFile.readLine() == "5,10,15\n"));

    // cleanup
    if (QFile::exists(fileName)) QFile::remove(fileName);
}

/// Fake marker source so that the recorder can be tested without a `Stream`
class FakeMarkers : public MarkerProvider
{
public:
    quint64 total = 0;
    QMap<quint64, QString> marks;

    quint64 totalSamples() const override { return total; }
    QString markerValue(quint64 sampleIndex) const override { return marks.value(sampleIndex); }
};

TEST_CASE("recording writes the received marker values", "[recorder, marker]")
{
    const auto fileName = QDir::tempPath() + QString("/sp_test_markers.csv");
    if (QFile::exists(fileName)) QFile::remove(fileName);

    FakeMarkers markers;
    markers.total = 0;
    markers.marks.insert(1, "A");
    markers.marks.insert(3, "9");

    DataRecorder rec;
    rec.setMarkerProvider(&markers);
    rec.setDecimals(0);

    TestSource source(1, false);
    source.connectSink(&rec);

    SamplePack samples(5, 1);
    for (int i = 0; i < 5; i++)
    {
        samples.data(0)[i] = i + 1;
    }

    REQUIRE(rec.startRecording(fileName, ",", {"Channel 1", "Marker", "Marker Value"},
                               DataRecorder::TimestampOption::disabled));
    source._feed(samples);
    rec.stopRecording();

    QFile recordFile(fileName);
    REQUIRE(recordFile.open(QIODevice::ReadOnly | QIODevice::Text));

    REQUIRE((recordFile.readLine() == "Channel 1,Marker,Marker Value\n"));
    // first marker column tells whether the sample is marked, the second one
    // carries the value that the tagging device sent (empty otherwise)
    REQUIRE((recordFile.readLine() == "1,0,\n"));
    REQUIRE((recordFile.readLine() == "2,1,A\n"));
    REQUIRE((recordFile.readLine() == "3,0,\n"));
    REQUIRE((recordFile.readLine() == "4,1,9\n"));
    REQUIRE((recordFile.readLine() == "5,0,\n"));
}

TEST_CASE("recording starts at the current sample index", "[recorder, marker]")
{
    const auto fileName = QDir::tempPath() + QString("/sp_test_markers_offset.csv");
    if (QFile::exists(fileName)) QFile::remove(fileName);

    // the stream already delivered 100 samples before the recording starts
    FakeMarkers markers;
    markers.total = 100;
    markers.marks.insert(101, "B");

    DataRecorder rec;
    rec.setMarkerProvider(&markers);
    rec.setDecimals(0);

    TestSource source(1, false);
    source.connectSink(&rec);

    SamplePack samples(3, 1);
    samples.data(0)[0] = 1;
    samples.data(0)[1] = 2;
    samples.data(0)[2] = 3;

    REQUIRE(rec.startRecording(fileName, ",", {"Channel 1", "Marker", "Marker Value"},
                               DataRecorder::TimestampOption::disabled));
    source._feed(samples);
    rec.stopRecording();

    QFile recordFile(fileName);
    REQUIRE(recordFile.open(QIODevice::ReadOnly | QIODevice::Text));

    REQUIRE((recordFile.readLine() == "Channel 1,Marker,Marker Value\n"));
    REQUIRE((recordFile.readLine() == "1,0,\n"));     // index 100
    REQUIRE((recordFile.readLine() == "2,1,B\n"));     // index 101, marked
    REQUIRE((recordFile.readLine() == "3,0,\n"));     // index 102
}

TEST_CASE("recording without a marker provider has no extra column", "[recorder, marker]")
{
    const auto fileName = QDir::tempPath() + QString("/sp_test_no_markers.csv");
    if (QFile::exists(fileName)) QFile::remove(fileName);

    DataRecorder rec;
    rec.setDecimals(0);

    TestSource source(1, false);
    source.connectSink(&rec);

    SamplePack samples(2, 1);
    samples.data(0)[0] = 7;
    samples.data(0)[1] = 8;

    REQUIRE(rec.startRecording(fileName, ",", {"Channel 1"},
                               DataRecorder::TimestampOption::disabled));
    source._feed(samples);
    rec.stopRecording();

    QFile recordFile(fileName);
    REQUIRE(recordFile.open(QIODevice::ReadOnly | QIODevice::Text));

    REQUIRE((recordFile.readLine() == "Channel 1\n"));
    REQUIRE((recordFile.readLine() == "7\n"));
    REQUIRE((recordFile.readLine() == "8\n"));
}

TEST_CASE("marker values that break the CSV are quoted", "[recorder, marker]")
{
    const auto fileName = QDir::tempPath() + QString("/sp_test_markers_quoted.csv");
    if (QFile::exists(fileName)) QFile::remove(fileName);

    FakeMarkers markers;
    markers.marks.insert(0, ",");   // the separator itself
    markers.marks.insert(1, " ");   // a lone space
    markers.marks.insert(2, "\"");  // a quote

    DataRecorder rec;
    rec.setMarkerProvider(&markers);
    rec.setDecimals(0);

    TestSource source(1, false);
    source.connectSink(&rec);

    SamplePack samples(3, 1);
    for (int i = 0; i < 3; i++)
    {
        samples.data(0)[i] = i + 1;
    }

    REQUIRE(rec.startRecording(fileName, ",", {"Channel 1", "Marker", "Marker Value"},
                               DataRecorder::TimestampOption::disabled));
    source._feed(samples);
    rec.stopRecording();

    QFile recordFile(fileName);
    REQUIRE(recordFile.open(QIODevice::ReadOnly | QIODevice::Text));

    REQUIRE((recordFile.readLine() == "Channel 1,Marker,Marker Value\n"));
    REQUIRE((recordFile.readLine() == "1,1,\",\"\n"));
    REQUIRE((recordFile.readLine() == "2,1,\" \"\n"));
    REQUIRE((recordFile.readLine() == "3,1,\"\"\"\"\n"));
}
