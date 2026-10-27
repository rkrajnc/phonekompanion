/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "bluetooth.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>

#include "phonekompaniond_debug.h"

#define TELEPHONY_SERVICE QStringLiteral("org.pipewire.Telephony")
#define TELEPHONY_PATH QStringLiteral("/org/pipewire/Telephony")
#define GATEWAY_INTERFACE QStringLiteral("org.pipewire.Telephony.AudioGateway1")
#define OBJECT_MANAGER_INTERFACE QStringLiteral("org.freedesktop.DBus.ObjectManager")
#define BLUEZ_SERVICE QStringLiteral("org.bluez")
#define BLUEZ_DEVICE_INTERFACE QStringLiteral("org.bluez.Device1")
#define HFP_AG_UUID QStringLiteral("0000111f-0000-1000-8000-00805f9b34fb")
#define SCREENSAVER_SERVICE QStringLiteral("org.freedesktop.ScreenSaver")
#define SCREENSAVER_PATH QStringLiteral("/ScreenSaver")

Bluetooth::Bluetooth(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    auto *watcher = new QDBusServiceWatcher(TELEPHONY_SERVICE, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &Bluetooth::refreshGateways);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &Bluetooth::clearGateways);
    // The root ObjectManager announces the gateways; each gateway's own one its calls
    bus.connect(TELEPHONY_SERVICE, TELEPHONY_PATH, OBJECT_MANAGER_INTERFACE, QStringLiteral("InterfacesAdded"), this, SLOT(interfacesAdded(QDBusMessage)));
    bus.connect(TELEPHONY_SERVICE, TELEPHONY_PATH, OBJECT_MANAGER_INTERFACE, QStringLiteral("InterfacesRemoved"), this, SLOT(interfacesRemoved(QDBusMessage)));
    refreshPairedPhones();
    refreshGateways();

    bus.connect(SCREENSAVER_SERVICE, SCREENSAVER_PATH, SCREENSAVER_SERVICE, QStringLiteral("ActiveChanged"), this, SLOT(screenLockChanged(bool)));
    QDBusMessage request = QDBusMessage::createMethodCall(SCREENSAVER_SERVICE, SCREENSAVER_PATH, SCREENSAVER_SERVICE, QStringLiteral("GetActive"));
    auto *lockWatcher = new QDBusPendingCallWatcher(bus.asyncCall(request), this);
    connect(lockWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<bool> reply = *watcher;
        if (!reply.isError()) {
            screenLockChanged(reply.value());
        }
    });
}

bool Bluetooth::telephonyAvailable() const
{
    return m_telephonyAvailable;
}

bool Bluetooth::pairedPhonesLoaded() const
{
    return m_pairedPhonesLoaded;
}

QHash<QString, QString> Bluetooth::pairedPhones() const
{
    return m_pairedPhones;
}

QString Bluetooth::phoneObject(const QString &address) const
{
    return m_phoneObjects.value(address);
}

QHash<QString, QString> Bluetooth::gateways() const
{
    return m_gateways;
}

int Bluetooth::phoneCount() const
{
    return m_phoneCount;
}

void Bluetooth::setPhoneCount(int count)
{
    if (m_phoneCount != count) {
        m_phoneCount = count;
        Q_EMIT changed();
    }
}

bool Bluetooth::isLocked() const
{
    return m_locked;
}

const KPeople::PersonsModel &Bluetooth::persons() const
{
    return m_persons;
}

QDBusPendingCallWatcher *Bluetooth::getManagedObjects(const QDBusConnection &bus,
                                                      const QString &service,
                                                      const QString &path,
                                                      QObject *context,
                                                      const std::function<void(const ObjectMap &)> &handler)
{
    QDBusMessage request = QDBusMessage::createMethodCall(service, path, OBJECT_MANAGER_INTERFACE, QStringLiteral("GetManagedObjects"));
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(request), context);
    connect(watcher, &QDBusPendingCallWatcher::finished, context, [service, path, handler](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusMessage reply = watcher->reply();
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            qCWarning(PHONEKOMPANIOND) << "GetManagedObjects failed for" << service << path << reply.errorMessage();
            return;
        }
        handler(qdbus_cast<ObjectMap>(reply.arguments().constFirst()));
    });
    return watcher;
}

