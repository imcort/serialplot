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

// This tells Catch to provide a main() - only do this in one cpp file per executable
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <QSignalSpy>
#include <QBuffer>
#include <QSpinBox>
#include "binarystreamreader.h"
#include "asciireader.h"
#include "framedreader.h"
#include "demoreader.h"
#include "numberformatbox.h"
#include "endiannessbox.h"
#include "binarystreamreadersettings.h"
#include "framedreadersettings.h"

#include "test_helpers.h"

static const int READYREAD_TIMEOUT = 10; // milliseconds

class CapturingSink : public TestSink
{
public:
    bool hasSample = false;
    double lastSample = 0.0;

    void feedIn(const SamplePack& data) override
    {
        if (data.numChannels() > 0 && data.numSamples() > 0)
        {
            hasSample = true;
            lastSample = data.data(0)[data.numSamples()-1];
        }

        TestSink::feedIn(data);
    }
};

TEST_CASE("reading data with BinaryStreamReader", "[reader]")
{
    QBuffer bufferDev;
    BinaryStreamReader bs(&bufferDev);
    bs.enable(true);

    TestSink sink;
    bs.connectSink(&sink);

    REQUIRE(sink._numChannels == 1);
    REQUIRE(sink._hasX == false);

    bufferDev.open(QIODevice::ReadWrite);
    const char data[] = {0x01, 0x02, 0x03, 0x04};
    bufferDev.write(data, 4);
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink.totalFed == 4);
}

TEST_CASE("disabled BinaryStreamReader shouldn't read", "[reader]")
{
    QBuffer bufferDev;
    BinaryStreamReader bs(&bufferDev); // disabled by default

    TestSink sink;
    bs.connectSink(&sink);

    REQUIRE(sink._numChannels == 1);
    REQUIRE(sink._hasX == false);

    bufferDev.open(QIODevice::ReadWrite);
    const char data[] = {0x01, 0x02, 0x03, 0x04};
    bufferDev.write(data, 4);
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    // readyRead isn't signaled because there are no connections to it
    REQUIRE_FALSE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink.totalFed == 0);
}

TEST_CASE("reading data with AsciiReader", "[reader, ascii]")
{
    QBuffer bufferDev;
    AsciiReader reader(&bufferDev);
    reader.enable(true);

    TestSink sink;
    reader.connectSink(&sink);

    REQUIRE(sink._numChannels == 1);
    REQUIRE(sink._hasX == false);

    // inject data to the buffer
    bufferDev.open(QIODevice::ReadWrite);
    bufferDev.write("0,1,3\n0,1,3\n0,1,3\n0,1,3\n");
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink._numChannels == 3);
    REQUIRE(sink._hasX == false);
    REQUIRE(sink.totalFed == 3);
}

TEST_CASE("AsciiReader shouldn't read when disabled", "[reader, ascii]")
{
    QBuffer bufferDev;
    AsciiReader reader(&bufferDev); // disabled by default

    TestSink sink;
    reader.connectSink(&sink);

    REQUIRE(sink._numChannels == 1);
    REQUIRE(sink._hasX == false);

    // inject data to the buffer
    bufferDev.open(QIODevice::ReadWrite);
    bufferDev.write("0,1,3\n0,1,3\n0,1,3\n0,1,3\n");
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE_FALSE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink._numChannels == 1);
    REQUIRE(sink._hasX == false);
    REQUIRE(sink.totalFed == 0);
}

TEST_CASE("reading data with FramedReader", "[reader]")
{
    QBuffer bufferDev;
    FramedReader reader(&bufferDev);
    reader.enable(true);

    TestSink sink;
    reader.connectSink(&sink);

    REQUIRE(sink._numChannels == 1);
    REQUIRE(sink._hasX == false);

    bufferDev.open(QIODevice::ReadWrite);
    const uint8_t data[] = {0xAA, 0xBB, 4, 0x01, 0x02, 0x03, 0x04};
    bufferDev.write((const char*) data, 7);
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink.totalFed == 4);
}

