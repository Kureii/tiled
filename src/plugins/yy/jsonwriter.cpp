/*
 * jsonwriter.cpp
 * Copyright 2011-2021, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "jsonwriter.h"

#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>

namespace Yy {

JsonWriter::JsonWriter(QIODevice *device)
    : m_device(device)
{
}

void JsonWriter::writeEndDocument()
{
    Q_ASSERT(m_scopes.isEmpty());
    write('\n');
}

void JsonWriter::writeStartScope(Scope scope)
{
    prepareNewValue();
    write(scope == Object ? '{' : '[');
    m_scopes.push(scope);
    m_newLine = false;
    m_valueWritten = false;
}

void JsonWriter::writeStartScope(Scope scope, const char *name)
{
    writeKey(name);
    write(scope == Object ? '{' : '[');
    m_scopes.push(scope);
    m_newLine = false;
    m_valueWritten = false;
}

void JsonWriter::writeStartScope(Scope scope, const QString &name)
{
    writeKey(name);
    write(scope == Object ? '{' : '[');
    m_scopes.push(scope);
    m_newLine = false;
    m_valueWritten = false;
}

void JsonWriter::writeEndScope(Scope scope)
{
    Q_ASSERT(m_scopes.last() == scope);
    m_scopes.pop();
    if (m_valueWritten) {
        write(m_valueSeparator);    // This is not JSON-conform, but it's what GameMaker does
        writeNewline();
    }
    write(scope == Object ? '}' : ']');
    m_newLine = false;
    m_valueWritten = true;
}

void JsonWriter::writeValue(const QByteArray &value)
{
    Q_ASSERT(m_scopes.last() == Array);
    prepareNewValue();
    write('"');
    write(value);
    write('"');
    m_newLine = false;
    m_valueWritten = true;
}

void JsonWriter::writeValue(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Null:
        writeUnquotedValue("null");
        break;
    case QJsonValue::Bool:
        writeUnquotedValue(value.toBool() ? "true" : "false");
        break;
    case QJsonValue::Double: {
        const double d = value.toDouble();
        if (qIsFinite(d))
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
            writeUnquotedValue(QByteArray::number(d, 'g', QLocale::FloatingPointShortest));
#else
            writeUnquotedValue(QByteArray::number(d));
#endif
        else
            writeUnquotedValue("null"); // +INF || -INF || NaN (see RFC4627#section2.4)
        break;
    }
    case QJsonValue::String:
        writeValue(value.toString());
        break;
    case QJsonValue::Array: {
        writeStartArray();
        const QJsonArray array = value.toArray();
        for (auto v : array) {
            prepareNewLine();
            writeValue(v);
        }
        writeEndArray();
        break;
    }
    case QJsonValue::Object: {
        writeStartObject();
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            writeMember(it.key().toUtf8(), it.value());
        writeEndObject();
        break;
    }
    case QJsonValue::Undefined:
        Q_UNREACHABLE();
        break;
    }
}

void JsonWriter::writeUnquotedValue(const QByteArray &value)
{
    prepareNewValue();
    write(value);
    m_newLine = false;
    m_valueWritten = true;
}

void JsonWriter::writeMember(const QByteArray &key, const char *value)
{
    writeKey(key.data());
    write('"');
    write(value);
    write('"');
    m_newLine = false;
    m_valueWritten = true;
}

void JsonWriter::writeMember(const QByteArray &key, const QByteArray &value)
{
    writeKey(key.data());
    write('"');
    write(value);
    write('"');
    m_newLine = false;
    m_valueWritten = true;
}

void JsonWriter::writeMember(const QByteArray &key, const QJsonValue &value)
{
    writeKey(key.data());
    writeValue(value);
}

void JsonWriter::writeUnquotedMember(const QByteArray &key,
                                     const QByteArray &value)
{
    writeKey(key.data());
    write(value);
    m_newLine = false;
    m_valueWritten = true;
}

/**
 * Quotes the given string, escaping special characters as necessary.
 */
QString JsonWriter::quote(const QString &str)
{
    QString quoted;
    quoted.reserve(str.length() + 2);   // most likely scenario
    quoted.append(QLatin1Char('"'));

    for (const QChar c : str) {
        switch (c.unicode()) {
        case '\\':  quoted.append(QStringLiteral("\\\\"));  break;
        case '"':   quoted.append(QStringLiteral("\\\""));  break;
        case '\n':  quoted.append(QStringLiteral("\\n"));   break;
        default:    quoted.append(c);
        }
    }

    quoted.append(QLatin1Char('"'));
    return quoted;
}

void JsonWriter::prepareNewLine()
{
    if (m_valueWritten) {
        write(m_valueSeparator);
        m_valueWritten = false;
    }
    writeNewline();
}

void JsonWriter::prepareNewValue()
{
    if (!m_valueWritten)
        writeNewline();
    else
        write(m_valueSeparator);
}

void JsonWriter::writeIndent()
{
    for (int level = m_scopes.size(); level; --level)
        write("  ");
}

void JsonWriter::writeNewline()
{
    if (!m_newLine) {
        if (!m_minimize && !m_suppressNewlines) {
            write('\n');
            writeIndent();
        }
        m_newLine = true;
    }
}

void JsonWriter::writeKey(const char *key)
{
    Q_ASSERT(m_scopes.last() == Object);
    prepareNewLine();
    write('"');
    write(key);
    write(m_minimize ? "\":" : "\": ");
}

void JsonWriter::write(const char *bytes, qint64 length)
{
    if (m_device->write(bytes, length) != length)
        m_error = true;
}

} // namespace Yy
