# KDE Connect's SMS app as a QML module, copied from kdeconnect-kde's smsapp/ and models/; it
# talks to KDE Connect's daemon over D-Bus like kdeconnect-sms does

# The texts are KDE Connect's, so its installed translations work
remove_definitions(-DTRANSLATION_DOMAIN=\"phonekompanion\")
add_definitions(-DTRANSLATION_DOMAIN=\"kdeconnect-sms\")

ecm_qt_declare_logging_category(debug_SRCS
    HEADER sms_conversations_list_debug.h
    IDENTIFIER KDECONNECT_SMS_CONVERSATIONS_LIST_MODEL
    CATEGORY_NAME phonekompanion.sms.conversations_list
    DESCRIPTION "Phone Kompanion (SMS conversation list)"
    EXPORT PHONEKOMPANION
)
ecm_qt_declare_logging_category(debug_SRCS
    HEADER sms_conversation_debug.h
    IDENTIFIER KDECONNECT_SMS_CONVERSATION_MODEL
    CATEGORY_NAME phonekompanion.sms.conversation
    DESCRIPTION "Phone Kompanion (SMS conversation)"
    EXPORT PHONEKOMPANION
)

qt_add_qml_module(phonekompanionsms
    URI io.github.rkrajnc.phonekompanion.sms
    VERSION 1.0
    STATIC
    # Its own plugin class, which adds the thumbnails image provider
    CLASS_NAME PhoneKompanionSmsPlugin
    NO_GENERATE_PLUGIN_SOURCE
    SOURCES
        ${debug_SRCS}
        attachmentinfo.cpp
        attachmentshelper.cpp
        conversationlistmodel.cpp
        conversationmessage.cpp
        conversationmodel.cpp
        conversationssortfilterproxymodel.cpp
        gsmasciimap.cpp
        kdeconnectinterfaces.cpp
        smshelper.cpp
        smsqmltypes.h
        thumbnailsprovider.cpp
    QML_FILES
        qml/AttachmentViewer.qml
        qml/ChatMessage.qml
        qml/ConversationDisplay.qml
        qml/ConversationList.qml
        qml/MessageAttachments.qml
        qml/SendingArea.qml
)

target_sources(phonekompanionsmsplugin PRIVATE smsplugin.cpp)
target_link_libraries(phonekompanionsmsplugin PRIVATE Qt::Gui Qt::Quick)

target_include_directories(phonekompanionsms PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(phonekompanionsms PRIVATE
    Qt::DBus
    Qt::Gui
    Qt::Qml
    Qt::Quick
    Qt::Widgets
    KF6::I18n
    KF6::People
)
