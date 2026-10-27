/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QList>
#include <QObject>
#include <QVariantMap>

class PhoneManager;

/**
 * KRunner D-Bus runner at /runner: "phone <app>", "phone screen", "phone camera"
 */
class Runner : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.krunner1")

public:
    struct RemoteMatch {
        QString id;
        QString text;
        QString iconName;
        int categoryRelevance = 0;
        double relevance = 0;
        QVariantMap properties;
    };
    using RemoteMatches = QList<RemoteMatch>;

    struct RemoteAction {
        QString id;
        QString text;
        QString iconName;
    };
    using RemoteActions = QList<RemoteAction>;

    explicit Runner(PhoneManager *manager, QObject *parent = nullptr);
    ~Runner() override;

public Q_SLOTS:
    RemoteActions Actions();
    RemoteMatches Match(const QString &query);
    void Run(const QString &matchId, const QString &actionId);

private:
    PhoneManager *m_manager;
};

Q_DECLARE_METATYPE(Runner::RemoteMatch)
Q_DECLARE_METATYPE(Runner::RemoteMatches)
Q_DECLARE_METATYPE(Runner::RemoteAction)
Q_DECLARE_METATYPE(Runner::RemoteActions)
