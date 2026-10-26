/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "tray.h"

#include <QCoreApplication>
#include <QMenu>
#include <QProcess>
#include <QStandardPaths>

#include <KLocalizedString>
#include <KStatusNotifierItem>

#include "phone.h"
#include "phonekompaniond_debug.h"
#include "phonemanager.h"

Tray::Tray(PhoneManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_item(new KStatusNotifierItem(this))
{
    m_item->setCategory(KStatusNotifierItem::Communications);
    m_item->setIconByName(QStringLiteral("smartphone"));
    m_item->setTitle(i18n("Phone Kompanion"));

    QAction *open = m_item->contextMenu()->addAction(QIcon::fromTheme(QStringLiteral("window")), i18n("Open Phone Kompanion"));
    connect(open, &QAction::triggered, this, &Tray::openWindow);
    connect(m_item, &KStatusNotifierItem::activateRequested, this, &Tray::openWindow);

    connect(m_manager, &PhoneManager::phonesChanged, this, &Tray::update);
    update();
}

void Tray::update()
{
    QStringList lines;
    bool connected = false;
    const QList<Phone *> phones = m_manager->allPhones();
    for (const Phone *phone : phones) {
        lines << (phone->isReachable() ? i18nc("@info:tooltip phone name", "%1: connected", phone->name())
                                       : i18nc("@info:tooltip phone name", "%1: not connected", phone->name()));
        connected |= phone->isReachable();
    }
    if (!m_manager->isKdeConnectRunning()) {
        lines = {i18n("KDE Connect is not running")};
    } else if (lines.isEmpty()) {
        lines << i18n("No phones paired in KDE Connect");
    }

    m_item->setToolTip(QStringLiteral("smartphone"), i18n("Phone Kompanion"), lines.join(QLatin1Char('\n')));
    // Passive moves the icon into the tray's hidden items
    m_item->setStatus(connected ? KStatusNotifierItem::Active : KStatusNotifierItem::Passive);
}

void Tray::openWindow()
{
    // Next to this program first, so a build that is not installed opens its own window
    QString app = QStandardPaths::findExecutable(QStringLiteral("phonekompanion"), {QCoreApplication::applicationDirPath()});
    if (app.isEmpty()) {
        app = QStandardPaths::findExecutable(QStringLiteral("phonekompanion"));
    }

    QProcess process;
    process.setProgram(app);
    // Lets the window take focus on Wayland; there is no token when opened from the menu
    if (const QString token = m_item->providedToken(); !token.isEmpty()) {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), token);
        process.setProcessEnvironment(environment);
    }
    if (app.isEmpty() || !process.startDetached()) {
        qCWarning(PHONEKOMPANIOND) << "Could not start phonekompanion" << app;
    }
}

#include "moc_tray.cpp"
