/*
  Copyright © 2017 Hasan Yavuz Özderya

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

#include <QtGlobal>

#include "ringbuffer.h"

RingBuffer::RingBuffer(unsigned n)
{
    _size = n;
    data = new double[_size]();
    headIndex = 0;
    hasData = false;

    limInvalid = false;
    limCache = {0, 0};
}

RingBuffer::~RingBuffer()
{
    delete[] data;
}

unsigned RingBuffer::size() const
{
    return _size;
}

double RingBuffer::sample(unsigned i) const
{
    unsigned index = headIndex + i;
    if (index >= _size) index -= _size;
    return data[index];
}

Range RingBuffer::limits() const
{
    if (limInvalid) updateLimits();
    return limCache;
}

void RingBuffer::resize(unsigned n)
{
    Q_ASSERT(n != _size);

    int offset = (int) n - (int) _size;
    if (offset == 0) return;

    double* newData = new double[n];

    // move data to new array
    int fill_start = offset > 0 ? offset : 0;

    for (int i = fill_start; i < int(n); i++)
    {
        newData[i] = sample(i - offset);
    }

    // Fill the newly added beginning with the oldest sample instead of zero.
    // This keeps the automatic Y scaling from being stretched down to zero
    // when the buffer is enlarged after data has been received.
    if (fill_start > 0)
    {
        const double fillValue = newData[fill_start];
        for (int i = 0; i < fill_start; i++)
        {
            newData[i] = fillValue;
        }
    }

    // data is ready, clean up and re-point
    delete[] data;
    data = newData;
    headIndex = 0;
    _size = n;

    // invalidate bounding rectangle
    limInvalid = true;
}

void RingBuffer::addSamples(double* samples, unsigned n)
{
    if (n == 0) return;

    // Before the first samples arrive the whole buffer is zero. Fill it with
    // the first sample value so that auto scaling isn't stretched down to
    // zero and the plot starts from a flat line at the first real value.
    if (!hasData)
    {
        for (unsigned i = 0; i < _size; i++)
        {
            data[i] = samples[0];
        }
        headIndex = 0;
        hasData = true;
    }

    unsigned shift = n;
    if (shift < _size)
    {
        unsigned x = _size - headIndex; // distance of `head` to end

        if (shift <= x) // there is enough room at the end of array
        {
            for (unsigned i = 0; i < shift; i++)
            {
                data[i+headIndex] = samples[i];
            }

            if (shift == x) // we used all the room at the end
            {
                headIndex = 0;
            }
            else
            {
                headIndex += shift;
            }
        }
        else // there isn't enough room
        {
            for (unsigned i = 0; i < x; i++) // fill the end part
            {
                data[i+headIndex] = samples[i];
            }
            for (unsigned i = 0; i < (shift-x); i++) // continue from the beginning
            {
                data[i] = samples[i+x];
            }
            headIndex = shift-x;
        }
    }
    else // number of new samples equal or bigger than current size (doesn't fit)
    {
        int x = shift - _size;
        for (unsigned i = 0; i < _size; i++)
        {
            data[i] = samples[i+x];
        }
        headIndex = 0;
    }

    // invalidate cache
    limInvalid = true;
}

void RingBuffer::clear()
{
    for (unsigned i=0; i < _size; i++)
    {
        data[i] = 0.;
    }

    headIndex = 0;
    hasData = false;
    limCache = {0, 0};
    limInvalid = false;
}

void RingBuffer::updateLimits() const
{
    limCache.start = data[0];
    limCache.end = data[0];

    for (unsigned i = 0; i < _size; i++)
    {
        if (data[i] > limCache.end)
        {
            limCache.end = data[i];
        }
        else if (data[i] < limCache.start)
        {
            limCache.start = data[i];
        }
    }

    limInvalid = false;
}
