#include "core/DocumentManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextDocument>

#include "core/AppPaths.h"
#include "core/FileUtils.h"
#include "core/Logger.h"
#include "security/Crypto.h"
#include "security/KeyStore.h"
#include "security/SecureBuffer.h"
#include "settings/SettingsManager.h"

namespace cf {

using sec::Crypto;
using sec::KeyStore;
using sec::SecureBuffer;

DocumentManager& DocumentManager::instance()
{
    static DocumentManager s;
    return s;
}

static const char* kRecoveryKeyName = "recovery-v1";

DocumentManager::DocumentManager(QObject* parent)
    : QObject(parent)
{
    configureAutosave();
    connect(&m_recoveryTimer, &QTimer::timeout, this, &DocumentManager::writeRecoverySnapshots);
    m_recoveryTimer.setInterval(qMax(10, SettingsManager::instance().getInt(QStringLiteral("security.recoveryIntervalSec"))) * 1000);
    m_recoveryTimer.start();
}

void DocumentManager::configureAutosave()
{
    const QString mode = SettingsManager::instance().getString(QStringLiteral("files.autosave"));
    if (mode == QLatin1String("afterDelay")) {
        m_autosaveTimer.setSingleShot(true);
        m_autosaveTimer.setInterval(SettingsManager::instance().getInt(QStringLiteral("files.autosaveDelayMs")));
        connect(&m_autosaveTimer, &QTimer::timeout, this, [this]() {
            saveAllModified();
        });
        // Restart debounce on any dirty change.
        for (const QPointer<TextDocument>& d : m_documents)
            if (d) connect(d, &TextDocument::dirtyChanged, this, [this](bool dirty) {
                if (dirty && SettingsManager::instance().getString(QStringLiteral("files.autosave")) == QLatin1String("afterDelay"))
                    m_autosaveTimer.start();
                });
        m_autosaveTimer.start();
    } else {
        m_autosaveTimer.stop();
    }
}

void DocumentManager::autosaveOnFocusChange()
{
    if (SettingsManager::instance().getString(QStringLiteral("files.autosave")) == QLatin1String("onFocusChange"))
        saveAllModified();
}

void DocumentManager::connectDoc(TextDocument* doc)
{
    connect(doc, &TextDocument::dirtyChanged, this, [this, doc](bool dirty) {
        emit dirtyChanged(doc, dirty);
        if (dirty && SettingsManager::instance().getString(QStringLiteral("files.autosave")) == QLatin1String("afterDelay"))
            m_autosaveTimer.start();
    });
}

TextDocument* DocumentManager::openDocument(const QString& pathIn, bool readOnly, QString* error)
{
    const QString path = QDir::fromNativeSeparators(QFileInfo(pathIn).absoluteFilePath());

    if (TextDocument* existing = documentByPath(path)) {
        if (error) error->clear();
        return existing;
    }

    QFileInfo info(path);
    if (!info.exists()) {
        if (error) *error = QStringLiteral("The file does not exist:\n%1").arg(path);
        return nullptr;
    }
    if (!info.isFile()) {
        if (error) *error = QStringLiteral("Not a file:\n%1").arg(path);
        return nullptr;
    }
    if (!info.isReadable()) {
        if (error) *error = QStringLiteral("Permission denied:\n%1").arg(path);
        return nullptr;
    }

    // Large-file policy.
    const qint64 warnBytes = qint64(SettingsManager::instance().getInt(QStringLiteral("files.largeFileWarningMB"))) * 1024 * 1024;
    bool asReadOnly = readOnly;
    if (warnBytes > 0 && info.size() > warnBytes && promptLargeFile) {
        const LargeFileDecision d = promptLargeFile(info.size());
        if (d == LargeFileDecision::Cancel) { if (error) error->clear(); return nullptr; }
        if (d == LargeFileDecision::ReadOnly) asReadOnly = true;
    }

    // Binary guard (fast header check).
    QString binErr;
    if (!fs::isTextFileByHead(path, &binErr)) {
        if (error) *error = binErr;
        return nullptr;
    }

    auto* doc = new TextDocument(this);
    doc->setFilePath(path);
    QString loadErr;
    if (!doc->load(&loadErr)) {
        if (error) *error = loadErr;
        doc->deleteLater();
        return nullptr;
    }
    doc->setReadOnly(asReadOnly);
    doc->snapshotDiskState();
    m_documents.append(doc);
    connectDoc(doc);
    CF_LOG_INFO(QStringLiteral("Document: opened %1 (%2 bytes)").arg(path).arg(info.size()));
    emit documentOpened(doc);
    if (error) error->clear();
    return doc;
}

TextDocument* DocumentManager::createUntitled()
{
    auto* doc = new TextDocument(this);
    m_documents.append(doc);
    connectDoc(doc);
    emit documentOpened(doc);
    return doc;
}

QList<TextDocument*> DocumentManager::documents() const
{
    QList<TextDocument*> out;
    for (const QPointer<TextDocument>& d : m_documents)
        if (d) out.append(d.data());
    return out;
}

TextDocument* DocumentManager::documentByPath(const QString& path) const
{
    const QString norm = QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
    for (const QPointer<TextDocument>& d : m_documents)
        if (d && d->filePath() == norm) return d.data();
    return nullptr;
}

TextDocument* DocumentManager::documentById(const QString& docId) const
{
    for (const QPointer<TextDocument>& d : m_documents)
        if (d && d->docId() == docId) return d.data();
    return nullptr;
}

bool DocumentManager::saveDocument(TextDocument* doc, QString* error)
{
    if (!doc) return false;
    if (!doc->isDirty() && doc->existsOnDisk()) return true;

    if (doc->isUntitled() && promptSaveBeforeClose) {
        // Caller decides how to get a path (Save As dialog happens via saveDocumentAs).
        // Treated by UI: MainWindow triggers Save As flow.
    }
    const bool ok = doc->save(error);
    if (ok) { clearRecoveryFor(doc); emit documentSaved(doc); }
    return ok;
}

bool DocumentManager::saveDocumentAs(TextDocument* doc, const QString& path, QString* error)
{
    const bool ok = doc->saveAs(path, error);
    if (ok) { clearRecoveryFor(doc); emit documentSaved(doc); }
    return ok;
}

int DocumentManager::saveAllModified()
{
    int saved = 0;
    for (const QPointer<TextDocument>& d : m_documents) {
        if (!d || !d->isDirty() || d->isUntitled() || d->isReadOnly()) continue;
        QString err;
        if (d->save(&err)) {
            ++saved;
            clearRecoveryFor(d);
            emit documentSaved(d);
        } else {
            CF_LOG_ERROR(QStringLiteral("Autosave failed for %1: %2").arg(d->filePath(), err));
        }
    }
    return saved;
}

bool DocumentManager::closeDocument(TextDocument* doc)
{
    if (!doc) return true;

    if (doc->isDirty() && !doc->isReadOnly() && promptSaveBeforeClose) {
        const SaveDecision decision = promptSaveBeforeClose(doc);
        if (decision == SaveDecision::Cancel) return false;
        if (decision == SaveDecision::Save) {
            if (doc->isUntitled()) {
                // The UI layer must run Save As; ask via the same callback contract:
                // saving untitled without path is handled by MainWindow before calling close.
                if (showError) showError(QStringLiteral("Cannot save untitled document without a file path."));
                return false;
            }
            QString err;
            if (!doc->save(&err)) {
                if (showError) showError(err);
                return false;
            }
        }
    }

    const QString id = doc->docId();
    clearRecoveryFor(doc);
    m_documents.removeAll(doc);
    emit documentClosed(id);
    doc->deleteLater();
    return true;
}

void DocumentManager::closeAllDocuments()
{
    // Only used at shutdown where session already handled prompts.
    for (const QPointer<TextDocument>& d : m_documents) {
        if (!d) continue;
        emit documentClosed(d->docId());
        d->deleteLater();
    }
    m_documents.clear();
}

// ---------- recovery ----------

QString DocumentManager::recoveryFilePath(const QString& docId) const
{
    QByteArray h = QCryptographicHash::hash(docId.toUtf8(), QCryptographicHash::Sha256).toHex();
    return paths::recoveryDir() + QStringLiteral("/%1.cfb").arg(QString::fromLatin1(h.left(24)));
}

void DocumentManager::writeRecoveryFor(TextDocument* doc)
{
    if (!doc || !doc->isDirty()) return;
    SettingsManager& s = SettingsManager::instance();
    if (!s.getBool(QStringLiteral("security.recoveryEnabled"))) return;

    // Remove stale snapshot for clean documents.
    if (!doc->isDirty()) {
        QFile::remove(recoveryFilePath(doc->docId()));
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("docId"), doc->docId());
    payload.insert(QStringLiteral("path"), doc->filePath());
    payload.insert(QStringLiteral("untitledId"), doc->isUntitled() ? doc->docId() : QString());
    payload.insert(QStringLiteral("title"), doc->displayName());
    payload.insert(QStringLiteral("encoding"), int(doc->encoding().id));
    payload.insert(QStringLiteral("eol"), int(doc->lineEndings()));
    payload.insert(QStringLiteral("text"), doc->document()->toPlainText());
    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    if (!s.getBool(QStringLiteral("security.encryptRecovery"))) {
        // SECURITY: never store plaintext content on disk. Encryption is
        // mandatory for snapshots that contain document text.
        CF_LOG_WARNING(QStringLiteral("Recovery: skipped (encryption disabled) for %1").arg(doc->docId()));
        return;
    }

    SecureBuffer key = KeyStore::getOrCreateKey(QString::fromLatin1(kRecoveryKeyName));
    if (key.isEmpty()) return;

    QByteArray nonce(Crypto::kNonceSize, Qt::Uninitialized);
    if (!Crypto::randomBytes(reinterpret_cast<quint8*>(nonce.data()), Crypto::kNonceSize)) return;

    QByteArray cipher;
    // AAD intentionally empty: the "CFRB1" magic header already binds this
    // blob to the recovery context, and an empty AAD keeps the CNG and
    // OpenSSL backends byte-identical on every Windows SDK revision.
    if (!Crypto::gcmEncrypt(key.data(), reinterpret_cast<const quint8*>(nonce.constData()), json,
                            QByteArray(), cipher))
        return;

    QByteArray blob;
    blob.append(QByteArrayLiteral("CFRB1"));
    blob.append(nonce);
    blob.append(cipher);
    fs::writeAllAtomic(recoveryFilePath(doc->docId()), blob);
}

void DocumentManager::writeRecoverySnapshots()
{
    for (const QPointer<TextDocument>& d : m_documents)
        if (d && d->isDirty()) writeRecoveryFor(d.data());
}

void DocumentManager::clearRecoveryFor(TextDocument* doc)
{
    if (doc) QFile::remove(recoveryFilePath(doc->docId()));
}

QList<DocumentManager::RecoveryEntry> DocumentManager::listRecoveryEntries() const
{
    QList<RecoveryEntry> out;
    SecureBuffer key = KeyStore::getOrCreateKey(QString::fromLatin1(kRecoveryKeyName));
    if (key.isEmpty()) return out;

    const QDir recDir(paths::recoveryDir());
    for (const QFileInfo& fi : recDir.entryInfoList({ QStringLiteral("*.cfb") }, QDir::Files)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray blob = f.readAll();
        if (blob.size() < 4 + Crypto::kNonceSize + Crypto::kTagSize) continue;
        if (blob.left(5) != QByteArrayLiteral("CFRB1")) continue;

        const QByteArray nonce = blob.mid(5, Crypto::kNonceSize);
        QByteArray plain;
        if (!Crypto::gcmDecrypt(key.data(), reinterpret_cast<const quint8*>(nonce.constData()),
                                blob.mid(5 + Crypto::kNonceSize), QByteArray(), plain))
            continue;
        const QJsonObject payload = QJsonDocument::fromJson(plain).object();
        RecoveryEntry e;
        e.docId = payload.value(QStringLiteral("docId")).toString();
        e.path = payload.value(QStringLiteral("path")).toString();
        e.untitledId = payload.value(QStringLiteral("untitledId")).toString();
        e.title = payload.value(QStringLiteral("title")).toString();
        if (!e.docId.isEmpty()) out.append(e);
    }
    return out;
}

bool DocumentManager::restoreRecoveryInto(TextDocument* doc)
{
    if (!doc) return false;
    SecureBuffer key = KeyStore::getOrCreateKey(QString::fromLatin1(kRecoveryKeyName));
    if (key.isEmpty()) return false;

    QFile f(recoveryFilePath(doc->docId()));
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray blob = f.readAll();
    if (blob.size() < 5 + Crypto::kNonceSize + Crypto::kTagSize) return false;

    const QByteArray nonce = blob.mid(5, Crypto::kNonceSize);
    QByteArray plain;
    if (!Crypto::gcmDecrypt(key.data(), reinterpret_cast<const quint8*>(nonce.constData()),
                            blob.mid(5 + Crypto::kNonceSize), QByteArray(), plain))
        return false;

    const QJsonObject payload = QJsonDocument::fromJson(plain).object();
    doc->setEncoding(enc::Info{});  // default; real encoding applied by loader for files
    doc->setLineEndings(LineEndings(payload.value(QStringLiteral("eol")).toInt()));
    doc->document()->setPlainText(payload.value(QStringLiteral("text")).toString());
    doc->markClean();
    return true;
}

void DocumentManager::discardRecoveryEntry(const QString& docId)
{
    QFile::remove(recoveryFilePath(docId));
}

// ---------- external changes ----------

void DocumentManager::checkExternalChanges()
{
    for (const QPointer<TextDocument>& d : m_documents) {
        if (!d || d->filePath().isEmpty()) continue;

        if (!QFileInfo::exists(d->filePath())) {
            emit d->deletedOnDisk();   // UI decides messaging; buffer kept in memory
            continue;
        }
        if (d->hasExternalChange()) {
            if (promptExternalChange) {
                const int choice = promptExternalChange(d.data());
                switch (choice) {
                case 0:  // Reload
                    d->reloadFromDisk();
                    d->snapshotDiskState();
                    break;
                case 1:  // Keep current
                    d->snapshotDiskState();   // don't nag again for same state
                    emit externalChangeDetected(d.data());
                    break;
                case 2:  // Compare
                    emit requestCompareWithDisk(d.data());
                    break;
                default:
                    break;
                }
            }
        }
    }
}

}  // namespace cf
