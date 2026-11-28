#include "core/ServiceController.hpp"
#include "core/ZaloService.hpp"
#include "core/ServiceHandoff.hpp"
#include "core/HubIntegration.hpp"
#include "utils/ZaloServiceUtils.hpp"
#include "utils/json/ZaloMessageJson.hpp"

#include <bb/system/InvokeManager>
#include <bb/system/InvokeRequest>
#include <bb/system/InvokeTargetReply>
#include <QTimer>
#include <QCoreApplication>
#include <QDateTime>
#include <QUrl>
#include <QDebug>

static const int UI_POLL_MS = 4000;

ServiceController::ServiceController(QObject *parent)
    : QObject(parent),
      m_svc(new ZaloService(this)),
      m_poll(new QTimer(this)),
      m_heartbeat(new QTimer(this)),
      m_invoke(new bb::system::InvokeManager(this)),
      m_active(false), m_sessionDead(false), m_lastUiAlive(false), m_lastCallLaunchMs(0)
{
    m_svc->setAppForeground(false);

    connect(m_svc, SIGNAL(sessionExpired()), this, SLOT(onSessionExpired()));
    connect(m_svc, SIGNAL(voiceCallControl(QVariantMap)), this, SLOT(onVoiceCallControl(QVariantMap)));
    connect(m_invoke, SIGNAL(invoked(const bb::system::InvokeRequest&)),
            this,     SLOT(onInvoked(const bb::system::InvokeRequest&)));
    connect(m_poll, SIGNAL(timeout()), this, SLOT(onPollTimer()));
    m_poll->start(UI_POLL_MS);

    connect(m_heartbeat, SIGNAL(timeout()), this, SLOT(onHeartbeat()));
    m_heartbeat->start(120000);
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(onAboutToQuit()));

    qDebug() << "[Service] started, pid file:" << ServiceHandoff::pidFilePath();
    evaluate();
}

void ServiceController::onInvoked(const bb::system::InvokeRequest &request)
{

    if (HubIntegration::isItemAction(request.action())) {
        QVariantMap attrs = jsonToMap(request.data()).value("attributes").toMap();
        QString threadId = attrs.value("sourceId").toString();
        if (threadId.isEmpty()) threadId = attrs.value("messageid").toString();
        m_svc->handleHubAction(request.action(), threadId);
        return;
    }
    if (request.action() == QLatin1String("com.BerryLife.Zalo10.service.OPEN_UI")) {
        // The app (minimized, incoming call) cannot bring itself forward: it asks here, and this OPEN comes from
        // another process, like a Hub tap.
        qDebug() << "[Service] OPEN_UI -> bringing the app to the front";
        openAppForCall(QByteArray("zalo10:call"));
        return;
    }
    if (request.action() == QLatin1String("bb.action.OPEN")
        || request.action() == QLatin1String("bb.action.VIEW")) {
        // Hub tap. The service has no window, so it cannot show the reply card itself: it always hands the tap to the
        // app (OPEN on the app target, which also declares VIEW). With "Reply Card in Hub" on (Zalo10 Settings >
        // Notifications, default on) the app then opens the reply card for a chat item (ApplicationUI::onInvoked());
        // with it off, and for the Feeds item, the app goes straight to the chat / the Feeds tab.
        qDebug() << "[Service] Hub tap action=" << request.action() << "-> forwarding to the app";

        bb::system::InvokeRequest fwd;
        fwd.setTarget("com.BerryLife.Zalo10.invoke");
        fwd.setAction("bb.action.OPEN");
        fwd.setMimeType(request.mimeType());
        fwd.setUri(request.uri());
        fwd.setData(request.data());
        bb::system::InvokeTargetReply *reply = m_invoke->invoke(fwd);
        if (reply) {
            connect(reply, SIGNAL(finished()), reply, SLOT(deleteLater()));
        } else {
            qWarning() << "[Service] could not forward the Hub tap to the app";
        }
        return;
    }
    evaluate();
}

void ServiceController::onPollTimer() { evaluate(); }

// Incoming call while the app is closed: the service holds the WebSocket, so it is the one that sees the call
// "request". It opens the app and passes the frame along; the app (call screen + voice engine) takes it from there.
void ServiceController::onVoiceCallControl(const QVariantMap &content)
{
    if (content.value("act").toString() != QLatin1String("request")) return;
    if (ServiceHandoff::isUiAlive()) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastCallLaunchMs > 0 && now - m_lastCallLaunchMs < 5000) return;
    m_lastCallLaunchMs = now;

    qDebug() << "[Service] incoming call while the app is closed -> opening the app";
    openAppForCall(QByteArray("zalo10:call|") + variantToJsonCompact(QVariant(content)));
}

// Same shape as the Hub-tap forward above (mime, no uri), which is known to reach the app.
void ServiceController::openAppForCall(const QByteArray &data)
{
    bb::system::InvokeRequest req;
    req.setTarget("com.BerryLife.Zalo10.invoke");
    req.setAction("bb.action.OPEN");
    req.setMimeType("application/vnd.zalo10.hub.chat");
    req.setData(data);
    bb::system::InvokeTargetReply *reply = m_invoke->invoke(req);
    if (reply) connect(reply, SIGNAL(finished()), this, SLOT(onCallInvokeFinished()));
    else qWarning() << "[Service] could not open the app for the call";
}

void ServiceController::onCallInvokeFinished()
{
    bb::system::InvokeTargetReply *reply = qobject_cast<bb::system::InvokeTargetReply*>(sender());
    if (!reply) return;
    qDebug() << "[Service] open-app-for-call reply error=" << (int)reply->error();
    reply->deleteLater();
}

void ServiceController::onHeartbeat()
{
    qDebug() << "[Service] alive: active =" << m_active << "sessionDead =" << m_sessionDead
             << "uiAlive =" << ServiceHandoff::isUiAlive()
             << "loggedIn =" << m_svc->property("loggedIn")
             << "suspended =" << m_svc->realtimeSuspended();
}

void ServiceController::onAboutToQuit()
{
    qDebug() << "[Service] aboutToQuit - process is being stopped (e.g. SIGTERM from the system)";
}

void ServiceController::onSessionExpired()
{
    qWarning() << "[Service] session expired - idle until user logs in via the app";
    m_sessionDead = true;
    if (m_active) { m_svc->suspendRealtime(); m_active = false; }
}

void ServiceController::evaluate()
{
    bool ui = ServiceHandoff::isUiAlive();

    if (m_lastUiAlive && !ui) m_sessionDead = false;
    m_lastUiAlive = ui;

    if (ui) {
        if (m_active) {
            qDebug() << "[Service] UI is running -> handing WebSocket over";
            m_svc->suspendRealtime();
            m_active = false;
        }
        return;
    }

    if (m_active || m_sessionDead) return;

    qDebug() << "[Service] UI not running -> taking WebSocket";
    m_active = true;
    bool resumed = m_svc->resumeRealtime();
    if (!resumed) {
        qDebug() << "[Service] no saved session - idle";
        m_active = false;
        m_sessionDead = true;
    }
}
