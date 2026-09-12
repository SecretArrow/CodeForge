#pragma once
// Text encoding detection / conversion (UTF-8, UTF-8 BOM, UTF-16 LE/BE,
// Windows-1252 / Latin-1) built on Qt's QStringConverter.
#include <QByteArray>
#include <QString>

#include "core/Common.h"

namespace cf::enc {

enum class Id { Utf8, Utf8Bom, Utf16LE, Utf16BE, Windows1252, Latin1 };

struct Info {
    Id id = Id::Utf8;
    QString label() const
    {
        switch (id) {
        case Id::Utf8:        return QStringLiteral("UTF-8");
        case Id::Utf8Bom:     return QStringLiteral("UTF-8 BOM");
        case Id::Utf16LE:     return QStringLiteral("UTF-16 LE");
        case Id::Utf16BE:     return QStringLiteral("UTF-16 BE");
        case Id::Windows1252: return QStringLiteral("Windows-1252");
        case Id::Latin1:      return QStringLiteral("Latin-1");
        }
        return QStringLiteral("UTF-8");
    }
    bool isUnicode() const { return id != Id::Windows1252 && id != Id::Latin1; }
    bool operator==(const Info& o) const { return id == o.id; }
};

// Detect encoding from raw bytes: BOM first, then UTF-8 validity check,
// falls back to Windows-1252 (single-byte, never fails).
Info detect(const QByteArray& data);

// Decode raw bytes to text. *ok set to false when substitution occurred.
QString decode(const QByteArray& data, const Info& e, bool* ok = nullptr);

// Encode text to raw bytes in the given encoding.
QByteArray encode(const QString& text, const Info& e, bool* ok = nullptr);

// Detect dominant line ending of a text buffer.
LineEndings detectLineEndings(const QString& text, LineEndings fallback = LineEndings::Lf);

}  // namespace cf::enc
