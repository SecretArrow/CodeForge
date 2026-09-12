#include "remote/RemotePanel.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "core/AppPaths.h"
#include "core/FileUtils.h"
#include "core/Logger.h"
#include "settings/SettingsManager.h"

namespace cf {

namespace {
// BatchMode: fail instead of hanging on interactive prompts. Password auth is
// intentionally unavailable in-app (documented; use keys or an agent).
QStringList baseSshOptions()
{
    return { QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
             QStringLiteral("-o"), QStringLiteral("ConnectTimeout=8"),
             QStringLiteral("-o"), QStringLiteral("StrictHostKeyChecking=accept-new") };
}
}  // namespace

QString RemotePanel::findSsh()
{
    return QStandardPaths::findExecutable(QStringLiteral("ssh"));
}

QString RemotePanel::findScp()
{
    return QStandardPaths::findExecutable(QStringLiteral("scp"));
}

RemotePanel::RemotePanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    loadProfiles();
}

void RemotePanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // ---- profile row ----
    auto* profRow = new QHBoxLayout;
    m_profiles = new QComboBox(this);
    m_connectBtn = new QToolButton(this);
    m_connectBtn->setText(tr("Connect"));
    m_disconnectBtn = new QToolButton(this);
    m_disconnectBtn->setText(tr("Disconnect"));
    m_disconnectBtn->setEnabled(false);
    m_removeProfile = new QToolButton(this);
    m_removeProfile->setText(tr("Delete profile"));
    profRow->addWidget(m_profiles, 1);
    profRow->addWidget(m_connectBtn);
    profRow->addWidget(m_disconnectBtn);
    profRow->addWidget(m_removeProfile);
    layout->addLayout(profRow);

    // ---- new profile form ----
    auto* form = new QFormLayout;
    form->setContentsMargins(0, 2, 0, 2);
    m_newName = new QLineEdit(this);
    m_newName->setPlaceholderText(tr("my-server"));
    m_newTarget = new QLineEdit(this);
    m_newTarget->setPlaceholderText(tr("user@host (key-based auth / agent)"));
    m_newPort = new QLineEdit(this);
    m_newPort->setPlaceholderText(QStringLiteral("22"));
    m_newPort->setMaximumWidth(90);
    m_newKey = new QLineEdit(this);
    m_newKey->setPlaceholderText(tr("identity file path (optional)"));
    auto* addBtn = new QToolButton(this);
    addBtn->setText(tr("Add profile"));
    auto* formRow = new QHBoxLayout;
    auto* formWidget = new QWidget(this);
    auto* formInner = new QFormLayout(formWidget);
    formInner->setContentsMargins(0, 0, 0, 0);
    formInner->addRow(tr("Name"), m_newName);
    formInner->addRow(tr("Target"), m_newTarget);
    auto* portKey = new QHBoxLayout;
    portKey->addWidget(m_newPort);
    portKey->addWidget(m_newKey, 1);
    formInner->addRow(tr("Port / Key"), portKey);
    formRow->addWidget(formWidget, 1);
    formRow->addWidget(addBtn);
    layout->addLayout(formRow);

    m_state = new QLabel(this);
    layout->addWidget(m_state);

    m_stack = new QStackedWidget(this);
    auto* hint = new QLabel(tr("Connect to browse remote files, edit them locally and\n"
                               "save back over SSH, download/upload and run commands.\n"
                               "Requires the OpenSSH client (installed by default on Windows 10/11)."),
                            this);
    m_stack->addWidget(hint);

    auto* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto* pathRow = new QHBoxLayout;
    m_remotePath = new QLineEdit(page);
    m_remotePath->setPlaceholderText(QStringLiteral("~/"));
    m_refreshBtn = new QToolButton(page);
    m_refreshBtn->setText(tr("Refresh"));
    pathRow->addWidget(new QLabel(tr("Path:"), page));
    pathRow->addWidget(m_remotePath, 1);
    pathRow->addWidget(m_refreshBtn);
    pageLayout->addLayout(pathRow);

    m_remoteFiles = new QTreeWidget(page);
    m_remoteFiles->setHeaderLabels({ tr("Name"), tr("Size") });
    m_remoteFiles->setColumnWidth(0, 320);
    pageLayout->addWidget(m_remoteFiles, 2);

    auto* fileRow = new QHBoxLayout;
    m_openBtn = new QToolButton(page);
    m_openBtn->setText(tr("Open remote file"));
    m_downloadBtn = new QToolButton(page);
    m_downloadBtn->setText(tr("Download…"));
    m_uploadBtn = new QToolButton(page);
    m_uploadBtn->setText(tr("Upload…"));
    fileRow->addWidget(m_openBtn);
    fileRow->addWidget(m_downloadBtn);
    fileRow->addWidget(m_uploadBtn);
    fileRow->addStretch(1);
    pageLayout->addLayout(fileRow);

    m_cmdInput = new QLineEdit(page);
    m_cmdInput->setPlaceholderText(tr("Run a command on the remote host and press Enter"));
    m_runBtn = new QToolButton(page);
    m_runBtn->setText(tr("Run"));
    auto* cmdRow = new QHBoxLayout;
    cmdRow->addWidget(m_cmdInput, 1);
    cmdRow->addWidget(m_runBtn);
    pageLayout->addLayout(cmdRow);

    m_cmdOutput = new QPlainTextEdit(page);
    m_cmdOutput->setReadOnly(true);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_cmdOutput->setFont(mono);
    pageLayout->addWidget(m_cmdOutput, 1);

    m_stack->addWidget(page);
    layout->addWidget(m_stack, 1);

    connect(m_connectBtn, &QToolButton::clicked, this, &RemotePanel::onConnect);
    connect(m_disconnectBtn, &QToolButton::clicked, this, &RemotePanel::onDisconnect);
    connect(m_refreshBtn, &QToolButton::clicked, this, &RemotePanel::onRefreshListing);
    connect(m_removeProfile, &QToolButton::clicked, this, [this]() {
        const QString name = m_profiles->currentText();
        if (name.isEmpty())
            return;
        m_profileNames.removeAll(name);
        m_profileTargets.remove(name);
        m_profilePorts.remove(name);
        m_profileKeys.remove(name);
        // persist
        QJsonArray arr;
        for (const QString& n : std::as_const(m_profileNames)) {
            QJsonObject o;
            o.insert(QStringLiteral("name"), n);
            o.insert(QStringLiteral("target"), m_profileTargets.value(n));
            o.insert(QStringLiteral("port"), m_profilePorts.value(n));
            o.insert(QStringLiteral("key"), m_profileKeys.value(n));
            arr.append(o);
        }
        SettingsManager::instance().set(
            QStringLiteral("remote.profiles"),
            QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        loadProfiles();
    });
    connect(addBtn, &QToolButton::clicked, this, [this]() {
        const QString name = m_newName->text().trimmed();
        const QString target = m_newTarget->text().trimmed();
        if (name.isEmpty() || target.isEmpty() || !target.contains(u'@')) {
            QMessageBox::information(this, tr("Remote"),
                                     tr("Profile needs a name and a user@host target."));
            return;
        }
        if (!m_profileNames.contains(name))
            m_profileNames.append(name);
        m_profileTargets.insert(name, target);
        m_profilePorts.insert(name, m_newPort->text().trimmed());
        m_profileKeys.insert(name, m_newKey->text().trimmed());
        QJsonArray arr;
        for (const QString& n : std::as_const(m_profileNames)) {
            QJsonObject o;
            o.insert(QStringLiteral("name"), n);
            o.insert(QStringLiteral("target"), m_profileTargets.value(n));
            o.insert(QStringLiteral("port"), m_profilePorts.value(n));
            o.insert(QStringLiteral("key"), m_profileKeys.value(n));
            arr.append(o);
        }
        SettingsManager::instance().set(
            QStringLiteral("remote.profiles"),
            QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
        m_newName->clear();
        m_newTarget->clear();
        m_newPort->clear();
        m_newKey->clear();
        loadProfiles();
    });
    connect(m_openBtn, &QToolButton::clicked, this, &RemotePanel::onOpenRemoteFile);
    connect(m_downloadBtn, &QToolButton::clicked, this, &RemotePanel::onDownloadFile);
    connect(m_uploadBtn, &QToolButton::clicked, this, &RemotePanel::onUploadFile);
    connect(m_runBtn, &QToolButton::clicked, this, &RemotePanel::onRunCommand);
    connect(m_cmdInput, &QLineEdit::returnPressed, this, &RemotePanel::onRunCommand);
}

void RemotePanel::loadProfiles()
{
    m_profiles->clear();
    const QJsonDocument doc = QJsonDocument::fromJson(
        SettingsManager::instance().getString(QStringLiteral("remote.profiles")).toUtf8());
    for (const auto& v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
            continue;
        if (!m_profileNames.contains(name))
            m_profileNames.append(name);
        m_profileTargets.insert(name, o.value(QStringLiteral("target")).toString());
        m_profilePorts.insert(name, o.value(QStringLiteral("port")).toString());
        m_profileKeys.insert(name, o.value(QStringLiteral("key")).toString());
    }
    m_profiles->addItems(m_profileNames);
}

QString RemotePanel::profileArg(const QString& profile) const
{
    QString arg = m_profileTargets.value(profile);
    if (arg.isEmpty())
        return arg;
    const QString port = m_profilePorts.value(profile);
    if (!port.isEmpty())
        arg += QStringLiteral(" -p %1").arg(port);
    const QString key = m_profileKeys.value(profile);
    if (!key.isEmpty())
        arg += QStringLiteral(" -i \"%1\"").arg(key);
    return arg;
}

void RemotePanel::onConnect()
{
    const QString profile = m_profiles->currentText();
    if (profile.isEmpty()) {
        QMessageBox::information(this, tr("Remote"), tr("Select or create a profile first."));
        return;
    }
    const QString ssh = findSsh();
    if (ssh.isEmpty()) {
        QMessageBox::warning(this, tr("Remote"),
                             tr("OpenSSH client not found. Install it via Settings > Apps > "
                                "Optional Features on Windows, or your package manager."));
        return;
    }
    m_activeProfile = profile;
    setConnected(false);
    // connectivity check (synchronous with BatchMode timeout; short on purpose)
    QProcess check(this);
    QStringList args = baseSshOptions();
    args << m_profileTargets.value(profile);
    args << QStringLiteral("echo __CF_OK__");
    m_state->setText(tr("Connecting to %1…").arg(profile));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    check.start(ssh, args);
    const bool finished = check.waitForFinished(15000);
    QApplication::restoreOverrideCursor();
    const QByteArray out = check.readAllStandardOutput();
    if (!finished || !out.contains("__CF_OK__")) {
        m_state->setText(tr("Connect failed: %1")
                             .arg(QString::fromUtf8(check.readAllStandardError().trimmed())));
        return;
    }
    setConnected(true);
    m_state->setText(tr("Connected: %1").arg(profile));
    m_remotePath->setText(QStringLiteral("~/"));
    onRefreshListing();
}

void RemotePanel::setConnected(bool on)
{
    m_connected = on;
    m_connectBtn->setEnabled(!on);
    m_disconnectBtn->setEnabled(on);
    m_stack->setCurrentIndex(on ? 1 : 0);
}

void RemotePanel::onDisconnect()
{
    if (m_listProc) {
        m_listProc->kill();
        m_listProc->deleteLater();
        m_listProc = nullptr;
    }
    if (m_cmdProc) {
        m_cmdProc->kill();
        m_cmdProc->deleteLater();
        m_cmdProc = nullptr;
    }
    m_activeProfile.clear();
    setConnected(false);
    m_state->setText(tr("Disconnected."));
}

void RemotePanel::onRefreshListing()
{
    if (!m_connected || m_listProc)
        return;
    const QString ssh = findSsh();
    const QString path = m_remotePath->text().isEmpty() ? QStringLiteral("~/")
                                                        : m_remotePath->text();
    m_listProc = new QProcess(this);
    connect(m_listProc, &QProcess::finished, this, &RemotePanel::onListingFinished);
    QStringList args = baseSshOptions();
    args << m_profileTargets.value(m_activeProfile);
    args << QStringLiteral("ls -la --time-style=+ %1 2>&1").arg(path);
    m_listProc->start(ssh, args);
}

void RemotePanel::onListingFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_listProc)
        return;
    const QByteArray out = m_listProc->readAllStandardOutput();
    m_listProc->deleteLater();
    m_listProc = nullptr;
    if (status != QProcess::NormalExit || exitCode != 0) {
        m_state->setText(tr("Listing failed: %1").arg(QString::fromUtf8(out.trimmed())));
        return;
    }
    m_remoteFiles->clear();
    const QStringList lines = QString::fromUtf8(out).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        // skip "total N" and permissionless noise
        if (line.startsWith(QLatin1String("total")))
            continue;
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 9)
            continue;
        const bool isDir = parts.at(0).startsWith(QLatin1Char('d'));
        const QString name = parts.at(8);
        if (name == QLatin1String(".") || name == QLatin1String(".."))
            continue;
        auto* item = new QTreeWidgetItem(m_remoteFiles);
        item->setText(0, (isDir ? QStringLiteral("[dir] ") : QString()) + name);
        item->setText(1, isDir ? QString() : parts.at(4));
        item->setData(0, Qt::UserRole, isDir);
        item->setData(1, Qt::UserRole, line);
    }
}

