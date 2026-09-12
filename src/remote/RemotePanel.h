#pragma once
// RemotePanel: SSH remote development via the Windows/OpenSSH client
// (ssh/scp shipped with Windows 10/11 or OpenSSH on other platforms).
// Profiles are stored in settings; authentication uses keys / ssh-agent
// (BatchMode is enforced so a session can never hang on a password prompt).
// Features: connect check, remote file browser, open remote file in a local
// temp copy, save back to the remote host, download/upload, run commands.
#include <QProcess>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;
class QTreeWidget;
class QToolButton;

namespace cf {

class RemotePanel : public QWidget {
    Q_OBJECT
public:
    explicit RemotePanel(QWidget* parent = nullptr);

    // resolved ssh binary ("" when OpenSSH is not installed)
    static QString findSsh();
    static QString findScp();

    // Remote file descriptor used by MainWindow to wire save-back.
    struct RemoteFile {
        QString profileName;
        QString remotePath;
        QString localTempPath;
        bool valid = false;
    };

    // Returns the RemoteFile for a local temp path, or an invalid one.
    RemoteFile remoteFileFor(const QString& localTempPath) const;
    // Push a local temp file back to its remote location. Returns false on error.
    bool saveBack(const RemoteFile& file, QString* error);
    // Download a remote file to a local temp copy and return the temp path.
    QString fetchRemote(const QString& remotePath, QString* error);

signals:
    void statusMessage(const QString& msg);
    void remoteFileFetched(const QString& remotePath, const QString& localTempPath);

private slots:
    void onConnect();
    void onDisconnect();
    void onRefreshListing();
    void onListingFinished(int exitCode, QProcess::ExitStatus status);
    void onOpenRemoteFile();
    void onDownloadFile();
    void onUploadFile();
    void onRunCommand();

private:
    void buildUi();
    void loadProfiles();
    QString profileArg(const QString& profile) const;   // "user@host -p port -i key"
    void setConnected(bool on);

    QComboBox* m_profiles;
    QLineEdit* m_newName;
    QLineEdit* m_newTarget;    // user@host
    QLineEdit* m_newPort;
    QLineEdit* m_newKey;
    QToolButton* m_addProfile;
    QToolButton* m_removeProfile;
    QToolButton* m_connectBtn;
    QToolButton* m_disconnectBtn;
    QLabel* m_state;
    QStackedWidget* m_stack;         // page 0: hint, page 1: browser
    QTreeWidget* m_remoteFiles;
    QLineEdit* m_remotePath;
    QPlainTextEdit* m_cmdOutput;
    QLineEdit* m_cmdInput;
    QToolButton* m_refreshBtn;
    QToolButton* m_openBtn;
    QToolButton* m_downloadBtn;
    QToolButton* m_uploadBtn;
    QToolButton* m_runBtn;

    QStringList m_profileNames;
    QHash<QString, QString> m_profileTargets;   // name -> user@host
    QHash<QString, QString> m_profilePorts;
    QHash<QString, QString> m_profileKeys;
    QString m_activeProfile;
    QHash<QString, QString> m_tempToRemote;     // localTempPath -> remotePath
    QProcess* m_listProc = nullptr;
    QProcess* m_cmdProc = nullptr;
    bool m_connected = false;
};

}  // namespace cf
