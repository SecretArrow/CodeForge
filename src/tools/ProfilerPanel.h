#pragma once
// ProfilerPanel: process performance monitor. Launch a target or attach to a
// PID and sample CPU% / RAM at a fixed interval:
//   Windows: OpenProcess + GetProcessTimes + K32GetProcessMemoryInfo (psapi)
//   Linux:   /proc/<pid>/stat + /proc/<pid>/statm
// Results are plotted live and exportable as CSV.
#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;
class QTimer;

namespace cf {

class ProfilerPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProfilerPanel(QWidget* parent = nullptr);

    // Pure sampler used by the timer: reports the process's cumulative CPU
    // milliseconds and resident set size in bytes. Returns false when the
    // process is gone. Unit-testable against a live process (tests spawn one).
    static bool sampleProcess(quint64 pid, double* cpuMsTotal, qint64* rssBytes);

private slots:
    void onStart();
    void onStop();
    void onTick();
    void onExport();

private:
    void buildUi();
    void resetSeries();
    void drawChart(QPainter& p);

    QWidget* m_chart;
    QLineEdit* m_target;
    QLineEdit* m_args;
    QLineEdit* m_pid;
    QLabel* m_values;
    QLabel* m_error;
    QToolButton* m_startBtn;
    QToolButton* m_stopBtn;
    QToolButton* m_exportBtn;
    QTimer* m_timer;

    quint64 m_pidValue = 0;
    double m_lastCpuMs = -1.0;      // cumulative CPU ms at previous sample
    qint64 m_lastWallMs = -1;
    QVector<double> m_cpu;          // percent per sample
    QVector<double> m_ram;          // MB per sample
    int m_cores = 1;
};

}  // namespace cf