TEST_CASE("FramedReader reads uint24 payload", "[reader]")
{
    QBuffer bufferDev;
    FramedReader reader(&bufferDev);
    reader.enable(true);

    CapturingSink sink;
    reader.connectSink(&sink);

    auto* settings = dynamic_cast<FramedReaderSettings*>(reader.settingsWidget());
    REQUIRE(settings != nullptr);
    auto* nfBox = settings->findChild<NumberFormatBox*>("nfBox");
    REQUIRE(nfBox != nullptr);
    nfBox->setSelection(NumberFormat_uint24);

    bufferDev.open(QIODevice::ReadWrite);
    const uint8_t data[] = {0xAA, 0xBB, 3, 0x56, 0x34, 0x12};
    bufferDev.write((const char*) data, 6);
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink.totalFed == 1);
    REQUIRE(sink.hasSample);
    REQUIRE(sink.lastSample == 1193046.0);
}

TEST_CASE("FramedReader reads signed int24 payload", "[reader]")
{
    QBuffer bufferDev;
    FramedReader reader(&bufferDev);
    reader.enable(true);

    CapturingSink sink;
    reader.connectSink(&sink);

    auto* settings = dynamic_cast<FramedReaderSettings*>(reader.settingsWidget());
    REQUIRE(settings != nullptr);
    auto* nfBox = settings->findChild<NumberFormatBox*>("nfBox");
    REQUIRE(nfBox != nullptr);
    nfBox->setSelection(NumberFormat_int24);

    bufferDev.open(QIODevice::ReadWrite);
    const uint8_t data[] = {0xAA, 0xBB, 3, 0xFF, 0xFF, 0xFF};
    bufferDev.write((const char*) data, 6);
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink.totalFed == 1);
    REQUIRE(sink.hasSample);
    REQUIRE(sink.lastSample == -1.0);
}

TEST_CASE("FramedReader skips leading payload bytes", "[reader]")
{
    QBuffer bufferDev;
    FramedReader reader(&bufferDev);
    reader.enable(true);

    CapturingSink sink;
    reader.connectSink(&sink);

    auto* settings = dynamic_cast<FramedReaderSettings*>(reader.settingsWidget());
    REQUIRE(settings != nullptr);
    auto* skipBox = settings->findChild<QSpinBox*>("spSkipBytes");
    REQUIRE(skipBox != nullptr);
    skipBox->setValue(1);

    bufferDev.open(QIODevice::ReadWrite);
    const uint8_t data[] = {0xAA, 0xBB, 2, 0x99, 0x07};
    bufferDev.write((const char*) data, 5);
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink.totalFed == 1);
    REQUIRE(sink.hasSample);
    REQUIRE(sink.lastSample == 7.0);
}

TEST_CASE("FramedReader shouldn't read when disabled", "[reader]")
{
    QBuffer bufferDev;
    FramedReader reader(&bufferDev);

    TestSink sink;
    reader.connectSink(&sink);

    REQUIRE(sink._numChannels == 1);
    REQUIRE(sink._hasX == false);

    bufferDev.open(QIODevice::ReadWrite);
    const uint8_t data[] = {0xAA, 0xBB, 4, 0x01, 0x02, 0x03, 0x04};
    bufferDev.write((const char*) data, 7);
    bufferDev.seek(0);

    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE_FALSE(spy.wait(READYREAD_TIMEOUT));
    REQUIRE(sink.totalFed == 0);
}

TEST_CASE("Generating data with DemoReader", "[reader, demo]")
{
    QBuffer bufferDev;          // not actually used
    DemoReader demoReader(&bufferDev);
    demoReader.enable(true);

    TestSink sink;
    demoReader.connectSink(&sink);
    REQUIRE(sink._numChannels == 1);
    REQUIRE(sink._hasX == false);

    // we need to wait somehow, we are not actually looking for signals
    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE_FALSE(spy.wait(1000)); // we need some time for demoreader to produce data
    REQUIRE(sink.totalFed >= 9);
}

TEST_CASE("DemoReader shouldn't generate data when paused", "[reader, demo]")
{
    QBuffer bufferDev;          // not actually used
    DemoReader demoReader(&bufferDev); // paused by default

    TestSink sink;
    demoReader.connectSink(&sink);
    REQUIRE(sink._numChannels == 1);

    // we need to wait somehow, we are not actually looking for signals
    QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
    REQUIRE_FALSE(spy.wait(1000)); // we need some time for demoreader to produce data
    REQUIRE(sink.totalFed == 0);
}

// Note: this is added because `QApplication` must be created for widgets
#include <QApplication>
int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    int result = Catch::Session().run( argc, argv );

    return result;
}

