/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QApplication>

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>

#include "phonekompanion-version.h"
#include "phonemanager.h"
#include "runner.h"
#include "tray.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    KLocalizedString::setApplicationDomain("phonekompanion");

    KAboutData aboutData(QStringLiteral("phonekompaniond"),
                         i18n("Phone Kompanion"),
                         QStringLiteral(PHONEKOMPANION_VERSION_STRING),
                         i18n("Background service of Phone Kompanion"),
                         KAboutLicense::GPL,
                         i18n("© 2026 Rok Krajnc"));
    aboutData.setOrganizationDomain("rkrajnc.github.io");
    aboutData.setDesktopFileName(QStringLiteral("io.github.rkrajnc.phonekompaniond"));
    KAboutData::setApplicationData(aboutData);

    // io.github.rkrajnc.phonekompaniond
    KDBusService service(KDBusService::Unique);

    PhoneManager manager;
    Tray tray(&manager);
    Runner runner(&manager);

    return app.exec();
}
