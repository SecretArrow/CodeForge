#include "ai/AssistantPanel.h"

#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "core/Logger.h"
#include "security/Crypto.h"
#include "security/KeyStore.h"
#include "settings/SettingsManager.h"
#include "themes/ThemeManager.h"
#include "ui/Icons.h"

namespace cf {

namespace {

constexpr char kApiKeyStore[] = "ai-apikey-v1";

// XOR-less tamper-evident storage: AES-256-GCM under a KeyStore key,
// stored base64(nonce|tag|ciphertext) in settings. Never logged, never synced.
QString loadStoredApiKey()
{
    const QByteArray blob = QByteArray::fromBase64(
        SettingsManager::instance().getString(QStringLiteral("ai.apiKeyEnc")).toUtf8());
    if (blob.size() < Crypto::kNonceSize + Crypto::kTagSize)
        return QString();
    const sec::SecureBuffer key = sec::KeyStore::getOrCreateKey(QLatin1String(kApiKeyStore));
    if (key.size() != Crypto::kKeySize)
        return QString();
    const QByteArray nonce(reinterpret_cast<const char*>(blob.constData()), Crypto::kNonceSize);
    const QByteArray cipherWithTag(blob.constData() + Crypto::kNonceSize,
                                   blob.size() - Crypto::kNonceSize);
    QByteArray plain;
    if (!Crypto::gcmDecrypt(key.data(), reinterpret_cast<const quint8*>(nonce.constData()),
                            cipherWithTag, QByteArray(), plain))
        return QString();
    return QString::fromUtf8(plain);
}

bool saveStoredApiKey(const QString& key)
{
    sec::SecureBuffer keyBuf = sec::KeyStore::getOrCreateKey(QLatin1String(kApiKeyStore));
    if (keyBuf.size() != Crypto::kKeySize)
        return false;
    quint8 nonceRaw[Crypto::kNonceSize];
    if (!Crypto::randomBytes(nonceRaw, Crypto::kNonceSize))
        return false;
    const QByteArray plain = key.toUtf8();
    QByteArray out;   // ciphertext || tag
    if (!Crypto::gcmEncrypt(keyBuf.data(), nonceRaw, plain, QByteArray(), out))
        return false;
    QByteArray blob(reinterpret_cast<const char*>(nonceRaw), Crypto::kNonceSize);
    blob.append(out);
    SettingsManager::instance().set(QStringLiteral("ai.apiKeyEnc"),
                                    QString::fromUtf8(blob.toBase64()));
    return true;
}

bool isLocalEndpoint(const QString& url)
{
    const QUrl u(url);
    const QString host = u.host();
    return host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1")
           || host == QLatin1String("[::1]") || host == QLatin1String("::1");
}

QString escapeHtml(QString s)
{
    s.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    s.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    s.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return s;
}

}  // namespace

AssistantPanel::AssistantPanel(QWidget* parent)
    : QWidget(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    buildUi();
    loadSettingsToUi();
}

void AssistantPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    m_privacy = new QLabel(this);
    m_privacy->setWordWrap(true);
    layout->addWidget(m_privacy);

    auto* endpointRow = new QHBoxLayout;
    m_endpoint = new QLineEdit(this);
    m_endpoint->setPlaceholderText(QStringLiteral("http://localhost:11434/v1"));
    m_modelsBtn = new QToolButton(this);
    m_modelsBtn->setText(tr("Models"));
    endpointRow->addWidget(new QLabel(tr("Endpoint:"), this));
    endpointRow->addWidget(m_endpoint, 1);
    endpointRow->addWidget(m_modelsBtn);
    layout->addLayout(endpointRow);

    auto* modelRow = new QHBoxLayout;
    m_model = new QComboBox(this);
    m_model->setEditable(true);
    m_apiKey = new QLineEdit(this);
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_apiKey->setPlaceholderText(tr("API key (optional; stored encrypted, local-only)"));
    modelRow->addWidget(new QLabel(tr("Model:"), this));
    modelRow->addWidget(m_model, 1);
    modelRow->addWidget(m_apiKey, 2);
    layout->addLayout(modelRow);

    m_transcript = new QTextBrowser(this);
    m_transcript->setOpenExternalLinks(true);
    layout->addWidget(m_transcript, 1);

    auto* actions = new QHBoxLayout;
    const QStringList kinds = { tr("Explain"), tr("Refactor"), tr("Fix"), tr("Tests") };
    for (const QString& k : kinds) {
        auto* b = new QToolButton(this);
        b->setText(k);
        connect(b, &QToolButton::clicked, this, [this, k]() { onAction(k); });
        actions->addWidget(b);
    }
    actions->addStretch(1);
    layout->addLayout(actions);

    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText(tr("Ask about the current file/selection… (Ctrl+Enter sends)"));
    m_input->setMaximumHeight(110);
    layout->addWidget(m_input);