QString RemotePanel::fetchRemote(const QString& remotePath, QString* error)
{
    const QString ssh = findSsh();
    if (ssh.isEmpty()) {
        if (error)
            *error = tr("OpenSSH client not found.");
        return QString();
    }
    QDir tmp = QDir(QDir::tempPath() + QStringLiteral("/codeforge-remote"));
    if (!tmp.exists())
        QDir::temp().mkpath(QStringLiteral("codeforge-remote"));
    const QString local = tmp.filePath(
        QFileInfo(remotePath).fileName().isEmpty() ? QStringLiteral("remote-file")
                                                   : QFileInfo(remotePath).fileName());
    QProcess fetch(this);
    QStringList args = baseSshOptions();
    args << m_profileTargets.value(m_activeProfile);
    args << QStringLiteral("cat") << remotePath;
    fetch.start(ssh, args);
    if (!fetch.waitForFinished(30000)) {
        fetch.kill();
        if (error)
            *error = tr("Timed out fetching %1").arg(remotePath);
        return QString();
    }
    if (fetch.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(fetch.readAllStandardError().trimmed());
        return QString();
    }
    const QByteArray data = fetch.readAllStandardOutput();
    QString err;
    if (!fs::writeAllAtomic(local, data, &err)) {
        if (error)
            *error = err;
        return QString();
    }
    m_tempToRemote.insert(local, remotePath);
    return local;
}

