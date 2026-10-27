/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "phonefeature.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>

#include "dbuscall.h"
#include "dbusnames.h"

PhoneFeature::PhoneFeature(const QString &name, const QString &interface, QObject *parent, bool ownSignals)
    : QObject(parent)
    , m_name(name)
    , m_interface(interface)
    , m_ownSignals(ownSignals)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    auto watcher = new QDBusServiceWatcher(PHONEKOMPANION_SERVICE, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        // A new daemon knows nothing of the old one's state, and answers to the old one are dropped
        ++m_generation;
        reset();
        if (!newOwner.isEmpty()) {
            readAll();
        }
    });
    // Phones come and go with KDE Connect
    bus.connect(PHONEKOMPANION_SERVICE, PHONEKOMPANION_PHONES_PATH, PHONEKOMPANION_PHONES_INTERFACE, QStringLiteral("phonesChanged"), this, SLOT(readStatus()));
}

QString PhoneFeature::phoneId() const
{
    return m_phoneId;
}

void PhoneFeature::setPhoneId(const QString &phoneId)
{
    if (m_phoneId == phoneId) {
        return;
    }
    connectSignals(false);
    m_phoneId = phoneId;
    ++m_generation;
    reset();
    connectSignals(true);
    Q_EMIT phoneIdChanged();
    readAll();
}

bool PhoneFeature::isAvailable() const
{
    return !m_status.isEmpty();
}

QVariantMap PhoneFeature::settings() const
{
    return m_settings;
}

void PhoneFeature::setSetting(const QString &key, const QVariant &value)
{
    call(QStringLiteral("setSetting"), {key, QVariant::fromValue(QDBusVariant(value))});
}

QVariant PhoneFeature::status(const QString &key) const
{
    return m_status.value(key);
}

bool PhoneFeature::hasPhone() const
{
    return !m_phoneId.isEmpty();
}

void PhoneFeature::call(const QString &method, const QVariantList &arguments, const std::function<void(const QDBusMessage &)> &callback)
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    auto watcher = new QDBusPendingCallWatcher(callDaemon(path(), m_interface, method, arguments), this);
    const quint64 generation = m_generation;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, callback](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (generation == m_generation && callback) {
            callback(watcher->reply());
        }
    });
}

void PhoneFeature::callAction(const QString &method, const QVariantList &arguments)
{
    call(method, arguments, [this](const QDBusMessage &reply) {
        const QString message = reply.type() == QDBusMessage::ReplyMessage ? reply.arguments().value(0).toString() : reply.errorMessage();
        if (!message.isEmpty()) {
            Q_EMIT error(message);
        }
    });
}

void PhoneFeature::readMore()
{
}

void PhoneFeature::reset()
{
    m_status.clear();
    m_settings.clear();
    Q_EMIT statusChanged();
    Q_EMIT settingsChanged();
}

QList<PhoneFeature::SignalSlot> PhoneFeature::signalSlots() const
{
    return {};
}

void PhoneFeature::statusRead()
{
}

void PhoneFeature::readStatus()
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    const quint64 statusGeneration = ++m_statusGeneration;
    auto watcher = new QDBusPendingCallWatcher(getAllProperties(path(), m_interface), this);
    const quint64 generation = m_generation;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, statusGeneration](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (generation != m_generation || statusGeneration != m_statusGeneration) {
            return;
        }
        const QDBusMessage reply = watcher->reply();
        const QVariantMap status = reply.type() == QDBusMessage::ReplyMessage ? qdbus_cast<QVariantMap>(reply.arguments().value(0)) : QVariantMap();
        if (status != m_status) {
            m_status = status;
            Q_EMIT statusChanged();
        }
        statusRead();
    });
}

void PhoneFeature::readSettings()
{
    call(QStringLiteral("settings"), {}, [this](const QDBusMessage &reply) {
        if (reply.type() == QDBusMessage::ReplyMessage) {
            m_settings = qdbus_cast<QVariantMap>(reply.arguments().value(0));
            Q_EMIT settingsChanged();
        }
    });
}

QString PhoneFeature::path() const
{
    const QString phone = PHONEKOMPANION_PHONES_PATH + QLatin1Char('/') + m_phoneId;
    return m_name.isEmpty() ? phone : phone + QLatin1Char('/') + m_name;
}

void PhoneFeature::connectSignals(bool connect)
{
    if (m_phoneId.isEmpty() || !m_ownSignals) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    QList<SignalSlot> signalSlots = this->signalSlots();
    signalSlots.append({SignalSlot("statusChanged", SLOT(readStatus())), SignalSlot("settingsChanged", SLOT(readSettings()))});
    for (const auto &[signal, slot] : std::as_const(signalSlots)) {
        if (connect) {
            bus.connect(PHONEKOMPANION_SERVICE, path(), m_interface, QLatin1String(signal), this, slot);
        } else {
            bus.disconnect(PHONEKOMPANION_SERVICE, path(), m_interface, QLatin1String(signal), this, slot);
        }
    }
}

void PhoneFeature::readAll()
{
    readStatus();
    if (m_ownSignals) {
        readSettings();
    }
    readMore();
}

#include "moc_phonefeature.cpp"