    auto* sendRow = new QHBoxLayout;
    m_sendBtn = new QPushButton(tr("Send"), this);
    m_sendBtn->setIcon(Icons::icon(Icons::Name::Robot));
    m_stopBtn = new QToolButton(this);
    m_stopBtn->setText(tr("Stop"));
    m_stopBtn->setEnabled(false);
    sendRow->addStretch(1);
    sendRow->addWidget(m_stopBtn);
    sendRow->addWidget(m_sendBtn);
    layout->addLayout(sendRow);

    connect(m_sendBtn, &QPushButton::clicked, this, &AssistantPanel::onSend);
    connect(m_stopBtn, &QToolButton::clicked, this, &AssistantPanel::onStop);
    connect(m_modelsBtn, &QToolButton::clicked, this, &AssistantPanel::onRefreshModels);
    connect(m_endpoint, &QLineEdit::editingFinished, this, &AssistantPanel::onEndpointChanged);
    connect(m_apiKey, &QLineEdit::editingFinished, this, [this]() { setStoredApiKey(m_apiKey->text()); });
    connect(m_input, &QPlainTextEdit::textChanged, this, [this]() {
        // Ctrl+Enter sends; Enter inserts newline
        if (QApplication::keyboardModifiers() & Qt::ControlModifier)
            onSend();
    });
}

void AssistantPanel::loadSettingsToUi()
{
    m_endpoint->setText(SettingsManager::instance()
                            .getString(QStringLiteral("ai.endpoint"))
                            .isEmpty()
                            ? QStringLiteral("http://localhost:11434/v1")
                            : SettingsManager::instance().getString(QStringLiteral("ai.endpoint")));
    m_model->setCurrentText(SettingsManager::instance().getString(QStringLiteral("ai.model")));
    m_apiKey->setText(loadStoredApiKey());
    onEndpointChanged();
}

void AssistantPanel::persistSettings()
{
    SettingsManager::instance().set(QStringLiteral("ai.endpoint"), m_endpoint->text().trimmed());
    SettingsManager::instance().set(QStringLiteral("ai.model"), m_model->currentText().trimmed());
}

void AssistantPanel::setEditorContextProvider(std::function<QString(int*)> provider)
{
    m_contextProvider = std::move(provider);
}

void AssistantPanel::setInsertCallback(std::function<void(const QString&)> cb)
{
    m_insertCallback = std::move(cb);
}

void AssistantPanel::onEndpointChanged()
{
    persistSettings();
    const bool local = isLocalEndpoint(m_endpoint->text().trimmed());
    if (local) {
        m_privacy->setText(tr("Offline-first: endpoint is local — code never leaves this machine."));
        m_privacy->setStyleSheet(QStringLiteral("color: #3fb950;"));
    } else {
        m_privacy->setText(tr("WARNING: remote endpoint — your code will be sent to %1.")
                               .arg(QUrl(m_endpoint->text()).host()));
        m_privacy->setStyleSheet(QStringLiteral("color: #d29922;"));
    }
}

QString AssistantPanel::currentApiKey() const
{
    return m_apiKey->text();
}

void AssistantPanel::setStoredApiKey(const QString& key)
{
    if (!saveStoredApiKey(key))
        Logger::instance().warning(QStringLiteral("AI: cannot persist API key"));
}

void AssistantPanel::onRefreshModels()
{
    const QUrl base(m_endpoint->text().trimmed());
    QNetworkRequest req(QUrl(base.resolved(QUrl(QStringLiteral("/models")))));
    req.setTransferTimeout(8000);
    if (!m_apiKey->text().isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_apiKey->text().toUtf8());
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            appendTranscript(QStringLiteral("system"),
                             tr("Could not list models: %1").arg(reply->errorString()));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray data = doc.object().value(QStringLiteral("data")).toArray();
        m_model->clear();
        for (const auto& v : data) {
            const QString id = v.toObject().value(QStringLiteral("id")).toString();
            if (!id.isEmpty())
                m_model->addItem(id);
        }
        if (m_model->count() == 0)
            m_model->setCurrentText(QString());
        persistSettings();
    });
}

void AssistantPanel::appendTranscript(const QString& who, const QString& html)
{
    const QColor acc = ThemeManager::instance().currentTheme()
                           .color(QStringLiteral("accent"), QColor(0x58, 0xa6, 0xff));
    QString whoHtml = escapeHtml(who);
    if (who == QStringLiteral("you"))
        whoHtml = QStringLiteral("<b style=\"color:%1\">%2</b>")
                      .arg(acc.name(), tr("you"));
    m_transcript->append(QStringLiteral("<p><span style=\"color:gray\">%1:</span> %2</p>")
                             .arg(whoHtml, html));
    m_transcript->moveCursor(QTextCursor::End);
}

