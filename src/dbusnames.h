/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

// The daemon's D-Bus API, shared by the daemon and its clients
#define PHONEKOMPANION_SERVICE QStringLiteral("io.github.rkrajnc.phonekompaniond")
#define PHONEKOMPANION_PHONES_PATH QStringLiteral("/phones")
#define PHONEKOMPANION_PHONES_INTERFACE QStringLiteral("io.github.rkrajnc.phonekompanion.phones")
#define PHONEKOMPANION_PHONE_INTERFACE QStringLiteral("io.github.rkrajnc.phonekompanion.phone")
