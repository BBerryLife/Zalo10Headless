#ifndef SERVICECONTROLLER_HPP
#define SERVICECONTROLLER_HPP

#include <QObject>
#include <QVariantMap>

class QTimer;
class ZaloService;
namespace bb { namespace system { class InvokeManager; class InvokeRequest; } }

class ServiceController : public QObject
{
    Q_OBJECT
public:
    explicit ServiceController(QObject *parent = 0);
    ZaloService *service() const { return m_svc; }

private slots:
    void onInvoked(const bb::system::InvokeRequest &request);
    void onPollTimer();
    void onSessionExpired();
    void onHeartbeat();
    void onAboutToQuit();
    void onVoiceCallControl(const QVariantMap &content);
    void onCallInvokeFinished();

private:
    void openAppForCall(const QByteArray &data);

private slots:

private:
    void evaluate();

    ZaloService *m_svc;
    QTimer      *m_poll;
    QTimer      *m_heartbeat;
    bb::system::InvokeManager *m_invoke;
    bool m_active;
    bool m_sessionDead;
    bool m_lastUiAlive;
    qint64 m_lastCallLaunchMs;
};

#endif
