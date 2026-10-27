/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <QTimer>

#include <KAboutData>
#include <KLocalizedString>

#include "dbusnames.h"
#include "phonekompanion-version.h"

#include <algorithm>
#include <iterator>
#include <optional>

namespace
{
struct PhoneInfo {
    QString id;
    QString name;
    bool reachable = false;
};

QString phonePath(const QString &id)
{
    return PHONEKOMPANION_PHONES_PATH + QLatin1Char('/') + id;
}

// --notify, for launchers, which have no terminal to show the error in
bool s_notify = false;

/**
 * Prints an error, and with --notify also shows it as a notification
 */
void fail(const QString &title, const QString &error)
{
    QTextStream(stderr) << i18n("error: ") << error << Qt::endl;
    if (!s_notify) {
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
                                                          QStringLiteral("/org/freedesktop/Notifications"),
                                                          QStringLiteral("org.freedesktop.Notifications"),
                                                          QStringLiteral("Notify"));
    message.setArguments({i18n("Phone Kompanion"), 0u, QStringLiteral("smartphone"), title, error, QStringList(), QVariantMap(), -1});
    QDBusConnection::sessionBus().call(message);
}

/**
 * Waits for the daemon's appsChanged, with its error
 */
class AppsWaiter : public QObject
{
    Q_OBJECT

public:
    QString error;

public Q_SLOTS:
    void appsChanged(const QString &error)
    {
        this->error = error;
        Q_EMIT done();
    }

Q_SIGNALS:
    void done();
};

std::optional<QList<PhoneInfo>> phones()
{
    // Starts the daemon if it is installed and not running
    QDBusInterface manager(PHONEKOMPANION_SERVICE, PHONEKOMPANION_PHONES_PATH, PHONEKOMPANION_PHONES_INTERFACE);
    if (!manager.isValid()) {
        fail(i18n("Phone Kompanion"), i18n("The Phone Kompanion service is not running."));
        return std::nullopt;
    }
    QList<PhoneInfo> phones;
    const QStringList ids = manager.property("phones").toStringList();
    for (const QString &id : ids) {
        QDBusInterface phone(PHONEKOMPANION_SERVICE, phonePath(id), PHONEKOMPANION_PHONE_INTERFACE);
        phones.append({id, phone.property("name").toString(), phone.property("reachable").toBool()});
    }
    return phones;
}

/**
 * The phone given by id or name, else the only phone, or the only connected one
 */
std::optional<PhoneInfo> choosePhone(const QList<PhoneInfo> &phones, const QString &wanted)
{
    if (!wanted.isEmpty()) {
        for (const PhoneInfo &phone : phones) {
            if (phone.id == wanted || phone.name.compare(wanted, Qt::CaseInsensitive) == 0) {
                return phone;
            }
        }
        fail(i18n("Phone Kompanion"), i18n("There is no phone \"%1\". See --list-phones.", wanted));
        return std::nullopt;
    }
    if (phones.size() == 1) {
        return phones.constFirst();
    }
    QList<PhoneInfo> connected;
    std::copy_if(phones.cbegin(), phones.cend(), std::back_inserter(connected), [](const PhoneInfo &phone) {
        return phone.reachable;
    });
    if (connected.size() == 1) {
        return connected.constFirst();
    }
    fail(i18n("Phone Kompanion"), phones.isEmpty() ? i18n("No phones are paired in KDE Connect.") : i18n("Choose a phone with --phone. See --list-phones."));
    return std::nullopt;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("phonekompanion");

    KAboutData aboutData(QStringLiteral("phonekompanion-cli"),
                         i18n("Phone Kompanion"),
                         QStringLiteral(PHONEKOMPANION_VERSION_STRING),
                         i18n("Command line for Phone Kompanion"),
                         KAboutLicense::GPL,
                         i18n("© 2026 Rok Krajnc"));
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    const QCommandLineOption listPhones({QStringLiteral("l"), QStringLiteral("list-phones")}, i18n("List the phones and their ids"));
    const QCommandLineOption phoneOption({QStringLiteral("p"), QStringLiteral("phone")},
                                         i18n("The phone to use, by id or name; needed when more than one phone is connected"),
                                         i18n("phone"));
    const QCommandLineOption mirror(QStringLiteral("mirror"), i18n("Show the phone's screen in a window"));
    const QCommandLineOption listApps(QStringLiteral("list-apps"), i18n("List the phone's apps and their package names"));
    const QCommandLineOption openApp(QStringLiteral("open-app"), i18n("Open an app of the phone in a window"), i18n("package"));
    const QCommandLineOption camera(QStringLiteral("camera"), i18n("Show a camera of the phone in a window"));
    const QCommandLineOption webcam(QStringLiteral("webcam"), i18n("Use a camera of the phone as a webcam"));
    const QCommandLineOption stopWebcam(QStringLiteral("stop-webcam"), i18n("Stop using the phone's camera as a webcam"));
    const QCommandLineOption cameraFacing(QStringLiteral("camera-facing"),
                                          i18n("Use this camera for --camera or --webcam, instead of the one in the settings"),
                                          QStringLiteral("back|front"));
    const QCommandLineOption notifyOption(QStringLiteral("notify"), i18n("Also show errors as a desktop notification"));
    parser.addOptions({listPhones, phoneOption, mirror, listApps, openApp, camera, webcam, stopWebcam, cameraFacing, notifyOption});
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);
    s_notify = parser.isSet(notifyOption);

