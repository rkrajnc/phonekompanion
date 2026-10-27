/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kdeconnect.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>

#include "phonekompaniond_debug.h"

#define KDECONNECT_SERVICE QStringLiteral("org.kde.kdeconnect")
#define KDECONNECT_PATH QStringLiteral("/modules/kdeconnect")
#define KDECONNECT_DAEMON_INTERFACE QStringLiteral("org.kde.kdeconnect.daemon")
#define KDECONNECT_DEVICE_INTERFACE QStringLiteral("org.kde.kdeconnect.device")
#define KDECONNECT_TELEPHONY_INTERFACE QStringLiteral("org.kde.kdeconnect.device.telephony")

// Keeps the tray and our own D-Bus API responsive if KDE Connect hangs
constexpr int CallTimeout = 2000;

KdeConnect::KdeConnect(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    auto watcher = new QDBusServiceWatcher(KDECONNECT_SERVICE, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        m_running = !newOwner.isEmpty();
        qCDebug(PHONEKOMPANIOND) << "KDE Connect running" << m_running;
        Q_EMIT devicesChanged();
    });
    m_running = bus.interface()->isServiceRegistered(KDECONNECT_SERVICE);

    bus.connect(KDECONNECT_SERVICE, KDECONNECT_PATH, KDECONNECT_DAEMON_INTERFACE, QStringLiteral("deviceListChanged"), this, SIGNAL(devicesChanged()));
    // An empty path matches the signals of every device
    for (const QString &signal : {QStringLiteral("nameChanged"),
                                  QStringLiteral("typeChanged"),
                                  QStringLiteral("reachableChanged"),
                                  QStringLiteral("pairStateChanged"),
                                  QStringLiteral("linksChanged")}) {
        bus.connect(KDECONNECT_SERVICE, QString(), KDECONNECT_DEVICE_INTERFACE, signal, this, SIGNAL(devicesChanged()));
    }
    bus.connect(KDECONNECT_SERVICE, QString(), KDECONNECT_TELEPHONY_INTERFACE, QStringLiteral("callReceived"), this, SLOT(telephonyCallReceived(QDBusMessage)));
}

void KdeConnect::telephonyCallReceived(const QDBusMessage &message)
{
    // From /modules/kdeconnect/devices/<id>/telephony
    const QStringList path = message.path().split(QLatin1Char('/'));
    const QVariantList arguments = message.arguments();
    if (path.size() < 2 || arguments.size() < 3) {
        return;
    }
    Q_EMIT callReceived(path.at(path.size() - 2), arguments.at(0).toString(), arguments.at(1).toString(), arguments.at(2).toString());
}

bool KdeConnect::isRunning() const
{
    return m_running;
}

std::optional<QStringList> KdeConnect::pairedDevices() const
{
    if (!m_running) {
        return QStringList();
    }

    QDBusMessage message = QDBusMessage::createMethodCall(KDECONNECT_SERVICE, KDECONNECT_PATH, KDECONNECT_DAEMON_INTERFACE, QStringLiteral("devices"));
    message << false << true; // onlyReachable, onlyPaired
    // Don't start KDE Connect; it has its own autostart
    message.setAutoStartService(false);
    const QDBusReply<QStringList> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, CallTimeout);
    if (!reply.isValid()) {
        qCWarning(PHONEKOMPANIOND) << "Could not list the KDE Connect devices" << reply.error().message();
        return std::nullopt;
    }
    return reply.value();
}

QVariantMap KdeConnect::deviceProperties(const QString &id) const
{
    QDBusMessage message = QDBusMessage::createMethodCall(KDECONNECT_SERVICE,
                                                          KDECONNECT_PATH + QStringLiteral("/devices/") + id,
                                                          QStringLiteral("org.freedesktop.DBus.Properties"),
                                                          QStringLiteral("GetAll"));
    message << KDECONNECT_DEVICE_INTERFACE;
    message.setAutoStartService(false);
    const QDBusReply<QVariantMap> reply = QDBusConnection::sessionBus().call(message, QDBus::Block, CallTimeout);
    if (!reply.isValid()) {
        qCWarning(PHONEKOMPANIOND) << "Could not read the KDE Connect device" << id << reply.error().message();
        return {};
    }
    return reply.value();
}

#include "moc_kdeconnect.cpp"
