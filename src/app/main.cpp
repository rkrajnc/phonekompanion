/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <QWindow>

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KWindowSystem>

#include "phonekompanion-version.h"

Q_IMPORT_QML_PLUGIN(PhoneKompanionSmsPlugin)

static void raiseWindow(QWindow *window)
{
    KWindowSystem::updateStartupId(window);
    KWindowSystem::activateWindow(window);
}

// What follows "<scheme>:" in a link, without "//" as in the common "tel://<number>", where QUrl
// would take the number for a host; empty for other links
static QString linkPath(const QString &link, QLatin1String scheme)
{
    if (!link.startsWith(scheme + QLatin1Char(':'), Qt::CaseInsensitive)) {
        return QString();
    }
    QString path = link.mid(scheme.size() + 1);
    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    return path;
}

// The number of a tel: link, without parameters such as ";ext=" or ";phone-context="
static QString telNumber(const QString &link)
{
    return QUrl::fromPercentEncoding(linkPath(link, QLatin1String("tel")).toUtf8()).section(QLatin1Char(';'), 0, 0).trimmed();
}

struct SmsLink {
    bool valid = false;
    QString number;
    QString text;
};

// The first recipient and the text of an sms: link, "sms:<number>[,<number>...][?body=<text>]"
// (RFC 5724), also "sms:?body=<text>" without a recipient and iOS's "sms:<number>&body=<text>"
static SmsLink smsLink(const QString &link)
{
    if (!link.startsWith(QLatin1String("sms:"), Qt::CaseInsensitive)) {
        return {};
    }
    const QString path = linkPath(link, QLatin1String("sms"));
    const qsizetype separator = path.indexOf(QRegularExpression(QStringLiteral("[?&]")));
    const QString recipients = separator < 0 ? path : path.left(separator);
    const QUrlQuery query(separator < 0 ? QString() : path.mid(separator + 1));
    // Without parameters such as ";phone-context=", as in tel: links
    const QString number = QUrl::fromPercentEncoding(recipients.section(QLatin1Char(','), 0, 0).section(QLatin1Char(';'), 0, 0).toUtf8()).trimmed();
    return {true, number, query.queryItemValue(QStringLiteral("body"), QUrl::FullyDecoded)};
}

// --phone, --apps, --messages, --dialer, and tel: and sms: links, from this start or from a second start that activates this window
static void openPhone(QCommandLineParser &parser, const QStringList &arguments, QObject *window)
{
    if (!parser.parse(arguments)) {
        qWarning() << parser.errorText();
        return;
    }
    const QString link = parser.positionalArguments().value(0);
    const SmsLink sms = smsLink(link);
    const QString number = sms.valid ? sms.number : telNumber(link);
    const bool messages = parser.isSet(QStringLiteral("messages")) || sms.valid;
    const bool dialer = !messages && (parser.isSet(QStringLiteral("dialer")) || !number.isEmpty());
    if (!parser.isSet(QStringLiteral("phone")) && !dialer && !messages && !parser.isSet(QStringLiteral("apps"))) {
        return;
    }
    // Without --phone, the window picks the phone once it has read the phones
    const QString phone = parser.value(QStringLiteral("phone"));
    QString subpage;
    if (dialer) {
        subpage = QStringLiteral("dialer");
    } else if (messages) {
        subpage = QStringLiteral("messages");
    } else if (parser.isSet(QStringLiteral("apps"))) {
        subpage = QStringLiteral("apps");
    }
    QMetaObject::invokeMethod(window,
                              "openPhoneById",
                              Qt::QueuedConnection,
                              Q_ARG(QString, phone),
                              Q_ARG(QString, subpage),
                              Q_ARG(QString, number),
                              Q_ARG(QString, sms.text));
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
    parser.addOption(QCommandLineOption(QStringLiteral("apps"), i18n("Open the apps, of --phone or else of the phone KDE Connect reaches")));
    parser.addOption(
        QCommandLineOption(QStringLiteral("messages"), i18n("Open the SMS conversations, of --phone or else of the phone whose messages are available")));
    parser.addOption(QCommandLineOption(QStringLiteral("dialer"), i18n("Open the phone dialer, of --phone or else of the phone connected for calls")));
    parser.addPositionalArgument(QStringLiteral("link"),
                                 i18n("A tel: link, to open the dialer with its number, or an sms: link, to open the conversation with its number and text"),
                                 QStringLiteral("[tel:number|sms:number?body=text]"));
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
