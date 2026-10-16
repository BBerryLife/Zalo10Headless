#include "core/ServiceController.hpp"
#include "core/ZaloService.hpp"
#include "core/ServiceHandoff.hpp"
#include "core/HubIntegration.hpp"
#include "utils/ZaloServiceUtils.hpp"

#include <bb/system/InvokeManager>
#include <bb/system/InvokeRequest>
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>

static const int UI_POLL_MS = 4000;

ServiceController::ServiceController(QObject *parent)
    : QObject(parent),
      m_svc(new ZaloService(this)),
      m_poll(new QTimer(this)),
      m_heartbeat(new QTimer(this)),
      m_invoke(new bb::system::InvokeManager(this)),
      m_active(false), m_sessionDead(false), m_lastUiAlive(false)
{
    m_svc->setAppForeground(false);

    connect(m_svc, SIGNAL(sessionExpired()), this, SLOT(onSessionExpired()));
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
    if (request.action() == QLatin1String("bb.action.OPEN")
        || request.action() == QLatin1String("bb.action.VIEW")) {
        bb::system::InvokeRequest fwd;
        fwd.setTarget("com.BerryLife.Zalo10.invoke");
        fwd.setAction(request.action());
        fwd.setMimeType(request.mimeType());
        fwd.setUri(request.uri());
        fwd.setData(request.data());
        m_invoke->invoke(fwd);
        return;
    }
    evaluate();
}

void ServiceController::onPollTimer() { evaluate(); }

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
