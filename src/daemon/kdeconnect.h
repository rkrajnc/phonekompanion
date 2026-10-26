/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QVariantMap>

#include <optional>

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

Q_SIGNALS:
    /**
     * The daemon started or stopped, or a device was added, removed, paired or changed
     */
    void devicesChanged();

private:
    bool m_running = false;
};
