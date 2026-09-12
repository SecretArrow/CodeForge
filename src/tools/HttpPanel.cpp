#include "tools/HttpPanel.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFontDatabase>
#include <QTreeWidgetItem>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
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
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "core/FileUtils.h"
#include "core/Logger.h"
#include "ui/Icons.h"

namespace cf {

HttpPanel::HttpPanel(QWidget* parent)
    : QWidget(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    buildUi();
}

void HttpPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ---- request row ----
    auto* reqRow = new QHBoxLayout;
    m_method = new QComboBox(this);
    m_method->addItems({ QStringLiteral("GET"), QStringLiteral("POST"), QStringLiteral("PUT"),
                         QStringLiteral("PATCH"), QStringLiteral("DELETE"), QStringLiteral("HEAD"),
                         QStringLiteral("OPTIONS") });
    m_url = new QLineEdit(this);
    m_url->setPlaceholderText(QStringLiteral("https://api.example.com/v1/resource"));
    m_url->setClearButtonEnabled(true);
    m_timeout = new QSpinBox(this);
    m_timeout->setRange(1, 600);
    m_timeout->setValue(30);
    m_timeout->setSuffix(tr(" s"));
    m_timeout->setToolTip(tr("Request timeout"));
    m_sendBtn = new QToolButton(this);
    m_sendBtn->setText(tr("Send"));
    m_sendBtn->setIcon(Icons::icon(Icons::Name::Play));
    m_sendBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_cancelBtn = new QToolButton(this);
    m_cancelBtn->setText(tr("Cancel"));
    m_cancelBtn->setEnabled(false);
    reqRow->addWidget(m_method);
    reqRow->addWidget(m_url, 1);
    reqRow->addWidget(m_timeout);
    reqRow->addWidget(m_sendBtn);
    reqRow->addWidget(m_cancelBtn);
    layout->addLayout(reqRow);

    auto* split = new QSplitter(Qt::Vertical, this);
    layout->addWidget(split, 1);

    // ---- request tabs ----
    auto* reqTabs = new QTabWidget(this);
    m_body = new QPlainTextEdit(reqTabs);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_body->setFont(mono);
    m_bodyType = new QComboBox(reqTabs);
    m_bodyType->addItems({ QStringLiteral("JSON"), QStringLiteral("Raw"), QStringLiteral("XML") });
    auto* bodyPage = new QWidget(reqTabs);
    auto* bodyLayout = new QVBoxLayout(bodyPage);
    bodyLayout->setContentsMargins(4, 4, 4, 4);
    auto* bodyTop = new QHBoxLayout;
    bodyTop->addWidget(new QLabel(tr("Type:"), bodyPage));
    bodyTop->addWidget(m_bodyType);
    bodyTop->addStretch(1);
    bodyLayout->addLayout(bodyTop);
    bodyLayout->addWidget(m_body, 1);
    reqTabs->addTab(bodyPage, tr("Body"));

    m_headers = new QTableWidget(0, 2, reqTabs);
    m_headers->setHorizontalHeaderLabels({ tr("Name"), tr("Value") });
    m_headers->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_headers->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_headers->verticalHeader()->setVisible(false);
    auto* headerPage = new QWidget(reqTabs);
    auto* headerLayout = new QVBoxLayout(headerPage);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    auto* addHeader = new QToolButton(headerPage);
    addHeader->setText(tr("Add header"));
    headerLayout->addWidget(addHeader);
    headerLayout->addWidget(m_headers, 1);
    connect(addHeader, &QToolButton::clicked, this, [this]() {
        m_headers->insertRow(m_headers->rowCount());
    });
    reqTabs->addTab(headerPage, tr("Headers"));

    auto* authPage = new QWidget(reqTabs);
    auto* authForm = new QFormLayout(authPage);
    authForm->setContentsMargins(4, 4, 4, 4);
    m_authType = new QComboBox(authPage);
    m_authType->addItems({ tr("None"), tr("Basic"), tr("Bearer token") });
    m_authUser = new QLineEdit(authPage);
    m_authUser->setEchoMode(QLineEdit::Normal);
    m_authPass = new QLineEdit(authPage);
    m_authPass->setEchoMode(QLineEdit::Password);   // never displayed, not logged
    m_authToken = new QLineEdit(authPage);
    m_authToken->setEchoMode(QLineEdit::Password);
    authForm->addRow(tr("Type"), m_authType);
    authForm->addRow(tr("User"), m_authUser);
    authForm->addRow(tr("Password"), m_authPass);
    authForm->addRow(tr("Token"), m_authToken);
    reqTabs->addTab(authPage, tr("Auth"));