void RemotePanel::onOpenRemoteFile()
{
    QTreeWidgetItem* it = m_remoteFiles->currentItem();
    if (!it || it->data(0, Qt::UserRole).toBool()) {
        QMessageBox::information(this, tr("Remote"), tr("Select a file (not a directory)."));
        return;
    }
    QString error;
    const QString remotePath = it->text(0);
    const QString base = m_remotePath->text();
    const QString fullRemote = base.endsWith(QLatin1Char('/')) ? base + remotePath
                                                               : base + QLatin1Char('/') + remotePath;
    const QString local = fetchRemote(fullRemote, &error);
    if (local.isEmpty()) {
        QMessageBox::warning(this, tr("Remote"), error);
        return;
    }
    emit remoteFileFetched(fullRemote, local);
}

void RemotePanel::onDownloadFile()
{
    QTreeWidgetItem* it = m_remoteFiles->currentItem();
    if (!it || it->data(0, Qt::UserRole).toBool())
        return;
    const QString base = m_remotePath->text();
    const QString fullRemote = base.endsWith(QLatin1Char('/')) ? base + it->text(0)
                                                               : base + QLatin1Char('/') + it->text(0);
    const QString target = QFileDialog::getSaveFileName(this, tr("Download to"),
                                                        QFileInfo(fullRemote).fileName());
    if (target.isEmpty())
        return;
    QString error;
    const QString tmp = fetchRemote(fullRemote, &error);
    if (tmp.isEmpty()) {
        QMessageBox::warning(this, tr("Download"), error);
        return;
    }
    QFile::remove(target);
    if (!QFile::copy(tmp, target)) {
        QMessageBox::warning(this, tr("Download"), tr("Cannot write %1").arg(target));
        return;
    }
    m_state->setText(tr("Downloaded %1 -> %2").arg(fullRemote, target));
}

