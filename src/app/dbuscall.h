/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>

#include "dbusnames.h"

/**
 * Calls the daemon without starting it: the window starts it on its own, and calls made right
 * after the user quit it must not start it again
 */
inline QDBusPendingCall callDaemon(const QString &path, const QString &interface, const QString &method, const QVariantList &arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(PHONEKOMPANION_SERVICE, path, interface, method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    return QDBusConnection::sessionBus().asyncCall(message);
}

inline QDBusPendingCall getAllProperties(const QString &path, const QString &interface)
{
    return callDaemon(path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"), {interface});
}