void Bluetooth::refreshPairedPhones()
{
    // The reply of a request already sent may predate the change, so ask again after it
    if (m_pairedPhonesRequest) {
        m_pairedPhonesStale = true;
        return;
    }
    m_pairedPhonesRequest = getManagedObjects(QDBusConnection::systemBus(), BLUEZ_SERVICE, QStringLiteral("/"), this, [this](const ObjectMap &objects) {
        m_pairedPhonesRequest.clear();
        m_pairedPhones.clear();
        m_phoneObjects.clear();
        for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
            const QVariantMap properties = it.value().value(BLUEZ_DEVICE_INTERFACE);
            if (properties.value(QStringLiteral("Paired")).toBool() && properties.value(QStringLiteral("UUIDs")).toStringList().contains(HFP_AG_UUID)) {
                const QString address = properties.value(QStringLiteral("Address")).toString().toUpper();
                m_pairedPhones.insert(address, properties.value(QStringLiteral("Alias")).toString());
                m_phoneObjects.insert(address, it.key().path());
            }
        }
        qCDebug(PHONEKOMPANIOND) << "Paired Bluetooth phones" << m_pairedPhones;
        m_pairedPhonesLoaded = true;
        Q_EMIT changed();
        if (m_pairedPhonesStale) {
            m_pairedPhonesStale = false;
            refreshPairedPhones();
        }
    });
}

void Bluetooth::refreshGateways()
{
    getManagedObjects(QDBusConnection::sessionBus(), TELEPHONY_SERVICE, TELEPHONY_PATH, this, [this](const ObjectMap &objects) {
        m_telephonyAvailable = true;
        for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
            addGateway(it.key().path(), it.value());
        }
        Q_EMIT changed();
    });
}

void Bluetooth::clearGateways()
{
    qCDebug(PHONEKOMPANIOND) << TELEPHONY_SERVICE << "disappeared";
    m_telephonyAvailable = false;
    m_gateways.clear();
    Q_EMIT changed();
}

bool Bluetooth::addGateway(const QString &path, const InterfaceMap &interfaces)
{
    if (!interfaces.contains(GATEWAY_INTERFACE)) {
        return false;
    }
    const QString address = interfaces.value(GATEWAY_INTERFACE).value(QStringLiteral("Address")).toString().toUpper();
    if (m_gateways.value(path) == address) {
        return false;
    }
    qCDebug(PHONEKOMPANIOND) << "Bluetooth gateway" << address << "appeared at" << path;
    m_gateways.insert(path, address);
    if (!m_pairedPhones.contains(address)) {
        refreshPairedPhones();
    }
    return true;
}

void Bluetooth::interfacesAdded(const QDBusMessage &message)
{
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() < 2) {
        return;
    }
    const QString path = qdbus_cast<QDBusObjectPath>(arguments.at(0)).path();
    if (addGateway(path, qdbus_cast<InterfaceMap>(arguments.at(1)))) {
        Q_EMIT changed();
    }
}

void Bluetooth::interfacesRemoved(const QDBusMessage &message)
{
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() < 2) {
        return;
    }
    const QString path = qdbus_cast<QDBusObjectPath>(arguments.at(0)).path();
    if (arguments.at(1).toStringList().contains(GATEWAY_INTERFACE) && m_gateways.remove(path)) {
        qCDebug(PHONEKOMPANIOND) << "Bluetooth gateway at" << path << "disappeared";
        Q_EMIT changed();
    }
}

void Bluetooth::screenLockChanged(bool locked)
{
    if (m_locked != locked) {
        m_locked = locked;
        Q_EMIT lockedChanged();
    }
}

#include "moc_bluetooth.cpp"
