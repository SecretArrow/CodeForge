#pragma once
// AssistantPanel: AI coding assistant for LOCAL models (offline-first).
// Talks to any OpenAI-compatible /v1/chat/completions endpoint — by default
// Ollama at http://localhost:11434/v1 (LM Studio, llama.cpp server, vLLM also
// work). Remote endpoints are supported but show a clear warning that code
// leaves the machine. Supports SSE streaming and editor-context actions
// (Explain / Refactor / Fix / Tests) on the active selection.
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QToolButton;

namespace cf {

class AssistantPanel : public QWidget {
    Q_OBJECT
public:
    explicit AssistantPanel(QWidget* parent = nullptr);

    void setEditorContextProvider(std::function<QString(int* selectionStartLine)> provider);
    void insertIntoActiveEditor(const QString& text);   // wired via a callback
    void setInsertCallback(std::function<void(const QString&)> cb);

private slots:
    void onSend();
    void onStop();
    void onAction(const QString& kind);
    void onReadyReadChunk();
    void onFinished();
    void onRefreshModels();
    void onEndpointChanged();

private:
    void buildUi();
    void loadSettingsToUi();
    void persistSettings();
    QString currentApiKey() const;        // decrypted in memory only
    void setStoredApiKey(const QString& key);
    void postChat(const QJsonArray& messages);
    void appendTranscript(const QString& who, const QString& html);
    QJsonArray buildMessages(const QString& userText) const;

    QNetworkAccessManager* m_nam;
    QLineEdit* m_endpoint;
    QComboBox* m_model;
    QLineEdit* m_apiKey;
    QTextBrowser* m_transcript;
    QPlainTextEdit* m_input;
    QPushButton* m_sendBtn;
    QToolButton* m_stopBtn;
    QToolButton* m_modelsBtn;
    QLabel* m_privacy;
    QNetworkReply* m_reply = nullptr;
    QString m_streamText;            // accumulating assistant message
    QJsonArray m_history;            // {"role","content"} pairs (local, never persisted)
    std::function<QString(int*)> m_contextProvider;
    std::function<void(const QString&)> m_insertCallback;
    bool m_streaming = false;
};

}  // namespace cf
