/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>

class KStatusNotifierItem;
class PhoneManager;

/**
 * The system tray icon: opens the window, shows which phones are connected
 */
class Tray : public QObject
{
    Q_OBJECT

public:
    explicit Tray(PhoneManager *manager, QObject *parent = nullptr);

private:
    void update();
    void openWindow();

    PhoneManager *m_manager;
    KStatusNotifierItem *m_item;
};
