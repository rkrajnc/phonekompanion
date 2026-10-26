/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QVariantMap>

/**
 * A phone or tablet paired in KDE Connect, at /phones/<KDE Connect device id>
 */
class Phone : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.rkrajnc.phonekompanion.phone")
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(bool reachable READ isReachable)

public:
    Phone(const QString &id, QObject *parent = nullptr);
    ~Phone() override;

    QString id() const;
    QString name() const;
    bool isReachable() const;

    /**
     * Takes the KDE Connect device properties, returns whether anything changed
     */
    bool update(const QVariantMap &properties);

private:
    QString dbusPath() const;

    const QString m_id;
    QString m_name;
    bool m_reachable = false;
};