void RemotePanel::onUploadFile()
{
    const QString local = QFileDialog::getOpenFileName(this, tr("Upload file"));
    if (local.isEmpty() || !m_connected)
        return;
    const QString scp = findScp();
    if (scp.isEmpty()) {
        QMessageBox::warning(this, tr("Remote"), tr("OpenSSH scp not found."));
        return;
    }
    const QString base = m_remotePath->text();
    const QString target = base.endsWith(QLatin1Char('/')) ? base + QFileInfo(local).fileName()
                                                           : base + QLatin1Char('/') + QFileInfo(local).fileName();
    QProcess up(this);
    QStringList args = baseSshOptions();
    const QString port = m_profilePorts.value(m_activeProfile);
    if (!port.isEmpty())
        args << QStringLiteral("-P") << port;
    const QString key = m_profileKeys.value(m_activeProfile);
    if (!key.isEmpty())
        args << QStringLiteral("-i") << key;
    args << local << m_profileTargets.value(m_activeProfile) + QStringLiteral(":") + target;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    up.start(scp, args);
    const bool ok = up.waitForFinished(60000);
    QApplication::restoreOverrideCursor();
    if (!ok || up.exitCode() != 0) {
        QMessageBox::warning(this, tr("Upload"),
                             QString::fromUtf8(up.readAllStandardError().trimmed()));
        return;
    }
    m_state->setText(tr("Uploaded %1 -> %2").arg(local, target));
    onRefreshListing();
}

