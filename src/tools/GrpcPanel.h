#pragma once
// GrpcPanel: gRPC caller that drives the grpcurl tool (grpc/grpcurl).
// Real functionality requires grpcurl to be installed and reachable on PATH
// or via the grpc.grpcurlPath setting; the panel reports exactly what is
// missing instead of faking a response.
#include <QProcess>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QToolButton;

namespace cf {

class GrpcPanel : public QWidget {
    Q_OBJECT
public:
    explicit GrpcPanel(QWidget* parent = nullptr);

    // PATH / setting lookup, exposed for tests and status display.
    static QString findGrpcurl();

private slots:
    void onRefreshTools();
    void onInvoke();
    void onCancel();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void buildUi();
    QStringList buildArgs() const;

    QLineEdit* m_target;
    QLineEdit* m_method;
    QCheckBox* m_plaintext;
    QLineEdit* m_grpcurlPath;
    QLineEdit* m_cacert;
    QPlainTextEdit* m_metadata;      // "key: value" lines
    QPlainTextEdit* m_request;       // JSON body
    QPlainTextEdit* m_response;
    QToolButton* m_invokeBtn;
    QToolButton* m_cancelBtn;
    QToolButton* m_refreshBtn;
    QLabel* m_toolStatus;
    QLabel* m_exitStatus;
    QProcess* m_proc = nullptr;
};

}  // namespace cf
