#include "tools/ProfilerPanel.h"

#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/FileUtils.h"
#include "themes/ThemeManager.h"
#include "ui/Icons.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#include <functional>

namespace cf {

namespace {
constexpr int kMaxSamples = 240;

class ChartWidget : public QWidget {
public:
    using QWidget::QWidget;
    std::function<void(QPainter&)> painter;

protected:
    void paintEvent(QPaintEvent* e) override
    {
        Q_UNUSED(e);
        QPainter p(this);
        if (painter)
            painter(p);
    }
};
}  // namespace

ProfilerPanel::ProfilerPanel(QWidget* parent)
    : QWidget(parent)
{
#ifdef Q_OS_WIN
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_cores = si.dwNumberOfProcessors > 0 ? int(si.dwNumberOfProcessors) : 1;
#else
    m_cores = qMax(1, QThread::idealThreadCount());
#endif
    buildUi();
    resetSeries();
}

void ProfilerPanel::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    auto* row = new QHBoxLayout;
    m_target = new QLineEdit(this);
    m_target->setPlaceholderText(tr("Executable to profile (empty = attach by PID)"));
    m_args = new QLineEdit(this);
    m_args->setPlaceholderText(tr("args"));
    m_args->setMaximumWidth(180);
    m_pid = new QLineEdit(this);
    m_pid->setPlaceholderText(tr("PID"));
    m_pid->setMaximumWidth(90);
    m_startBtn = new QToolButton(this);
    m_startBtn->setText(tr("Start"));
    m_startBtn->setIcon(Icons::icon(Icons::Name::Gauge));
    m_startBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_stopBtn = new QToolButton(this);
    m_stopBtn->setText(tr("Stop"));
    m_stopBtn->setEnabled(false);
    m_exportBtn = new QToolButton(this);
    m_exportBtn->setText(tr("Export CSV"));
    m_exportBtn->setEnabled(false);
    row->addWidget(m_target, 2);
    row->addWidget(m_args);
    row->addWidget(new QLabel(tr("or"), this));
    row->addWidget(m_pid);
    row->addWidget(m_startBtn);
    row->addWidget(m_stopBtn);
    row->addWidget(m_exportBtn);
    layout->addLayout(row);

    auto* chart = new ChartWidget(this);
    chart->setMinimumHeight(160);
    chart->painter = [this](QPainter& p) { drawChart(p); };
    m_chart = chart;
    layout->addWidget(m_chart, 1);

    m_values = new QLabel(tr("Ready."), this);
    m_error = new QLabel(this);
    layout->addWidget(m_values);
    layout->addWidget(m_error);

    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, &QTimer::timeout, this, &ProfilerPanel::onTick);

    connect(m_startBtn, &QToolButton::clicked, this, &ProfilerPanel::onStart);
    connect(m_stopBtn, &QToolButton::clicked, this, &ProfilerPanel::onStop);
    connect(m_exportBtn, &QToolButton::clicked, this, &ProfilerPanel::onExport);
}

void ProfilerPanel::resetSeries()
{
    m_cpu.clear();
    m_ram.clear();
    m_lastCpuMs = -1.0;
    m_lastWallMs = -1;
}

bool ProfilerPanel::sampleProcess(quint64 pid, double* cpuMsTotal, qint64* rssBytes)
{
    if (pid == 0)
        return false;
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return false;
    FILETIME c, e, k, u;
    if (!GetProcessTimes(h, &c, &e, &k, &u)) {
        CloseHandle(h);
        return false;
    }
    const quint64 cpuMs =
        (static_cast<quint64>(k.dwHighDateTime) << 32 | k.dwLowDateTime) / 10000
        + (static_cast<quint64>(u.dwHighDateTime) << 32 | u.dwLowDateTime) / 10000;
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (!K32GetProcessMemoryInfo(h, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                 sizeof(pmc))) {
        CloseHandle(h);
        return false;
    }
    const qint64 rss = static_cast<qint64>(pmc.WorkingSetSize);
    CloseHandle(h);
    *cpuMsTotal = static_cast<double>(cpuMs);
    *rssBytes = rss;
    return true;
#else
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly))
        return false;
    const QStringList parts =
        QString::fromUtf8(stat.readAll()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 24)
        return false;
    const quint64 utime = parts.at(13).toULongLong();
    const quint64 stime = parts.at(14).toULongLong();
    *cpuMsTotal = static_cast<double>(utime + stime) * 10.0;   // jiffies(100Hz) -> ms
    QFile statm(QStringLiteral("/proc/%1/statm").arg(pid));
    qint64 rss = 0;
    if (statm.open(QIODevice::ReadOnly)) {
        const QStringList m =
            QString::fromUtf8(statm.readAll()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (m.size() >= 2)
            rss = m.at(1).toLongLong() * 4096;
    }
    *rssBytes = rss;
    return true;
#endif
}

