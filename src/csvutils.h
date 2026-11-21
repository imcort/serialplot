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

#ifndef CSVUTILS_H
#define CSVUTILS_H

#include <QString>

/// Wraps a value in quotes when the CSV would otherwise be ambiguous
inline QString csvField(const QString& value, const QString& separator)
{
    const bool needsQuotes =
        value.contains(separator) ||
        value.contains('"') ||
        value.contains('\n') ||
        value.contains('\r') ||
        (!value.isEmpty() && (value.front().isSpace() || value.back().isSpace()));

    if (!needsQuotes) return value;

    QString escaped = value;
    escaped.replace('"', "\"\"");
    return QString('"') + escaped + QString('"');
}

#endif // CSVUTILS_H
