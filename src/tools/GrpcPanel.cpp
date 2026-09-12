#include "tools/GrpcPanel.h"

#include <QCheckBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QProcess>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/Logger.h"
#include "settings/SettingsManager.h"

#include <QElapsedTimer>

namespace cf {

GrpcPanel::GrpcPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    onRefreshTools();
}

QString GrpcPanel::findGrpcurl()
{
    QString p = SettingsManager::instance().getString(QStringLiteral("grpc.grpcurlPath"));
    if (!p.isEmpty() && QFileInfo::exists(p))
        return p;
    return QStandardPaths::findExecutable(QStringLiteral("grpcurl"));
}

void GrpcPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    m_toolStatus = new QLabel(this);
    layout->addWidget(m_toolStatus);

    auto* row1 = new QHBoxLayout;
    m_grpcurlPath = new QLineEdit(this);
    m_grpcurlPath->setPlaceholderText(tr("grpcurl path (empty = auto-detect on PATH)"));
    m_refreshBtn = new QToolButton(this);
    m_refreshBtn->setText(tr("Detect"));
    row1->addWidget(new QLabel(tr("grpcurl:"), this));
    row1->addWidget(m_grpcurlPath, 1);
    row1->addWidget(m_refreshBtn);
    layout->addLayout(row1);

    auto* row2 = new QHBoxLayout;
    m_target = new QLineEdit(this);
    m_target->setPlaceholderText(QStringLiteral("host:port  (e.g. localhost:50051)"));
    m_method = new QLineEdit(this);
    m_method->setPlaceholderText(QStringLiteral("package.Service/Method"));
    m_plaintext = new QCheckBox(tr("Plaintext (-plaintext)"), this);
    m_plaintext->setChecked(true);
    row2->addWidget(new QLabel(tr("Target:"), this));
    row2->addWidget(m_target, 2);
    row2->addWidget(new QLabel(tr("Method:"), this));
    row2->addWidget(m_method, 3);
    row2->addWidget(m_plaintext);
    layout->addLayout(row2);

    auto* row3 = new QHBoxLayout;
    m_cacert = new QLineEdit(this);
    m_cacert->setPlaceholderText(tr("CA certificate path (optional, TLS)"));
    row3->addWidget(new QLabel(tr("CA:"), this));
    row3->addWidget(m_cacert, 1);
    layout->addLayout(row3);

    auto* split = new QSplitter(Qt::Vertical, this);
    layout->addWidget(split, 1);

    auto* reqTabs = new QTabWidget(this);
    m_request = new QPlainTextEdit(reqTabs);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_request->setFont(mono);
    m_request->setPlaceholderText(QStringLiteral("{\n  \"name\": \"world\"\n}"));
    reqTabs->addTab(m_request, tr("Request (JSON, sent via -d @)"));
    m_metadata = new QPlainTextEdit(reqTabs);
    m_metadata->setFont(mono);
    m_metadata->setPlaceholderText(tr("One per line:  authorization: Bearer <token>"));
    reqTabs->addTab(m_metadata, tr("Metadata (-H)"));
    split->addWidget(reqTabs);

    m_response = new QPlainTextEdit(this);
    m_response->setReadOnly(true);
    m_response->setFont(mono);
    m_exitStatus = new QLabel(this);
    auto* respPage = new QWidget(this);
    auto* respLayout = new QVBoxLayout(respPage);
    respLayout->setContentsMargins(0, 0, 0, 0);
    respLayout->addWidget(m_response, 1);
    respLayout->addWidget(m_exitStatus);
    split->addWidget(respPage);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);

    auto* row4 = new QHBoxLayout;
    m_invokeBtn = new QToolButton(this);
    m_invokeBtn->setText(tr("Invoke"));
    m_cancelBtn = new QToolButton(this);
    m_cancelBtn->setText(tr("Cancel"));
    m_cancelBtn->setEnabled(false);
    row4->addStretch(1);
    row4->addWidget(m_invokeBtn);
    row4->addWidget(m_cancelBtn);
    layout->addLayout(row4);

    connect(m_refreshBtn, &QToolButton::clicked, this, &GrpcPanel::onRefreshTools);
    connect(m_invokeBtn, &QToolButton::clicked, this, &GrpcPanel::onInvoke);
    connect(m_cancelBtn, &QToolButton::clicked, this, &GrpcPanel::onCancel);
}

