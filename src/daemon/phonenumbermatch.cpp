/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "phonenumbermatch.h"

#include <QLocale>

#include "config-phonekompaniond.h"

#if HAVE_PHONENUMBER
#include <phonenumbers/phonenumberutil.h>

using i18n::phonenumbers::PhoneNumber;
using i18n::phonenumbers::PhoneNumberUtil;
#endif

namespace
{
// Region for numbers without a country code: the caller's, else the system's
std::string defaultRegion(const QString &number)
{
    const std::string region = QLocale::territoryToCode(QLocale::system().territory()).toStdString();
#if HAVE_PHONENUMBER
    const PhoneNumberUtil *util = PhoneNumberUtil::GetInstance();
    PhoneNumber parsed;
    if (util->Parse(number.toStdString(), region, &parsed) == PhoneNumberUtil::NO_PARSING_ERROR) {
        std::string callerRegion;
        util->GetRegionCodeForNumber(parsed, &callerRegion);
        if (!callerRegion.empty()) {
            return callerRegion;
        }
    }
#else
    Q_UNUSED(number);
#endif
    return region;
}

QString normalizeNumber(const QString &number, const std::string &region)
{
#if HAVE_PHONENUMBER
    const PhoneNumberUtil *util = PhoneNumberUtil::GetInstance();
    PhoneNumber parsed;
    if (util->Parse(number.toStdString(), region, &parsed) == PhoneNumberUtil::NO_PARSING_ERROR) {
        std::string formatted;
        util->Format(parsed, PhoneNumberUtil::E164, &formatted);
        return QString::fromStdString(formatted);
    }
#else
    Q_UNUSED(region);
#endif
    QString normalized;
    for (const QChar c : number) {
        if (c.isDigit() || (c == QLatin1Char('+') && normalized.isEmpty())) {
            normalized.append(c);
        }
    }
    if (normalized.startsWith(QLatin1String("00"))) {
        normalized.replace(0, 2, QLatin1Char('+'));
    }
    return normalized;
}
}

PhoneNumberMatcher::PhoneNumberMatcher(const QString &number)
    : m_region(defaultRegion(number))
    , m_number(normalizeNumber(number, m_region))
{
}

bool PhoneNumberMatcher::matches(const QString &number) const
{
    return !m_number.isEmpty() && m_number == normalizeNumber(number, m_region);
}
