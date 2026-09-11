#pragma once
// Status bar: git branch, Ln/Col, selection, encoding, EOL, language, size,
// read-only state — all interactive like a modern IDE.
#include <QLabel>
#include <QMouseEvent>
#include <QStatusBar>

namespace cf {

class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget* parent = nullptr) : QLabel(parent) {}
    void mousePressEvent(QMouseEvent* e) override
    {
        Q_UNUSED(e);
        emit clicked();
    }
signals:
    void clicked();
};

class StatusBar : public QStatusBar {
    Q_OBJECT
public:
    explicit StatusBar(QWidget* parent = nullptr);

    void setBranch(const QString& branch);
    void setCursorInfo(int line, int col, int selChars, int selLines);
    void setEncoding(const QString& label);
    void setLineEndings(const QString& label);
    void setLanguage(const QString& label);
    void setFileSize(qint64 bytes);
    void setReadOnly(bool ro);
    void setFilePath(const QString& path);
    void clearFile();

signals:
    void branchClicked();
    void encodingClicked();
    void lineEndingsClicked();
    void languageClicked();
    void positionClicked();

private:
    ClickableLabel* m_branch;
    QLabel* m_file;
    ClickableLabel* m_position;
    QLabel* m_selection;
    ClickableLabel* m_encoding;
    ClickableLabel* m_eol;
    ClickableLabel* m_language;
    QLabel* m_size;
    QLabel* m_readOnly;
};

}  // namespace cf
