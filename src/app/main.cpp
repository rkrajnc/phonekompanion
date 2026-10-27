/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QUrl>
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

// The number of a tel: link, without parameters such as ";ext=" or ";phone-context="; empty for other links.
// Also for the common "tel://<number>", where QUrl would take the number for a host.
static QString telNumber(const QString &link)
{
    if (!link.startsWith(QLatin1String("tel:"), Qt::CaseInsensitive)) {
        return QString();
    }
    QString number = QUrl::fromPercentEncoding(link.mid(4).toUtf8());
    while (number.startsWith(QLatin1Char('/'))) {
        number.remove(0, 1);
    }
    return number.section(QLatin1Char(';'), 0, 0).trimmed();
}

// --phone, --apps, --dialer and tel: links, from this start or from a second start that activates this window
static void openPhone(QCommandLineParser &parser, const QStringList &arguments, QObject *window)
{
    if (!parser.parse(arguments)) {
        qWarning() << parser.errorText();
        return;
    }
    const QString number = telNumber(parser.positionalArguments().value(0));
    const bool dialer = parser.isSet(QStringLiteral("dialer")) || !number.isEmpty();
    if (!parser.isSet(QStringLiteral("phone")) && !dialer) {
        return;
    }
    // Without --phone, the window picks the phone connected for calls once it has read the phones
    const QString phone = parser.value(QStringLiteral("phone"));
    const QString subpage = dialer ? QStringLiteral("dialer") : parser.isSet(QStringLiteral("apps")) ? QStringLiteral("apps") : QString();
    QMetaObject::invokeMethod(window, "openPhoneById", Qt::QueuedConnection, Q_ARG(QString, phone), Q_ARG(QString, subpage), Q_ARG(QString, number));
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

    QCommandLineParser parser;
    parser.addOption(QCommandLineOption(QStringLiteral("phone"), i18n("Open the page of this phone, by its KDE Connect id"), i18n("id")));
    parser.addOption(QCommandLineOption(QStringLiteral("apps"), i18n("Open the phone's apps, with --phone")));
    parser.addOption(QCommandLineOption(QStringLiteral("dialer"), i18n("Open the phone dialer, of --phone or else of the phone connected for calls")));
    parser.addPositionalArgument(QStringLiteral("link"), i18n("A tel: link, to open the dialer with its number"), QStringLiteral("[tel:number]"));
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

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
    openPhone(parser, QCoreApplication::arguments(), window);
    QObject::connect(&service, &KDBusService::activateRequested, window, [window, &parser](const QStringList &arguments) {
        openPhone(parser, arguments, window);
        raiseWindow(window);
    });

    return app.exec();
}