    auto* saveBtn = new QToolButton(this);
    saveBtn->setText(tr("Save request"));
    auto* loadBtn = new QToolButton(this);
    loadBtn->setText(tr("Load request"));
    auto* curlBtn = new QToolButton(this);
    curlBtn->setText(tr("Copy as cURL"));
    auto* saveRow = new QHBoxLayout;
    saveRow->addWidget(saveBtn);
    saveRow->addWidget(loadBtn);
    saveRow->addWidget(curlBtn);
    saveRow->addStretch(1);
    auto* bottomRow = new QWidget(this);
    bottomRow->setLayout(saveRow);

    auto* reqContainer = new QWidget(this);
    auto* reqContainerLayout = new QVBoxLayout(reqContainer);
    reqContainerLayout->setContentsMargins(0, 0, 0, 0);
    reqContainerLayout->addWidget(reqTabs, 1);
    reqContainerLayout->addWidget(bottomRow);
    split->addWidget(reqContainer);

    // ---- response ----
    auto* respTabs = new QTabWidget(this);
    m_respBody = new QPlainTextEdit(respTabs);
    m_respBody->setReadOnly(true);
    m_respBody->setFont(mono);
    m_prettyBtn = new QToolButton(respTabs);
    m_prettyBtn->setText(tr("Pretty JSON"));
    m_prettyBtn->setCheckable(true);
    auto* respBodyPage = new QWidget(respTabs);
    auto* respBodyLayout = new QVBoxLayout(respBodyPage);
    respBodyLayout->setContentsMargins(4, 4, 4, 4);
    auto* respTop = new QHBoxLayout;
    respTop->addWidget(m_prettyBtn);
    respTop->addStretch(1);
    respBodyLayout->addLayout(respTop);
    respBodyLayout->addWidget(m_respBody, 1);
    respTabs->addTab(respBodyPage, tr("Body"));

    m_respHeaders = new QTreeWidget(respTabs);
    m_respHeaders->setHeaderLabels({ tr("Header"), tr("Value") });
    m_respHeaders->setColumnWidth(0, 240);
    respTabs->addTab(m_respHeaders, tr("Headers"));

    m_status = new QLabel(tr("Ready. Offline by default — nothing is sent until you press Send."),
                          this);
    m_timeSize = new QLabel(this);

    auto* respContainer = new QWidget(this);
    auto* respLayout = new QVBoxLayout(respContainer);
    respLayout->setContentsMargins(0, 0, 0, 0);
    auto* respMeta = new QHBoxLayout;
    respMeta->addWidget(m_status, 1);
    respMeta->addWidget(m_timeSize);
    respLayout->addLayout(respMeta);
    respLayout->addWidget(respTabs, 1);
    split->addWidget(respContainer);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);

    connect(m_sendBtn, &QToolButton::clicked, this, &HttpPanel::onSend);
    connect(m_cancelBtn, &QToolButton::clicked, this, &HttpPanel::onCancel);
    connect(m_url, &QLineEdit::returnPressed, this, &HttpPanel::onSend);
    connect(m_prettyBtn, &QToolButton::toggled, this, &HttpPanel::onPrettyToggled);
    connect(m_bodyType, &QComboBox::currentIndexChanged, this, [this](int) { setBodyWidgetsEnabled(); });
    connect(saveBtn, &QToolButton::clicked, this, &HttpPanel::onSaveRequest);
    connect(loadBtn, &QToolButton::clicked, this, &HttpPanel::onLoadRequest);
    connect(curlBtn, &QToolButton::clicked, this, &HttpPanel::onCopyAsCurl);
    setBodyWidgetsEnabled();
}

void HttpPanel::focusUrlEdit()
{
    m_url->setFocus();
    m_url->selectAll();
}

void HttpPanel::setBodyWidgetsEnabled()
{
    const bool hasBody = m_method->currentText() != QLatin1String("GET")
                         && m_method->currentText() != QLatin1String("HEAD");
    m_body->setEnabled(hasBody);
    m_bodyType->setEnabled(hasBody);
}

QByteArray HttpPanel::buildRequestBody() const
{
    return m_body->toPlainText().toUtf8();
}

