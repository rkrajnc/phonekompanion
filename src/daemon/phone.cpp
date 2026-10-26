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

bool Phone::update(const QVariantMap &properties)
{
    const QString name = properties.value(QStringLiteral("name")).toString();
    const bool reachable = properties.value(QStringLiteral("isReachable")).toBool();
    if (name == m_name && reachable == m_reachable) {
        return false;
    }

    m_name = name;
    m_reachable = reachable;
    return true;
}

QString Phone::dbusPath() const
{
    return PHONEKOMPANION_PHONES_PATH + QLatin1Char('/') + m_id;
}

#include "moc_phone.cpp"
