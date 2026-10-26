/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>

/**
 * The phones known to phonekompaniond, over D-Bus
 */
class PhonesModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool serviceRunning READ isServiceRunning NOTIFY serviceRunningChanged)
    Q_PROPERTY(bool kdeConnectRunning READ isKdeConnectRunning NOTIFY kdeConnectRunningChanged)

public:
    enum Roles {
        PhoneIdRole = Qt::UserRole + 1,
        NameRole,
        ReachableRole,
    };

    explicit PhonesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isServiceRunning() const;
    bool isKdeConnectRunning() const;

    Q_INVOKABLE void startService();
    Q_INVOKABLE void openKdeConnect();

Q_SIGNALS:
    void serviceRunningChanged();
    void kdeConnectRunningChanged();

private Q_SLOTS:
    void refresh();

private:
    struct Phone {
        QString id;
        QString name;
        bool reachable = false;
    };

    void setPhones(QList<Phone> phones, bool kdeConnectRunning);

    QList<Phone> m_phones;
    bool m_serviceRunning = false;
    bool m_kdeConnectRunning = false;
    // Replies to an older refresh() are dropped
    quint64 m_generation = 0;
};