void RemotePanel::onRunCommand()
{
    if (!m_connected || m_cmdProc)
        return;
    const QString cmd = m_cmdInput->text().trimmed();
    if (cmd.isEmpty())
        return;
    const QString ssh = findSsh();
    m_cmdProc = new QProcess(this);
    connect(m_cmdProc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
        if (!m_cmdProc)
            return;
        const QByteArray out = m_cmdProc->readAllStandardOutput();
        const QByteArray err = m_cmdProc->readAllStandardError();
        m_cmdProc->deleteLater();
        m_cmdProc = nullptr;
        m_cmdOutput->appendPlainText(QStringLiteral("$ %1").arg(m_cmdInput->text()));
        m_cmdOutput->appendPlainText(QString::fromUtf8(out));
        if (!err.isEmpty())
            m_cmdOutput->appendPlainText(QStringLiteral("[stderr] %1").arg(QString::fromUtf8(err)));
        m_cmdOutput->appendPlainText(QStringLiteral("[exit %1]").arg(code));
        Q_UNUSED(st);
    });
    QStringList args = baseSshOptions();
    args << m_profileTargets.value(m_activeProfile);
    args << cmd;
    m_cmdProc->start(ssh, args);
}

RemotePanel::RemoteFile RemotePanel::remoteFileFor(const QString& localTempPath) const
{
    RemoteFile f;
    f.localTempPath = localTempPath;
    if (!m_tempToRemote.contains(localTempPath))
        return f;
    f.valid = true;
    f.profileName = m_activeProfile;
    f.remotePath = m_tempToRemote.value(localTempPath);
    return f;
}

bool RemotePanel::saveBack(const RemoteFile& file, QString* error)
{
    if (!file.valid || !m_profileTargets.contains(file.profileName)) {
        if (error)
            *error = tr("File is not linked to a remote host.");
        return false;
    }
    const QString ssh = findSsh();
    if (ssh.isEmpty()) {
        if (error)
            *error = tr("OpenSSH client not found.");
        return false;
    }
    QFile local(file.localTempPath);
    if (!local.open(QIODevice::ReadOnly)) {
        if (error)
            *error = local.errorString();
        return false;
    }
    QProcess push(this);
    QStringList args = baseSshOptions();
    args << m_profileTargets.value(file.profileName);
    args << QStringLiteral("tee %1 > /dev/null").arg(file.remotePath);
    push.start(ssh, args);
    push.write(local.readAll());
    local.close();
    push.closeWriteChannel();
    if (!push.waitForFinished(30000)) {
        push.kill();
        if (error)
            *error = tr("Timed out saving to %1").arg(file.remotePath);
        return false;
    }
    if (push.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(push.readAllStandardError().trimmed());
        return false;
    }
    emit statusMessage(tr("Saved to %1 (remote)").arg(file.remotePath));
    return true;
}

}  // namespace cf
