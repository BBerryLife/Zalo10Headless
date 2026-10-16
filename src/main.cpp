// Headless service: keeps the Zalo WebSocket alive while the UI app is closed and pushes
// messages to BlackBerry Hub, reusing ZaloService.
// Safeguards: bb::Application (not QCoreApplication); notify() wrapped in try/catch so a slot
// exception (e.g. bad_alloc) doesn't terminate the process; fcntl singleton lock so two
// services never share a session.
#include "ServiceController.hpp"
#include "ZaloServiceUtils.hpp"
#include "LogSanitizer.hpp"

#include <bb/Application>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <openssl/crypto.h>

using namespace bb;

static QFile *g_logFile = 0;

static void serviceMessageHandler(QtMsgType type, const char *msg)
{
    if (!g_logFile) {
        QString path = QDir::homePath() + "/zalo10_service.log";
        g_logFile = new QFile(path);
        QFileInfo fi(path);
        QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
        mode |= (fi.exists() && fi.size() > 1024 * 1024) ? QIODevice::Truncate : QIODevice::Append;
        g_logFile->open(mode);
        g_logFile->write(QString("\n===== Zalo10Service started %1 =====\n")
                         .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).toUtf8());
    }
    QString text = QString::fromUtf8(msg);
    if (type != QtFatalMsg && LogSanitizer::isNoise(text)) return;
    text = LogSanitizer::sanitize(text);
    const char *lvl = (type == QtWarningMsg) ? "WARN" : (type == QtCriticalMsg) ? "ERROR"
                    : (type == QtFatalMsg) ? "FATAL" : "DEBUG";
    QString line = QString("[%1] [%2] %3\n")
                   .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(lvl)
                   .arg(text);
    if (g_logFile->isOpen()) { g_logFile->write(line.toUtf8()); g_logFile->flush(); }
    fprintf(stderr, "%s", line.toUtf8().constData());
    if (type == QtFatalMsg) abort();
}

// Qt can't propagate exceptions out of event handlers: catch in notify() and keep running.
// Repeated exceptions (>= 20 in 300ms) mean corrupted state, so exit instead.
class SafeServiceApplication : public Application
{
public:
    SafeServiceApplication(int &argc, char **argv)
        : Application(argc, argv), m_burstWindowStart(0), m_burstCount(0) {}

    bool notify(QObject *receiver, QEvent *event)
    {
        try {
            return Application::notify(receiver, event);
        } catch (const std::exception &e) {
            qWarning() << "[Service] CAUGHT exception in event handler:" << e.what();
            noteException();
            return false;
        } catch (...) {
            qWarning() << "[Service] CAUGHT unknown exception in event handler";
            noteException();
            return false;
        }
    }

private:
    qint64 m_burstWindowStart;
    int    m_burstCount;

    void noteException()
    {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_burstWindowStart == 0 || (now - m_burstWindowStart) > 300) {
            m_burstWindowStart = now;
            m_burstCount = 1;
            return;
        }
        if (++m_burstCount >= 20) {
            qWarning() << "[Service] runaway exception loop (" << m_burstCount
                       << "in 300ms) - exiting. Notifications resume when the app is opened or the device reboots.";
            ::exit(1);
        }
    }
};

// Log "[SIGNAL] n" on fatal signals using only write() (signal-safe), then restore the
// default handler and re-raise.
static int g_sigLogFd = -1;

static void fatalSignalHandler(int sig)
{
    if (g_sigLogFd >= 0) {
        const char prefix[] = "\n[SIGNAL] process died with signal ";
        ::write(g_sigLogFd, prefix, sizeof(prefix) - 1);
        char tmp[16]; int t = 0; int v = sig;
        if (v == 0) tmp[t++] = '0';
        while (v > 0 && t < 15) { tmp[t++] = (char)('0' + v % 10); v /= 10; }
        char out[18]; int n = 0;
        while (t > 0) out[n++] = tmp[--t];
        out[n++] = '\n';
        ::write(g_sigLogFd, out, n);
    }
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

static void installSignalLogging()
{
    QByteArray lp = (QDir::homePath() + "/zalo10_service.log").toLocal8Bit();
    g_sigLogFd = ::open(lp.constData(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    const int fatal[] = { SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL };
    for (unsigned i = 0; i < sizeof(fatal) / sizeof(fatal[0]); ++i)
        ::signal(fatal[i], fatalSignalHandler);
    ::signal(SIGPIPE, SIG_IGN);
}

// Keep the fd open for the process lifetime so the lock persists. fcntl(F_SETLK) because
// flock() may be missing on QNX. If open fails, don't block.
static bool acquireSingleInstanceLock()
{
    QByteArray path = (QDir::homePath() + "/zalo10_service.lock").toLocal8Bit();
    int fd = ::open(path.constData(), O_RDWR | O_CREAT, 0600);
    if (fd < 0) return true;

    struct flock fl;
    std::memset(&fl, 0, sizeof(fl));
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    if (::fcntl(fd, F_SETLK, &fl) == -1) {
        ::close(fd);
        return false;
    }
    return true;
}

Q_DECL_EXPORT int main(int argc, char **argv)
{
    // QtScript segfaults in the headless process, so all JSON goes through the hand-written parser.
    ZJson::nativeMode() = true;
    qInstallMsgHandler(serviceMessageHandler);
    qDebug() << "[Service] OpenSSL:" << SSLeay_version(SSLEAY_VERSION);

    SafeServiceApplication app(argc, argv);
    installSignalLogging();

    if (!acquireSingleInstanceLock()) {
        qDebug() << "[Service] another instance already running - exiting without touching the session";
        return 0;
    }

    ServiceController controller;
    return SafeServiceApplication::exec();
}
