/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "runner.h"

#include <KLocalizedString>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QJsonArray>
#include <QJsonObject>

#include "calls.h"
#include "phone.h"
#include "phonekompaniond_debug.h"
#include "phonemanager.h"
#include "scrcpy.h"

#include <algorithm>

#define RUNNER_PATH QStringLiteral("/runner")

using RemoteMatch = Runner::RemoteMatch;
using RemoteMatches = Runner::RemoteMatches;
using RemoteAction = Runner::RemoteAction;
using RemoteActions = Runner::RemoteActions;

QDBusArgument &operator<<(QDBusArgument &argument, const RemoteMatch &match)
{
    argument.beginStructure();
    argument << match.id << match.text << match.iconName << match.categoryRelevance << match.relevance << match.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteMatch &match)
{
    argument.beginStructure();
    argument >> match.id >> match.text >> match.iconName >> match.categoryRelevance >> match.relevance >> match.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const RemoteAction &action)
{
    argument.beginStructure();
    argument << action.id << action.text << action.iconName;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, RemoteAction &action)
{
    argument.beginStructure();
    argument >> action.id >> action.text >> action.iconName;
    argument.endStructure();
    return argument;
}

Runner::Runner(PhoneManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    qDBusRegisterMetaType<RemoteMatch>();
    qDBusRegisterMetaType<RemoteMatches>();
    qDBusRegisterMetaType<RemoteAction>();
    qDBusRegisterMetaType<RemoteActions>();
    if (!QDBusConnection::sessionBus().registerObject(RUNNER_PATH, this, QDBusConnection::ExportAllSlots)) {
        qCWarning(PHONEKOMPANIOND) << "Could not register the KRunner runner at" << RUNNER_PATH;
    }
}

Runner::~Runner()
{
    QDBusConnection::sessionBus().unregisterObject(RUNNER_PATH);
}

RemoteActions Runner::Actions()
{
    return {};
}

namespace
{
// Match ids are "<phone id>\n<kind>\n<argument>", with kind screen, camera, app (argument the
// package) or call (argument the number); with several phones, the phone's name is in the subtext
RemoteMatch makeMatch(const Phone *phone, int phoneCount, const QString &kind, const QString &argument, const QString &text, double relevance, int category)
{
    RemoteMatch match;
    match.id = phone->id() + QLatin1Char('\n') + kind + QLatin1Char('\n') + argument;
    match.text = text;
    match.categoryRelevance = category;
    match.relevance = relevance;
    if (kind == QLatin1String("call")) {
        match.iconName = QStringLiteral("call-start");
        match.properties.insert(QStringLiteral("subtext"), phoneCount > 1 ? i18n("%1 via %2", argument, phone->name()) : argument);
    } else {
        match.iconName = kind == QLatin1String("camera") ? QStringLiteral("camera-photo") : QStringLiteral("smartphone");
        if (phoneCount > 1) {
            match.properties.insert(QStringLiteral("subtext"), phone->name());
        }
    }
    return match;
}
}