    QTextStream out(stdout);
    const std::optional<QList<PhoneInfo>> allPhones = phones();
    if (!allPhones) {
        return 1;
    }

    if (parser.isSet(listPhones)) {
        for (const PhoneInfo &phone : *allPhones) {
            out << phone.name << ": " << phone.id << " (" << (phone.reachable ? i18n("connected") : i18n("not connected")) << ")" << Qt::endl;
        }
        return 0;
    }

    const bool action =
        parser.isSet(mirror) || parser.isSet(listApps) || parser.isSet(openApp) || parser.isSet(camera) || parser.isSet(webcam) || parser.isSet(stopWebcam);
    if (!action) {
        parser.showHelp(1);
    }

    const std::optional<PhoneInfo> phone = choosePhone(*allPhones, parser.value(phoneOption));
    if (!phone) {
        return 1;
    }
    QDBusInterface scrcpy(PHONEKOMPANION_SERVICE, phonePath(phone->id) + QStringLiteral("/scrcpy"), PHONEKOMPANION_SCRCPY_INTERFACE);

    if (parser.isSet(listApps)) {
        // Read the apps again; longer than the daemon's 30 s limit
        AppsWaiter waiter;
        QEventLoop loop;
        QObject::connect(&waiter, &AppsWaiter::done, &loop, &QEventLoop::quit);
        scrcpy.connection().connect(scrcpy.service(), scrcpy.path(), scrcpy.interface(), QStringLiteral("appsChanged"), &waiter, SLOT(appsChanged(QString)));
        QTimer::singleShot(35000, &loop, [&loop, &waiter] {
            waiter.error = i18n("Reading the apps took too long.");
            loop.quit();
        });
        const QDBusReply<QString> reason = scrcpy.call(QStringLiteral("refreshApps"));
        if (!reason.isValid() || !reason.value().isEmpty()) {
            fail(phone->name, reason.isValid() ? reason.value() : reason.error().message());
            return 1;
        }
        loop.exec();
        if (!waiter.error.isEmpty()) {
            fail(phone->name, waiter.error);
            return 1;
        }
        const QDBusReply<QString> apps = scrcpy.call(QStringLiteral("apps"));
        const QJsonArray array = QJsonDocument::fromJson(apps.value().toUtf8()).array();
        for (const QJsonValue &app : array) {
            out << app[QStringLiteral("package")].toString() << ": " << app[QStringLiteral("name")].toString() << Qt::endl;
        }
        return 0;
    }

    QDBusMessage reply;
    if (parser.isSet(mirror)) {
        reply = scrcpy.call(QStringLiteral("mirrorScreen"));
    } else if (parser.isSet(openApp)) {
        reply = scrcpy.call(QStringLiteral("openApp"), parser.value(openApp));
    } else if (parser.isSet(webcam)) {
        reply = scrcpy.call(QStringLiteral("startWebcam"), parser.value(cameraFacing));
    } else if (parser.isSet(stopWebcam)) {
        reply = scrcpy.call(QStringLiteral("stopWebcam"));
    } else {
        reply = scrcpy.call(QStringLiteral("showCamera"), parser.value(cameraFacing));
    }
    const QString error = reply.type() == QDBusMessage::ErrorMessage ? reply.errorMessage() : reply.arguments().value(0).toString();
    if (!error.isEmpty()) {
        fail(phone->name, error);
        return 1;
    }
    return 0;
}

#include "main.moc"
