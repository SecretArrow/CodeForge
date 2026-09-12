#pragma once
// HttpPanel: built-in REST/HTTP client (QtNetwork). Methods, headers, auth
// (basic/bearer), body, response viewer with timing/size and pretty JSON.
// Requests can be saved/loaded as JSON in <workspace>/.codeforge/http/.
// Offline-first: no request is made until the user presses Send.
#include <QElapsedTimer>
#include <QHash>
#include <QNetworkRequest>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;
class QTreeWidget;
class QToolButton;

namespace cf {

class HttpPanel : public QWidget {
    Q_OBJECT
public:
    explicit HttpPanel(QWidget* parent = nullptr);

    // Focus helpers for commands.
    void focusUrlEdit();

private slots:
    void onSend();
    void onCancel();
    void onResponseFinished();
    void onReadReady();
    void onPrettyToggled(bool on);
    void onSaveRequest();
    void onLoadRequest();
    void onCopyAsCurl();

private:
    void buildUi();
    QByteArray buildRequestBody() const;
    QNetworkRequest buildRequest() const;
    void setBodyWidgetsEnabled();
    void storeLastReplyData();

    QNetworkAccessManager* m_nam;
    QComboBox* m_method;
    QLineEdit* m_url;
    QSpinBox* m_timeout;
    QToolButton* m_sendBtn;
    QToolButton* m_cancelBtn;

    QPlainTextEdit* m_body;
    QComboBox* m_bodyType;
    QTableWidget* m_headers;
    QComboBox* m_authType;
    QLineEdit* m_authUser;
    QLineEdit* m_authPass;
    QLineEdit* m_authToken;

    QLabel* m_status;
    QLabel* m_timeSize;
    QTreeWidget* m_respHeaders;
    QPlainTextEdit* m_respBody;
    QToolButton* m_prettyBtn;

    QNetworkReply* m_reply = nullptr;
    qint64 m_bytes = 0;
    QElapsedTimer m_timer;
    int m_lastStatus = 0;
    QByteArray m_lastRawBody;
    QHash<QString, QString> m_lastHeaderMap;
    QString m_lastRawBodyShown;   // unmodified text shown (pretty toggle state)
};

}  // namespace cf
