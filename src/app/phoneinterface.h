/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "phonefeature.h"

/**
 * A phone, /phones/<id> of phonekompaniond; its status follows the daemon's phonesChanged
 */
class PhoneInterface : public PhoneFeature
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString name READ name NOTIFY statusChanged)
    // Reachable through KDE Connect
    Q_PROPERTY(bool reachable READ isReachable NOTIFY statusChanged)
    Q_PROPERTY(bool callsConnected READ callsConnected NOTIFY statusChanged)
    // KDE Connect's SMS plugin is loaded for the phone
    Q_PROPERTY(bool messagesAvailable READ messagesAvailable NOTIFY statusChanged)

public:
    explicit PhoneInterface(QObject *parent = nullptr);

    QString name() const;
    bool isReachable() const;
    bool callsConnected() const;
    bool messagesAvailable() const;
};