RemoteMatches Runner::Match(const QString &query)
{
    // The keywords are English only, as KRunner filters queries with the regex in the runner's .desktop file
    const QString callKeyword = QStringLiteral("call ");
    if (query.startsWith(callKeyword, Qt::CaseInsensitive)) {
        return matchCall(query.mid(callKeyword.size()).trimmed());
    }
    const QString keyword = QStringLiteral("phone ");
    if (!query.startsWith(keyword, Qt::CaseInsensitive)) {
        return {};
    }
    const QString rest = query.mid(keyword.size()).trimmed();
    if (rest.isEmpty()) {
        return {};
    }

    QList<Phone *> phones = m_manager->allPhones();
    phones.removeIf([](const Phone *phone) {
        return !phone->isReachable();
    });

    RemoteMatches matches;
    const auto addMatch = [&phones, &matches](Phone *phone, const QString &kind, const QString &package, const QString &text, double relevance, int category) {
        matches.append(makeMatch(phone, phones.size(), kind, package, text, relevance, category));
    };

    for (Phone *phone : std::as_const(phones)) {
        if (rest.compare(QLatin1String("screen"), Qt::CaseInsensitive) == 0) {
            addMatch(phone, QStringLiteral("screen"), QString(), i18nc("@action", "Mirror phone screen"), 1, 100);
        } else if (rest.compare(QLatin1String("camera"), Qt::CaseInsensitive) == 0) {
            addMatch(phone, QStringLiteral("camera"), QString(), i18nc("@action", "Show phone camera"), 1, 100);
        }

        // Exact names first, then names starting with the text, then the rest; KRunner's categories Highest, High and Moderate
        QList<std::pair<double, QJsonObject>> found;
        const QJsonArray apps = phone->scrcpy()->appList();
        for (const QJsonValue &app : apps) {
            const QString name = app[QStringLiteral("name")].toString();
            if (name.compare(rest, Qt::CaseInsensitive) == 0) {
                found.append({0.9, app.toObject()});
            } else if (name.startsWith(rest, Qt::CaseInsensitive)) {
                found.append({0.8, app.toObject()});
            } else if (name.contains(rest, Qt::CaseInsensitive)) {
                found.append({0.6, app.toObject()});
            }
        }
        std::stable_sort(found.begin(), found.end(), [](const std::pair<double, QJsonObject> &a, const std::pair<double, QJsonObject> &b) {
            return a.first > b.first;
        });
        for (const auto &[relevance, app] : found.mid(0, 10)) {
            const QString name = app[QStringLiteral("name")].toString();
            const int category = relevance >= 0.9 ? 100 : relevance >= 0.8 ? 70 : 50;
            addMatch(phone,
                     QStringLiteral("app"),
                     app[QStringLiteral("package")].toString(),
                     i18nc("@action %1 is a phone app", "Open %1", name),
                     relevance,
                     category);
        }
    }
    return matches;
}

RemoteMatches Runner::matchCall(const QString &numberOrName) const
{
    if (numberOrName.isEmpty()) {
        return {};
    }
    QList<Phone *> phones = m_manager->allPhones();
    phones.removeIf([](const Phone *phone) {
        return !phone->callsConnected();
    });

    RemoteMatches matches;
    const auto addMatch = [&matches, &phones](const Phone *phone, const QString &text, const QString &number) {
        matches.append(makeMatch(phone, phones.size(), QStringLiteral("call"), number, text, 1, 100));
    };

    const bool isNumber = Calls::isPhoneNumber(numberOrName);
    for (const Phone *phone : std::as_const(phones)) {
        if (isNumber) {
            addMatch(phone, i18n("Call %1", numberOrName), numberOrName);
            continue;
        }
        const QList<Calls::Contact> contacts = phone->calls()->findContacts(numberOrName, 10);
        for (const Calls::Contact &contact : contacts) {
            for (const QString &number : contact.numbers) {
                addMatch(phone, i18n("Call %1", contact.name), number);
            }
        }
    }
    return matches;
}

void Runner::Run(const QString &matchId, const QString &actionId)
{
    Q_UNUSED(actionId);
    const QString phoneId = matchId.section(QLatin1Char('\n'), 0, 0);
    const QString kind = matchId.section(QLatin1Char('\n'), 1, 1);
    // The package of an app, the number of a call
    const QString argument = matchId.section(QLatin1Char('\n'), 2);
    const QList<Phone *> phones = m_manager->allPhones();
    for (Phone *phone : phones) {
        if (phone->id() != phoneId) {
            continue;
        }
        Scrcpy *scrcpy = phone->scrcpy();
        if (kind == QLatin1String("call")) {
            // A typed number or a contact's, so not taken for a name
            phone->notifyError(phone->calls()->dialStoredNumber(argument));
        } else if (kind == QLatin1String("screen")) {
            phone->notifyError(scrcpy->mirrorScreen());
        } else if (kind == QLatin1String("camera")) {
            phone->notifyError(scrcpy->showCamera(QString()));
        } else {
            phone->notifyError(scrcpy->openApp(argument));
        }
        return;
    }
}

#include "moc_runner.cpp"
