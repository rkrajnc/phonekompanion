/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <KPeople/PersonsModel>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QHash>
#include <QPointer>
#include <QVariantMap>

#include <functional>

class QDBusMessage;
class QDBusPendingCallWatcher;

/**
 * What all phones' calls share: the paired Bluetooth phones (BlueZ), the HFP audio gateways
 * (PipeWire's telephony API), the screen lock and the contacts
 */
class Bluetooth : public QObject
{
    Q_OBJECT

public:
    using InterfaceMap = QMap<QString, QVariantMap>;
    using ObjectMap = QMap<QDBusObjectPath, InterfaceMap>;

    explicit Bluetooth(QObject *parent = nullptr);

    /**
     * Whether PipeWire's telephony API is there
     */
    bool telephonyAvailable() const;
    bool pairedPhonesLoaded() const;
    /**
     * Bluetooth address -> name, for paired devices offering the HFP audio gateway role
     */
    QHash<QString, QString> pairedPhones() const;
    /**
     * The BlueZ object path of a paired phone
     */
    QString phoneObject(const QString &address) const;
    /**
     * PipeWire audio gateway object path -> Bluetooth address
     */
    QHash<QString, QString> gateways() const;
    /**
     * How many phones there are; with only one, it takes the only gateway also under another name
     */
    int phoneCount() const;
    void setPhoneCount(int count);
    bool isLocked() const;
    const KPeople::PersonsModel &persons() const;

    /**
     * Calls handler with the objects below path, unless the call fails or context is gone by then
     */
    static QDBusPendingCallWatcher *getManagedObjects(const QDBusConnection &bus,
                                                      const QString &service,
                                                      const QString &path,
                                                      QObject *context,
                                                      const std::function<void(const ObjectMap &)> &handler);

Q_SIGNALS:
    /**
     * The paired phones, the gateways, telephonyAvailable or phoneCount changed
     */
    void changed();
    void lockedChanged();

private Q_SLOTS:
    void interfacesAdded(const QDBusMessage &message);
    void interfacesRemoved(const QDBusMessage &message);
    void screenLockChanged(bool locked);

private:
    void refreshPairedPhones();
    void refreshGateways();
    void clearGateways();
    /**
     * Returns whether the gateways changed
     */
    bool addGateway(const QString &path, const InterfaceMap &interfaces);

    QHash<QString, QString> m_pairedPhones;
    // Bluetooth address -> BlueZ object path, for paired phones
    QHash<QString, QString> m_phoneObjects;
    bool m_pairedPhonesLoaded = false;
    QPointer<QDBusPendingCallWatcher> m_pairedPhonesRequest;
    // A change came while BlueZ was asked, so its answer may be old
    bool m_pairedPhonesStale = false;
    QHash<QString, QString> m_gateways;
    bool m_telephonyAvailable = false;
    int m_phoneCount = 0;
    bool m_locked = false;
    KPeople::PersonsModel m_persons;
};