QNetworkRequest HttpPanel::buildRequest() const
{
    QNetworkRequest req(QUrl(m_url->text().trimmed()));
    req.setTransferTimeout(m_timeout->value() * 1000);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // headers
    for (int i = 0; i < m_headers->rowCount(); ++i) {
        const QString name = m_headers->item(i, 0)
                                 ? m_headers->item(i, 0)->text().trimmed()
                                 : QString();
        const QString value = m_headers->item(i, 1)
                                  ? m_headers->item(i, 1)->text()
                                  : QString();
        if (!name.isEmpty())
            req.setRawHeader(name.toUtf8(), value.toUtf8());
    }
    // default content type from body type
    if (req.hasRawHeader("Content-Type")) {
        // caller override wins
    } else if (m_bodyType->currentText() == QLatin1String("JSON")) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    } else if (m_bodyType->currentText() == QLatin1String("XML")) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml"));
    }
    // auth (credentials are used in-memory only; never persisted or logged)
    if (m_authType->currentText() == QStringLiteral("Basic")
        && !m_authUser->text().isEmpty()) {
        const QString cred = m_authUser->text() + QLatin1Char(':') + m_authPass->text();
        req.setRawHeader("Authorization", "Basic " + cred.toUtf8().toBase64());
    } else if (m_authType->currentText() == QStringLiteral("Bearer token")
               && !m_authToken->text().isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + m_authToken->text().toUtf8());
    }
    return req;
}

void HttpPanel::onSend()
{
    if (m_reply)
        return;
    const QUrl url(m_url->text().trimmed());
    if (!url.isValid() || url.scheme().isEmpty()) {
        m_status->setText(tr("Invalid URL."));
        return;
    }
    const QByteArray method = m_method->currentText().toLatin1();
    QNetworkRequest req = buildRequest();
    m_bytes = 0;
    m_lastStatus = 0;
    m_lastRawBody.clear();
    m_lastHeaderMap.clear();
    m_respHeaders->clear();
    m_timer.start();
    if (method == "GET" || method == "HEAD")
        m_reply = m_nam->sendCustomRequest(req, method);
    else
        m_reply = m_nam->sendCustomRequest(req, method, buildRequestBody());
    connect(m_reply, &QNetworkReply::finished, this, &HttpPanel::onResponseFinished);
    connect(m_reply, &QNetworkReply::readyRead, this, &HttpPanel::onReadReady);
    m_sendBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_status->setText(tr("Sending…"));
}

void HttpPanel::onCancel()
{
    if (m_reply) {
        m_reply->abort();   // finished() will fire
    }
}

void HttpPanel::onReadReady()
{
    if (!m_reply)
        return;
    m_lastRawBody += m_reply->readAll();
    m_bytes = m_lastRawBody.size();
}

void HttpPanel::storeLastReplyData()
{
    if (!m_reply)
        return;
    m_lastStatus = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    for (const QNetworkReply::RawHeaderPair& pair : m_reply->rawHeaderPairs())
        m_lastHeaderMap.insert(QString::fromLatin1(pair.first),
                               QString::fromLatin1(pair.second));
}

void HttpPanel::onResponseFinished()
{
    if (!m_reply)
        return;
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    storeLastReplyData();
    m_lastRawBody += reply->readAll();
    const qint64 elapsed = m_timer.elapsed();
    const QString err = reply->error() == QNetworkReply::NoError
                            ? QString()
                            : reply->errorString();
    reply->deleteLater();

    m_sendBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);

    m_status->setText(QStringLiteral("%1 %2").arg(m_lastStatus).arg(err));
    if (m_lastStatus >= 500)
        m_status->setStyleSheet(QStringLiteral("color: #f85149;"));
    else if (m_lastStatus >= 400)
        m_status->setStyleSheet(QStringLiteral("color: #d29922;"));
    else if (m_lastStatus >= 200 && m_lastStatus < 300)
        m_status->setStyleSheet(QStringLiteral("color: #3fb950;"));
    else
        m_status->setStyleSheet(QString());
    m_timeSize->setText(tr("%1 ms · %2 B").arg(elapsed).arg(m_lastRawBody.size()));

    for (auto it = m_lastHeaderMap.constBegin(); it != m_lastHeaderMap.constEnd(); ++it) {
        auto* item = new QTreeWidgetItem(m_respHeaders);
        item->setText(0, it.key());
        item->setText(1, it.value());
    }
    m_prettyBtn->setChecked(false);
    m_respBody->setPlainText(QString::fromUtf8(m_lastRawBody));
    if (!err.isEmpty() && m_lastStatus == 0)
        Logger::instance().warning(QStringLiteral("HTTP request failed: %1").arg(err));
}

