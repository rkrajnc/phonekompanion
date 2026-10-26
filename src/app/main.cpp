/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QWindow>

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KWindowSystem>

#include "phonekompanion-version.h"

static void raiseWindow(QWindow *window)
{
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
}

int main(int argc, char *argv[])
{
    QIcon::setFallbackThemeName(QStringLiteral("breeze"));
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("phonekompanion");

    KAboutData aboutData(QStringLiteral("phonekompanion"),
                         i18n("Phone Kompanion"),
                         QStringLiteral(PHONEKOMPANION_VERSION_STRING),
                         i18n("Calls and phone apps on your computer, next to KDE Connect"),
                         KAboutLicense::GPL,
                         i18n("© 2026 Rok Krajnc"));
    aboutData.setOrganizationDomain("rkrajnc.github.io");
    aboutData.setDesktopFileName(QStringLiteral("io.github.rkrajnc.phonekompanion"));
    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("smartphone")));

    // io.github.rkrajnc.phonekompanion
    KDBusService service(KDBusService::Unique);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.loadFromModule("io.github.rkrajnc.phonekompanion", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    if (!window) {
        return 1;
    }
    QObject::connect(&service, &KDBusService::activateRequested, window, [window]() {
        raiseWindow(window);
    });

    return app.exec();
}