// ---------------------------------------------------------------------------
// 24 bit formats: exercise the sign extension and both byte orders at the
// boundaries of the 24 bit range.
// ---------------------------------------------------------------------------

struct Sample24Case
{
    const char* name;
    NumberFormat format;
    Endianness endianness;
    uint8_t bytes[3];
    double expected;
};

static const Sample24Case sample24Cases[] = {
    // unsigned
    {"uint24 LE zero",        NumberFormat_uint24, LittleEndian, {0x00, 0x00, 0x00}, 0.0},
    {"uint24 LE max",         NumberFormat_uint24, LittleEndian, {0xFF, 0xFF, 0xFF}, 16777215.0},
    {"uint24 LE sign bit",    NumberFormat_uint24, LittleEndian, {0x00, 0x00, 0x80}, 8388608.0},
    {"uint24 BE one",         NumberFormat_uint24, BigEndian,    {0x00, 0x00, 0x01}, 1.0},
    {"uint24 BE max",         NumberFormat_uint24, BigEndian,    {0xFF, 0xFF, 0xFF}, 16777215.0},
    {"uint24 BE 0x123456",    NumberFormat_uint24, BigEndian,    {0x12, 0x34, 0x56}, 1193046.0},
    {"uint24 LE 0x123456",    NumberFormat_uint24, LittleEndian, {0x56, 0x34, 0x12}, 1193046.0},
    // signed: the 0x80/0x7F boundary is where sign extension can go wrong
    {"int24 LE zero",         NumberFormat_int24,  LittleEndian, {0x00, 0x00, 0x00}, 0.0},
    {"int24 LE -1",           NumberFormat_int24,  LittleEndian, {0xFF, 0xFF, 0xFF}, -1.0},
    {"int24 LE max",          NumberFormat_int24,  LittleEndian, {0xFF, 0xFF, 0x7F}, 8388607.0},
    {"int24 LE min",          NumberFormat_int24,  LittleEndian, {0x00, 0x00, 0x80}, -8388608.0},
    {"int24 LE -2",           NumberFormat_int24,  LittleEndian, {0xFE, 0xFF, 0xFF}, -2.0},
    {"int24 LE 0x000080",     NumberFormat_int24,  LittleEndian, {0x80, 0x00, 0x00}, 128.0},
    {"int24 BE -1",           NumberFormat_int24,  BigEndian,    {0xFF, 0xFF, 0xFF}, -1.0},
    {"int24 BE max",          NumberFormat_int24,  BigEndian,    {0x7F, 0xFF, 0xFF}, 8388607.0},
    {"int24 BE min",          NumberFormat_int24,  BigEndian,    {0x80, 0x00, 0x00}, -8388608.0},
    {"int24 BE 0x000080",     NumberFormat_int24,  BigEndian,    {0x00, 0x00, 0x80}, 128.0},
};

TEST_CASE("BinaryStreamReader reads 24 bit samples", "[reader, int24]")
{
    for (const auto& c : sample24Cases)
    {
        INFO(c.name);

        QBuffer bufferDev;
        BinaryStreamReader reader(&bufferDev);
        reader.enable(true);

        CapturingSink sink;
        reader.connectSink(&sink);

        auto* settings = dynamic_cast<BinaryStreamReaderSettings*>(reader.settingsWidget());
        REQUIRE(settings != nullptr);
        auto* nfBox = settings->findChild<NumberFormatBox*>("nfBox");
        REQUIRE(nfBox != nullptr);
        nfBox->setSelection(c.format);
        auto* endiBox = settings->findChild<EndiannessBox*>("endiBox");
        REQUIRE(endiBox != nullptr);
        endiBox->setSelection(c.endianness);

        bufferDev.open(QIODevice::ReadWrite);
        bufferDev.write((const char*) c.bytes, 3);
        bufferDev.seek(0);

        QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
        REQUIRE(spy.wait(READYREAD_TIMEOUT));

        REQUIRE(sink.totalFed == 1);
        REQUIRE(sink.hasSample);
        REQUIRE(sink.lastSample == c.expected);
    }
}

