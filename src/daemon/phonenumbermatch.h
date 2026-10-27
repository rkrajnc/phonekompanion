/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QString>

#include <string>

/**
 * Compares phone numbers with one number, e.g. a caller's with a contact's numbers.
 * With libphonenumber, "041 415733" matches "+386 41 415 733"; without it, only
 * formatting differences are ignored.
 *
 * Kept out of calls.cpp because libphonenumber's i18n namespace clashes with KI18n's i18n().
 */
class PhoneNumberMatcher
{
public:
    explicit PhoneNumberMatcher(const QString &number);

    bool matches(const QString &number) const;

private:
    std::string m_region;
    QString m_number;
};