void ProfilerPanel::onStart()
{
    quint64 pid = m_pid->text().trimmed().toULongLong();
    if (pid == 0 && !m_target->text().trimmed().isEmpty()) {
        const QString exe = m_target->text().trimmed();
        if (!QFile::exists(exe)) {
            m_error->setText(tr("Executable not found: %1").arg(exe));
            return;
        }
        auto* proc = new QProcess(this);
        proc->setProcessChannelMode(QProcess::ForwardedChannels);
        connect(proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                m_error->setText(tr("Failed to start the target process."));
        });
        connect(proc, &QProcess::finished, this, [this, proc](int code, QProcess::ExitStatus) {
            m_error->setText(tr("Target exited (code %1).").arg(code));
            if (static_cast<quint64>(proc->processId()) == m_pidValue)
                onStop();
            proc->deleteLater();
        });
        proc->start(exe, m_args->text().split(QLatin1Char(' '), Qt::SkipEmptyParts));
        if (!proc->waitForStarted(3000)) {
            m_error->setText(tr("Failed to start %1").arg(exe));
            proc->deleteLater();
            return;
        }
        pid = static_cast<quint64>(proc->processId());
    }
    if (pid == 0) {
        m_error->setText(tr("Enter a PID or an executable."));
        return;
    }
    m_pidValue = pid;
    m_pid->setText(QString::number(pid));
    resetSeries();
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_exportBtn->setEnabled(false);
    m_error->clear();
    m_timer->start();
}

void ProfilerPanel::onStop()
{
    m_timer->stop();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_exportBtn->setEnabled(!m_cpu.isEmpty());
    m_values->setText(tr("Stopped. %1 sample(s).").arg(m_cpu.size()));
}

void ProfilerPanel::onTick()
{
    double cpuRaw = 0;
    qint64 rss = 0;
    if (!sampleProcess(m_pidValue, &cpuRaw, &rss)) {
        m_error->setText(tr("Process %1 is gone.").arg(m_pidValue));
        onStop();
        return;
    }
    const qint64 wall = QDateTime::currentMSecsSinceEpoch();
    if (m_lastWallMs > 0) {
        const double wallDelta = wall - m_lastWallMs;
        const double cpuDelta = cpuRaw - m_lastCpuMs;
        const double percent = wallDelta > 0 ? (cpuDelta / wallDelta) * 100.0 / m_cores : 0;
        m_cpu.append(qBound(0.0, percent, m_cores * 100.0));
    }
    m_lastCpuMs = cpuRaw;
    m_lastWallMs = wall;
    m_ram.append(rss / (1024.0 * 1024.0));
    while (m_cpu.size() > kMaxSamples)
        m_cpu.removeFirst();
    while (m_ram.size() > kMaxSamples)
        m_ram.removeFirst();
    const double curCpu = m_cpu.isEmpty() ? 0 : m_cpu.last();
    m_values->setText(tr("PID %1 — CPU %2% · RAM %3 MB")
                          .arg(m_pidValue)
                          .arg(curCpu, 0, 'f', 1)
                          .arg(m_ram.last(), 0, 'f', 1));
    m_chart->update();
}

void ProfilerPanel::onExport()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"),
                                                      QStringLiteral("profile.csv"),
                                                      tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString csv = QStringLiteral("sample,cpu_percent,ram_mb\n");
    for (int i = 0; i < m_cpu.size(); ++i)
        csv += QStringLiteral("%1,%2,%3\n")
                   .arg(i)
                   .arg(m_cpu.at(i), 0, 'f', 2)
                   .arg(m_ram.at(i), 0, 'f', 2);
    QString err;
    if (!fs::writeAllAtomic(path, csv.toUtf8(), &err)) {
        QMessageBox::warning(this, tr("Export"), err);
        return;
    }
    m_values->setText(tr("Exported to %1").arg(path));
}

void ProfilerPanel::drawChart(QPainter& p)
{
    p.fillRect(rect(), palette().window());
    const Theme& t = ThemeManager::instance().currentTheme();
    const QColor grid = t.color(QStringLiteral("border"), QColor(0x30, 0x30, 0x30));
    const QColor cpuColor = t.color(QStringLiteral("accent"), QColor(0x58, 0xa6, 0xff));
    const QColor ramColor = t.color(QStringLiteral("syntax.string"), QColor(0x3f, 0xb9, 0x50));

    const int w = width(), h = height();
    p.setPen(grid);
    for (int i = 1; i < 4; ++i)
        p.drawLine(0, h * i / 4, w, h * i / 4);

    auto plot = [&p, w, h, grid](const QVector<double>& series, double maxVal,
                                 const QColor& color) {
        if (series.size() < 2 || maxVal <= 0)
            return;
        p.setPen(QPen(color, 1.6));
        QPainterPath path;
        for (int i = 0; i < series.size(); ++i) {
            const double x = w * i / double(kMaxSamples - 1);
            const double y = h - (h - 6) * qBound(0.0, series.at(i), maxVal) / maxVal - 3;
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }
        p.drawPath(path);
    };
    const double cpuMax = qMax(100.0, m_cores * 100.0);
    double ramMax = 1;
    for (double v : m_ram)
        ramMax = qMax(ramMax, v);
    plot(m_cpu, cpuMax, cpuColor);
    plot(m_ram, ramMax, ramColor);

    p.setPen(grid);
    p.drawText(6, 14, QStringLiteral("CPU (max %1%) — RAM (max %2 MB)").arg(cpuMax, 0, 'f', 0).arg(ramMax, 0, 'f', 0));
}

}  // namespace cf