void AssistantPanel::onAction(const QString& kind)
{
    if (!m_contextProvider)
        return;
    int selStartLine = -1;
    const QString code = m_contextProvider(&selStartLine);
    if (code.trimmed().isEmpty()) {
        QMessageBox::information(this, tr("AI Assistant"),
                                 tr("Open a file (or select code) first."));
        return;
    }
    QString instruction;
    if (kind == tr("Explain"))
        instruction = tr("Explain what this code does, step by step:");
    else if (kind == tr("Refactor"))
        instruction = tr("Refactor this code for clarity and correctness, keep behavior identical:");
    else if (kind == tr("Fix"))
        instruction = tr("Find and fix bugs or issues in this code, show the corrected version:");
    else
        instruction = tr("Write unit tests for this code:");
    QString loc;
    if (selStartLine >= 0)
        loc = tr("\n(selection starting at line %1)").arg(selStartLine + 1);
    const QString user = QStringLiteral("%1%2\n\n```%3")
                             .arg(instruction, loc, code);
    m_input->setPlainText(QString());
    postChat(buildMessages(user));
    appendTranscript(tr("you"), QStringLiteral("<i>%1</i>").arg(escapeHtml(instruction)) + loc);
}

QJsonArray AssistantPanel::buildMessages(const QString& userText) const
{
    QJsonArray messages = m_history;
    QJsonObject sys;
    sys.insert(QStringLiteral("role"), QStringLiteral("system"));
    sys.insert(QStringLiteral("content"),
               tr("You are a concise coding assistant embedded in CodeForge, a lightweight "
                  "code editor. Answer with practical, minimal prose and code blocks."));
    messages.append(sys);
    QJsonObject u;
    u.insert(QStringLiteral("role"), QStringLiteral("user"));
    u.insert(QStringLiteral("content"), userText);
    messages.append(u);
    return messages;
}

void AssistantPanel::onSend()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || m_streaming)
        return;
    m_input->clear();
    appendTranscript(tr("you"), escapeHtml(text));
    postChat(buildMessages(text));
}

void AssistantPanel::postChat(const QJsonArray& messages)
{
    if (m_streaming)
        return;
    QJsonObject body;
    body.insert(QStringLiteral("model"), m_model->currentText().trimmed());
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("stream"), true);
    QNetworkRequest req(QUrl(m_endpoint->text().trimmed() + QStringLiteral("/chat/completions")));
    req.setTransferTimeout(120000);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_apiKey->text().isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_apiKey->text().toUtf8());
    m_streamText.clear();
    m_streaming = true;
    m_sendBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &AssistantPanel::onReadyReadChunk);
    connect(m_reply, &QNetworkReply::finished, this, &AssistantPanel::onFinished);
}

void AssistantPanel::onStop()
{
    if (m_reply)
        m_reply->abort();
}

void AssistantPanel::onReadyReadChunk()
{
    if (!m_reply)
        return;
    while (m_reply->canReadLine()) {
        const QString line = QString::fromUtf8(m_reply->readLine()).trimmed();
        if (!line.startsWith(QLatin1String("data:")))
            continue;
        const QString payload = line.mid(5).trimmed();
        if (payload == QLatin1String("[DONE]"))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
        const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty())
            continue;
        const QString delta = choices.first().toObject()
                                  .value(QStringLiteral("delta"))
                                  .toObject()
                                  .value(QStringLiteral("content"))
                                  .toString();
        if (delta.isEmpty())
            continue;
        m_streamText += delta;
        appendTranscript(tr("assistant"), QStringLiteral("<span style=\"white-space:pre-wrap\">%1</span>")
                                             .arg(escapeHtml(m_streamText)));
    }
}

void AssistantPanel::onFinished()
{
    if (!m_reply)
        return;
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    m_streaming = false;
    m_sendBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    const QString err = reply->error() == QNetworkReply::NoError
                            ? QString()
                            : reply->errorString();
    reply->deleteLater();
    if (!m_streamText.isEmpty()) {
        m_history.append(QJsonObject({ { QStringLiteral("role"), QStringLiteral("assistant") },
                                       { QStringLiteral("content"), m_streamText } }));
        // keep the local conversation bounded
        while (m_history.size() > 12)
            m_history.removeFirst();
        Logger::instance().info(QStringLiteral("AI: reply received (%1 chars)").arg(m_streamText.size()));
    }
    if (!err.isEmpty())
        appendTranscript(QStringLiteral("system"),
                         tr("Request failed: %1 — is your local model server running?").arg(err));
}

}  // namespace cf
