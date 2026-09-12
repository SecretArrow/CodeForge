#pragma once
// DiffViewer: side-by-side diff with hunk navigation and merge-lite "copy to
// the other side" actions. Contents come from callers (compare files, git
// diff, external-change comparison).
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

namespace cf {

struct DiffRow;   // from DiffEngine.h
class DiffEngine;

class DiffViewer : public QWidget {
    Q_OBJECT
public:
    explicit DiffViewer(QWidget* parent = nullptr);

    // Set contents and compute the diff. Line endings are normalized for
    // display; the texts are shown read-only.
    void setContents(const QString& titleA, const QString& textA,
                     const QString& titleB, const QString& textB);

    // Copy current hunk to the other side (merge-lite). Returns the edited
    // text of the target side (emits contentsEdited).
    void copyHunkToRight(int hunkIndex);
    void copyHunkToLeft(int hunkIndex);

    QString textA() const { return m_textA; }
    QString textB() const { return m_textB; }

    QString sideAPath;    // used by callers to resolve save targets (may be empty)
    QString sideBPath;

signals:
    void statusMessage(const QString& msg);
    void contentsEdited();   // after a copy-to-side modified a side

public slots:
    void nextHunk();
    void previousHunk();

private slots:
    void onCopyRight();
    void onCopyLeft();
    void onScrollMoved(int value);

private:
    void rebuild();               // re-run the diff on current texts
    void applyHunk(bool toRight, int hunkIndex);
    void colorizeRows();
    void highlightCurrentHunk();
    QString currentSideText(bool right) const;
    void setSideText(bool right, const QString& text);

    QLineEdit* m_titleA;
    QLineEdit* m_titleB;
    QPlainTextEdit* m_editA;
    QPlainTextEdit* m_editB;
    QLabel* m_status;
    QString m_textA;
    QString m_textB;
    QVector<int> m_hunkRowStarts;        // block numbers where hunks begin
    QVector<int> m_rowTypes;             // per-row DiffRow::Type (as int)
    int m_currentHunk = -1;
    bool m_syncing = false;
    bool m_computing = false;
};

}  // namespace cf