TEST_CASE("FramedReader reads 24 bit samples", "[reader, int24]")
{
    for (const auto& c : sample24Cases)
    {
        INFO(c.name);

        QBuffer bufferDev;
        FramedReader reader(&bufferDev);
        reader.enable(true);

        CapturingSink sink;
        reader.connectSink(&sink);

        auto* settings = dynamic_cast<FramedReaderSettings*>(reader.settingsWidget());
        REQUIRE(settings != nullptr);
        auto* nfBox = settings->findChild<NumberFormatBox*>("nfBox");
        REQUIRE(nfBox != nullptr);
        nfBox->setSelection(c.format);
        auto* endiBox = settings->findChild<EndiannessBox*>("endiBox");
        REQUIRE(endiBox != nullptr);
        endiBox->setSelection(c.endianness);

        // default frame: 2 byte sync word "AA BB", 1 byte size, 3 byte payload,
        // checksum disabled by default
        const uint8_t frame[6] = {0xAA, 0xBB, 3, c.bytes[0], c.bytes[1], c.bytes[2]};

        bufferDev.open(QIODevice::ReadWrite);
        bufferDev.write((const char*) frame, 6);
        bufferDev.seek(0);

        QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
        REQUIRE(spy.wait(READYREAD_TIMEOUT));

        REQUIRE(sink.totalFed == 1);
        REQUIRE(sink.hasSample);
        REQUIRE(sink.lastSample == c.expected);
    }
}

/// Captures the last sample of every channel of the last fed pack
class RowCapturingSink : public TestSink
{
public:
    QVector<double> lastRow;

    void feedIn(const SamplePack& data) override
    {
        lastRow.clear();
        if (data.numSamples() > 0)
        {
            for (unsigned ci = 0; ci < data.numChannels(); ci++)
            {
                lastRow << data.data(ci)[data.numSamples()-1];
            }
        }

        TestSink::feedIn(data);
    }
};

TEST_CASE("24 bit samples of several channels are not interleaved wrongly", "[reader, int24]")
{
    // channel 0: 1, channel 1: -1 -- with 3 byte samples a wrong stride shows up here
    const uint8_t payload[] = {0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF};

    SECTION("BinaryStreamReader")
    {
        QBuffer bufferDev;
        BinaryStreamReader reader(&bufferDev);
        reader.enable(true);

        RowCapturingSink sink;
        reader.connectSink(&sink);

        auto* settings = dynamic_cast<BinaryStreamReaderSettings*>(reader.settingsWidget());
        REQUIRE(settings != nullptr);
        settings->findChild<NumberFormatBox*>("nfBox")->setSelection(NumberFormat_int24);
        settings->findChild<QSpinBox*>("spNumOfChannels")->setValue(2);

        bufferDev.open(QIODevice::ReadWrite);
        bufferDev.write((const char*) payload, 6);
        bufferDev.seek(0);

        QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
        REQUIRE(spy.wait(READYREAD_TIMEOUT));

        REQUIRE(sink.totalFed == 1);
        REQUIRE(sink.lastRow.size() == 2);
        REQUIRE(sink.lastRow[0] == 1.0);
        REQUIRE(sink.lastRow[1] == -1.0);
    }

    SECTION("FramedReader")
    {
        QBuffer bufferDev;
        FramedReader reader(&bufferDev);
        reader.enable(true);

        RowCapturingSink sink;
        reader.connectSink(&sink);

        auto* settings = dynamic_cast<FramedReaderSettings*>(reader.settingsWidget());
        REQUIRE(settings != nullptr);
        settings->findChild<NumberFormatBox*>("nfBox")->setSelection(NumberFormat_int24);
        settings->findChild<QSpinBox*>("spNumOfChannels")->setValue(2);

        // sync word + payload size (6) + payload
        const uint8_t frame[] = {0xAA, 0xBB, 6,
                                 0x01, 0x00, 0x00, 0xFF, 0xFF, 0xFF};

        bufferDev.open(QIODevice::ReadWrite);
        bufferDev.write((const char*) frame, sizeof(frame));
        bufferDev.seek(0);

        QSignalSpy spy(&bufferDev, SIGNAL(readyRead()));
        REQUIRE(spy.wait(READYREAD_TIMEOUT));

        REQUIRE(sink.totalFed == 1);
        REQUIRE(sink.lastRow.size() == 2);
        REQUIRE(sink.lastRow[0] == 1.0);
        REQUIRE(sink.lastRow[1] == -1.0);
    }
}
