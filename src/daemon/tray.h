/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QList>
#include <QObject>

class KStatusNotifierItem;
class Phone;
class PhoneManager;
class QAction;

/**
 * The system tray icon: opens the window, shows which phones are connected, and has
 * the mirroring actions of the connected phones in its menu
 */
class Tray : public QObject
{
    Q_OBJECT

public:
    explicit Tray(PhoneManager *manager, QObject *parent = nullptr);

private:
    void update();
    void updateMenu();
    void openWindow(const QStringList &arguments = {});

    PhoneManager *m_manager;
    KStatusNotifierItem *m_item;
    // The phones' actions in the menu, rebuilt when the phones or their status change
    QList<QAction *> m_phoneActions;
    QAction *m_phonesEnd;
};
