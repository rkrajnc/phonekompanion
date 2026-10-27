/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <qqmlregistration.h>

#include "attachmentshelper.h"
#include "conversationlistmodel.h"
#include "conversationmodel.h"
#include "conversationssortfilterproxymodel.h"
#include "smshelper.h"

// The QML names KDE Connect's SMS QML uses, from its kdeconnect-sms

class ConversationsSortFilterProxyModelForeign
{
    Q_GADGET
    QML_NAMED_ELEMENT(QSortFilterProxyModel)
    QML_FOREIGN(ConversationsSortFilterProxyModel)
};

class ConversationModelForeign
{
    Q_GADGET
    QML_NAMED_ELEMENT(ConversationModel)
    QML_FOREIGN(ConversationModel)
};

class ConversationListModelForeign
{
    Q_GADGET
    QML_NAMED_ELEMENT(ConversationListModel)
    QML_FOREIGN(ConversationListModel)
};

class SmsHelperForeign
{
    Q_GADGET
    QML_NAMED_ELEMENT(SmsHelper)
    QML_SINGLETON
    QML_FOREIGN(SmsHelper)
};

class AttachmentsHelperForeign
{
    Q_GADGET
    QML_NAMED_ELEMENT(AttachmentsHelper)
    QML_SINGLETON
    QML_FOREIGN(AttachmentsHelper)
};
