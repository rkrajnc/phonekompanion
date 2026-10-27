/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "phoneinterface.h"

#include "dbusnames.h"

PhoneInterface::PhoneInterface(QObject *parent)
    : PhoneFeature(QString(), PHONEKOMPANION_PHONE_INTERFACE, parent, false)
{
}

QString PhoneInterface::name() const
{
    return status(QStringLiteral("name")).toString();
}

bool PhoneInterface::isReachable() const
{
    return status(QStringLiteral("reachable")).toBool();
}

bool PhoneInterface::messagesAvailable() const
{
    return status(QStringLiteral("messagesAvailable")).toBool();
}

bool PhoneInterface::callsConnected() const
{
    return status(QStringLiteral("callsConnected")).toBool();
}

#include "moc_phoneinterface.cpp"
