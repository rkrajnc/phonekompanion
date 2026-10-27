/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "phone.h"

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
    if (name == m_name && reachable == m_reachable && addresses == m_addresses) {
        return false;
    }

    const bool visible = name != m_name || reachable != m_reachable;
    m_name = name;
    m_reachable = reachable;
    m_addresses = addresses;
    Q_EMIT changed();
    return visible;
}

QString Phone::dbusPath() const
{
    return PHONEKOMPANION_PHONES_PATH + QLatin1Char('/') + m_id;
}

#include "moc_phone.cpp"
