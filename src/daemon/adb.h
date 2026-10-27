/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>

class QProcess;

/**
 * adb and scrcpy on this computer, and the adb devices connected to it; shared by all phones
 */
class Adb : public QObject
{
    Q_OBJECT

public:
    explicit Adb(QObject *parent = nullptr);
    ~Adb() override;

    /**
     * Looks for the tools again, as they may have been installed in the meantime
     */
    void findTools();

    QString adb() const;
    QString scrcpy() const;
    QStringList missingTools() const;
    /**
     * Serials of the connected devices
     */
    QStringList devices() const;
    /**
     * The IP address of a device, from its serial or its mDNS service
     */
    QString deviceAddress(const QString &serial) const;

Q_SIGNALS:
    void toolsChanged();
    /**
     * The devices or their mDNS addresses changed
     */
    void devicesChanged();

private:
    void trackDevices();
    void stopTracking();
    void readTrackedDevices();
    void refreshMdnsServices();

    QString m_adb;
    QString m_scrcpy;
    QStringList m_missingTools;
    // adb track-devices, reports every change of the connected devices
    QProcess *m_tracker = nullptr;
    QByteArray m_trackerOutput;
    QStringList m_devices;
    // mDNS service name -> IP address, for the adb-tls-connect services
    QHash<QString, QString> m_mdnsAddresses;
};
