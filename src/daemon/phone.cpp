/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "phone.h"

#include "calls.h"

#include <KNotification>
#include <QDBusConnection>

#include "dbusnames.h"

Phone::Phone(const QString &id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{
    QDBusConnection::sessionBus().registerObject(dbusPath(), this, QDBusConnection::ExportScriptableContents);
}

Phone::~Phone()
{
    QDBusConnection::sessionBus().unregisterObject(dbusPath());
}

QString Phone::id() const
{
    return m_id;
}

QString Phone::name() const
{
    return m_name;
}

bool Phone::isReachable() const
{
    return m_reachable;
}

QStringList Phone::addresses() const
{
    return m_addresses;
}

bool Phone::update(const QVariantMap &properties)
{
    const QString name = properties.value(QStringLiteral("name")).toString();
    const bool reachable = properties.value(QStringLiteral("isReachable")).toBool();
    const QStringList addresses = properties.value(QStringLiteral("reachableAddresses")).toStringList();
    // Without loadedPlugins, e.g. as KDE Connect did not answer in time, as it was
    const bool messagesAvailable = reachable
        && (properties.contains(QStringLiteral("loadedPlugins"))
                ? properties.value(QStringLiteral("loadedPlugins")).toStringList().contains(QStringLiteral("kdeconnect_sms"))
                : m_messagesAvailable);
    if (name == m_name && reachable == m_reachable && addresses == m_addresses && messagesAvailable == m_messagesAvailable) {
        return false;
    }

    const bool visible = name != m_name || reachable != m_reachable || messagesAvailable != m_messagesAvailable;
    m_name = name;
    m_reachable = reachable;
    m_messagesAvailable = messagesAvailable;
    m_addresses = addresses;
    Q_EMIT changed();
    return visible;
}

QString Phone::dbusPath() const
{
    return PHONEKOMPANION_PHONES_PATH + QLatin1Char('/') + m_id;
}

Scrcpy *Phone::scrcpy() const
{
    return m_scrcpy;
}

void Phone::setScrcpy(Scrcpy *scrcpy)
{
    m_scrcpy = scrcpy;
}

void Phone::notifyError(const QString &error, const QString &title) const
{
    if (error.isEmpty()) {
        return;
    }
    auto *notification = new KNotification(QStringLiteral("error"));
    notification->setTitle(title.isEmpty() ? m_name : title);
    notification->setText(error);
    notification->sendEvent();
}

bool Phone::messagesAvailable() const
{
    return m_messagesAvailable;
}

bool Phone::callsConnected() const
{
    return m_calls && m_calls->gatewayConnected();
}

Calls *Phone::calls() const
{
    return m_calls;
}

void Phone::setCalls(Calls *calls)
{
    m_calls = calls;
}

#include "moc_phone.cpp"