void GrpcPanel::onRefreshTools()
{
    // persist a manual override so detection is stable
    if (!m_grpcurlPath->text().trimmed().isEmpty()) {
        SettingsManager::instance().set(QStringLiteral("grpc.grpcurlPath"),
                                        m_grpcurlPath->text().trimmed());
    }
    const QString tool = findGrpcurl();
    if (tool.isEmpty()) {
        m_toolStatus->setText(
            tr("grpcurl not found. Install it (go install github.com/fullstorydev/grpcurl/cmd/grpcurl@latest "
               "or download from github.com/grpc/grpcui / grpcurl releases) and set its path above."));
        m_invokeBtn->setEnabled(false);
    } else {
        m_toolStatus->setText(tr("grpcurl found: %1").arg(tool));
        if (m_grpcurlPath->text().trimmed().isEmpty())
            m_grpcurlPath->setPlaceholderText(tool);
        m_invokeBtn->setEnabled(true);
    }
}

QStringList GrpcPanel::buildArgs() const
{
    QStringList args;
    const QString tool = m_grpcurlPath->text().trimmed();
    Q_UNUSED(tool);
    if (m_plaintext->isChecked())
        args << QStringLiteral("-plaintext");
    if (!m_cacert->text().trimmed().isEmpty())
        args << QStringLiteral("-cacert") << m_cacert->text().trimmed();
    const QStringList metaLines =
        m_metadata->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : metaLines) {
        const QString t = line.trimmed();
        if (!t.isEmpty())
            args << QStringLiteral("-H") << t;
    }
    args << QStringLiteral("-d") << QStringLiteral("@");
    args << m_target->text().trimmed();
    args << m_method->text().trimmed();
    return args;
}

void GrpcPanel::onInvoke()
{
    if (m_proc)
        return;
    QString tool = findGrpcurl();
    if (!m_grpcurlPath->text().trimmed().isEmpty())
        tool = m_grpcurlPath->text().trimmed();
    if (tool.isEmpty() || m_target->text().trimmed().isEmpty()
        || m_method->text().trimmed().isEmpty()) {
        QMessageBox::information(this, tr("gRPC"),
                                 tr("grpcurl, target and method are all required."));
        return;
    }
    m_response->clear();
    m_exitStatus->setText(tr("Invoking…"));
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished, this, &GrpcPanel::onProcessFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            m_exitStatus->setText(tr("Failed to start grpcurl."));
            m_invokeBtn->setEnabled(true);
            m_cancelBtn->setEnabled(false);
        }
    });
    m_proc->start(tool, buildArgs());
    m_proc->write(m_request->toPlainText().toUtf8());
    m_proc->closeWriteChannel();
    m_invokeBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
}

void GrpcPanel::onCancel()
{
    if (m_proc)
        m_proc->kill();
}

void GrpcPanel::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_proc)
        return;
    const QByteArray out = m_proc->readAllStandardOutput();
    const QByteArray errOut = m_proc->readAllStandardError();
    m_proc->deleteLater();
    m_proc = nullptr;
    m_invokeBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_response->setPlainText(QString::fromUtf8(out + errOut));
    if (status == QProcess::CrashExit) {
        m_exitStatus->setText(tr("grpcurl crashed (killed)."));
    } else if (exitCode == 0) {
        m_exitStatus->setText(tr("OK (exit %1)").arg(exitCode));
    } else {
        m_exitStatus->setText(tr("Failed (exit %1) — see output for the server/tool error.")
                                  .arg(exitCode));
    }
    Logger::instance().info(QStringLiteral("grpcurl exit %1").arg(exitCode));
}

}  // namespace cf
