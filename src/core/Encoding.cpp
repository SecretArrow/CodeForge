#include "core/Encoding.h"

#include <QStringConverter>
#include <QStringDecoder>

namespace cf::enc {

static QStringConverter::Encoding toConverter(Id id)
{
    switch (id) {
    case Id::Utf8:        return QStringConverter::Utf8;
    case Id::Utf8Bom:     return QStringConverter::Utf8;
    case Id::Utf16LE:     return QStringConverter::Utf16LE;
    case Id::Utf16BE:     return QStringConverter::Utf16BE;
    case Id::Windows1252:
#ifdef Q_OS_WIN
        return QStringConverter::System;
#else
        return QStringConverter::Latin1;
#endif
    case Id::Latin1:      return QStringConverter::Latin1;
    }
    return QStringConverter::Utf8;
}

Info detect(const QByteArray& data)
{
    const uchar* p = reinterpret_cast<const uchar*>(data.constData());
    const int n = data.size();
    if (n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) { Info i; i.id = Id::Utf8Bom; return i; }
    if (n >= 2 && p[0] == 0xFF && p[1] == 0xFE) { Info i; i.id = Id::Utf16LE; return i; }
    if (n >= 2 && p[0] == 0xFE && p[1] == 0xFF) { Info i; i.id = Id::Utf16BE; return i; }

    // UTF-8 strict validation: a stateless decoder reports errors on invalid input.
    QStringDecoder dec(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    const QString text = dec.decode(data);
    if (!dec.hasError()) { Info i; i.id = Id::Utf8; return i; }
    Info i; i.id = Id::Windows1252;
    return i;
}

QString decode(const QByteArray& data, const Info& e, bool* ok)
{
    if (ok) *ok = true;
    if (e.id == Id::Utf8Bom) {
        QByteArray stripped = data;
        if (stripped.startsWith("\xEF\xBB\xBF")) stripped.remove(0, 3);
        QStringDecoder dec(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
        const QString out = dec.decode(stripped);
        if (ok) *ok = !dec.hasError();
        return out;
    }
    QStringDecoder dec(toConverter(e.id), QStringConverter::Flag::Stateless);
    const QString out = dec.decode(data);
    if (ok) *ok = !dec.hasError();
    return out;
}

QByteArray encode(const QString& text, const Info& e, bool* ok)
{
    if (ok) *ok = true;
    QByteArray out;
    switch (e.id) {
    case Id::Utf8:
        out = text.toUtf8();
        break;
    case Id::Utf8Bom:
        out = QByteArrayLiteral("\xEF\xBB\xBF") + text.toUtf8();
        break;
    case Id::Utf16LE:
        out.resize(text.size() * 2 + 2);
        {
            const char bom[] = {'\xFF', '\xFE'};
            out = QByteArray(bom, 2);
            const char16_t* u = reinterpret_cast<const char16_t*>(text.utf16());
            for (int i = 0; i < text.size(); ++i) {
                out.append(char(u[i] & 0xFF));
                out.append(char(u[i] >> 8));
            }
        }
        break;
    case Id::Utf16BE:
        out = QByteArray();
        {
            const char bom[] = {'\xFE', '\xFF'};
            out = QByteArray(bom, 2);
            const char16_t* u = reinterpret_cast<const char16_t*>(text.utf16());
            for (int i = 0; i < text.size(); ++i) {
                out.append(char(u[i] >> 8));
                out.append(char(u[i] & 0xFF));
            }
        }
        break;
    case Id::Windows1252: {
        QStringEncoder enc(toConverter(e.id), QStringConverter::Flag::Stateless);
        out = enc.encode(text);
        if (ok) *ok = !enc.hasError();
        break;
    }
    case Id::Latin1:
        out = text.toLatin1();
        if (ok) *ok = true;
        break;
    }
    return out;
}

LineEndings detectLineEndings(const QString& text, LineEndings fallback)
{
    qsizetype crlf = 0, lf = 0, cr = 0;
    const qsizetype n = text.size();
    for (qsizetype i = 0; i < n; ++i) {
        const QChar c = text.at(i);
        if (c == u'\r') {
            if (i + 1 < n && text.at(i + 1) == u'\n') { ++crlf; ++i; }
            else ++cr;
        } else if (c == u'\n') {
            ++lf;
        }
    }
    const qsizetype best = qMax(crlf, qMax(lf, cr));
    if (best == 0) return fallback;
    if (best == crlf) return LineEndings::Crlf;
    if (best == cr) return LineEndings::Cr;
    return LineEndings::Lf;
}

}  // namespace cf::enc
