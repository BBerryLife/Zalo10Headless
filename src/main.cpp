// Zalo10Service — service headless: giữ WebSocket Zalo khi app UI đã đóng hẳn để
// vẫn nhận tin và đẩy vào BlackBerry Hub. Dùng lại nguyên ZaloService của app UI.
//
// Các biện pháp an toàn dưới đây rút từ bài học của project Zalo10Headless cũ
// (nơi service từng "chết lặng lẽ" và sinh instance trùng lặp):
//  1. Dùng bb::Application (không phải QCoreApplication) — đúng loại cho headless.
//  2. notify() bọc try/catch: exception ném từ slot (vd std::bad_alloc) không làm
//     std::terminate() cả process.
//  3. Khoá singleton bằng fcntl: không bao giờ có 2 service cùng giữ session/WS.
#include "ServiceController.hpp"

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
        // File riêng, KHÔNG dùng zalo10_runtime.log của UI để 2 process không trộn log.
        QString path = QDir::homePath() + "/zalo10_service.log";
        g_logFile = new QFile(path);
        QFileInfo fi(path);
        QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
        mode |= (fi.exists() && fi.size() > 1024 * 1024) ? QIODevice::Truncate : QIODevice::Append;
        g_logFile->open(mode);
        g_logFile->write(QString("\n===== Zalo10Service started %1 =====\n")
                         .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")).toUtf8());
    }
    const char *lvl = (type == QtWarningMsg) ? "WARN" : (type == QtCriticalMsg) ? "ERROR"
                    : (type == QtFatalMsg) ? "FATAL" : "DEBUG";
    QString line = QString("[%1] [%2] %3\n")
                   .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(lvl)
                   .arg(QString::fromUtf8(msg));
    if (g_logFile->isOpen()) { g_logFile->write(line.toUtf8()); g_logFile->flush(); }
    fprintf(stderr, "%s", line.toUtf8().constData());
    if (type == QtFatalMsg) abort();
}

// Qt không hỗ trợ ném exception ra khỏi event handler: nếu không chặn ở notify(),
// std::terminate() giết cả process mà log không có dòng lỗi nào. Bắt ở đây để
// service chỉ log rồi sống tiếp. Nếu exception lặp dồn dập (>= 20 lần / 300ms) thì
// process đã ở trạng thái bộ nhớ hỏng (cùng 1 event bị dispatch lại liên tục),
// thoát hẳn còn hơn "sống dở chết dở".
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

// Ghi dòng "[SIGNAL] n" vào log khi process sắp chết vì lỗi nghiêm trọng (segfault...),
// để biết chết ở đâu thay vì log đứt đột ngột. Chỉ dùng write() (an toàn trong signal handler),
// rồi trả lại hành vi mặc định và raise lại để hệ thống vẫn xử lý như bình thường.
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
    // Ghi vào socket đã bị đối phương đóng mà không ignore SIGPIPE thì process bị giết lặng lẽ.
    ::signal(SIGPIPE, SIG_IGN);
}

// Giữ fd mở suốt vòng đời process để khoá không bị nhả. Dùng fcntl(F_SETLK) thay vì
// flock() vì flock() không chắc có trên QNX. Lỗi mở file => không chặn (trả true).
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