void HttpPanel::onPrettyToggled(bool on)
{
    if (!on) {
        m_respBody->setPlainText(QString::fromUtf8(m_lastRawBody));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(m_lastRawBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_status->setText(tr("Response is not valid JSON — showing raw body."));
        m_prettyBtn->setChecked(false);
        return;
    }
    m_respBody->setPlainText(QString::fromUtf8(
        doc.toJson(QJsonDocument::Indented)));
}

void HttpPanel::onSaveRequest()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save request"), QStringLiteral("request.json"),
        tr("HTTP request (*.json)"));
    if (path.isEmpty())
        return;
    QJsonObject o;
    o.insert(QStringLiteral("method"), m_method->currentText());
    o.insert(QStringLiteral("url"), m_url->text());
    o.insert(QStringLiteral("bodyType"), m_bodyType->currentText());
    o.insert(QStringLiteral("body"), m_body->toPlainText());
    o.insert(QStringLiteral("timeoutSec"), m_timeout->value());
    o.insert(QStringLiteral("authType"), m_authType->currentText());
    // SECURITY: password/token are intentionally NOT saved.
    QJsonArray headers;
    for (int i = 0; i < m_headers->rowCount(); ++i) {
        const QString name = m_headers->item(i, 0) ? m_headers->item(i, 0)->text() : QString();
        const QString value = m_headers->item(i, 1) ? m_headers->item(i, 1)->text() : QString();
        if (!name.isEmpty()) {
            QJsonObject h;
            h.insert(QStringLiteral("name"), name);
            h.insert(QStringLiteral("value"), value);
            headers.append(h);
        }
    }
    o.insert(QStringLiteral("headers"), headers);
    QString err;
    if (!fs::writeAllAtomic(path, QJsonDocument(o).toJson(QJsonDocument::Indented), &err))
        QMessageBox::warning(this, tr("Save request"), err);
}

void HttpPanel::onLoadRequest()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load request"), QString(), tr("HTTP request (*.json)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Load request"), f.errorString());
        return;
    }
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    const int idx = m_method->findText(o.value(QStringLiteral("method")).toString());
    if (idx >= 0)
        m_method->setCurrentIndex(idx);
    m_url->setText(o.value(QStringLiteral("url")).toString());
    const int bt = m_bodyType->findText(o.value(QStringLiteral("bodyType")).toString());
    if (bt >= 0)
        m_bodyType->setCurrentIndex(bt);
    m_body->setPlainText(o.value(QStringLiteral("body")).toString());
    m_timeout->setValue(o.value(QStringLiteral("timeoutSec")).toInt(30));
    const int at = m_authType->findText(o.value(QStringLiteral("authType")).toString());
    if (at >= 0)
        m_authType->setCurrentIndex(at);
    m_headers->setRowCount(0);
    const QJsonArray headers = o.value(QStringLiteral("headers")).toArray();
    for (const auto& h : headers) {
        const QJsonObject ho = h.toObject();
        const int row = m_headers->rowCount();
        m_headers->insertRow(row);
        m_headers->setItem(row, 0, new QTableWidgetItem(ho.value(QStringLiteral("name")).toString()));
        m_headers->setItem(row, 1, new QTableWidgetItem(ho.value(QStringLiteral("value")).toString()));
    }
    setBodyWidgetsEnabled();
}

void HttpPanel::onCopyAsCurl()
{
    QString curl = QStringLiteral("curl -X %1 '%2'").arg(m_method->currentText(), m_url->text());
    for (int i = 0; i < m_headers->rowCount(); ++i) {
        const QString name = m_headers->item(i, 0) ? m_headers->item(i, 0)->text() : QString();
        const QString value = m_headers->item(i, 1) ? m_headers->item(i, 1)->text() : QString();
        if (!name.isEmpty())
            curl += QStringLiteral(" -H '%1: %2'").arg(name, value);
    }
    if (m_authType->currentText() == QStringLiteral("Basic") && !m_authUser->text().isEmpty())
        curl += QStringLiteral(" -u '%1:%2'").arg(m_authUser->text(), m_authPass->text());
    else if (m_authType->currentText() == QStringLiteral("Bearer token")
             && !m_authToken->text().isEmpty())
        curl += QStringLiteral(" -H 'Authorization: Bearer <token>'");
    const QByteArray body = buildRequestBody();
    if (!body.isEmpty() && m_method->currentText() != QLatin1String("GET"))
        curl += QStringLiteral(" --data '%1'").arg(QString::fromUtf8(body));
    QApplication::clipboard()->setText(curl);
    m_status->setText(tr("cURL command copied to clipboard."));
}

}  // namespace cf
