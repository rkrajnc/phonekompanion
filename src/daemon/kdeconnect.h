/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QVariantMap>

#include <optional>

class QDBusMessage;

/**
 * The KDE Connect daemon, over D-Bus
 */
class KdeConnect : public QObject
{
    Q_OBJECT

public:
    explicit KdeConnect(QObject *parent = nullptr);

    bool isRunning() const;
    /**
     * Empty when KDE Connect is not running, nullopt when it did not answer
     */
    std::optional<QStringList> pairedDevices() const;
    /**
     * The org.kde.kdeconnect.device properties (name, type, isReachable, ...), empty when KDE Connect did not answer
     */
    QVariantMap deviceProperties(const QString &id) const;
    /**
     * The plugins loaded for a device, which happens only while it is reachable; nullopt when
     * KDE Connect did not answer
     */
    std::optional<QStringList> loadedPlugins(const QString &id) const;

Q_SIGNALS:
    /**
     * The daemon started or stopped, or a device was added, removed, paired or changed
     */
    void devicesChanged();
    /**
     * The telephony plugin's callReceived: event is "callReceived" while ringing or "missedCall"
     */
    void callReceived(const QString &deviceId, const QString &event, const QString &number, const QString &contactName);

private Q_SLOTS:
    void telephonyCallReceived(const QDBusMessage &message);

private:
    bool m_running = false;
};
