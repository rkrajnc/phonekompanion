add_subdirectory(sms)

add_executable(phonekompanion
    main.cpp
    callsinterface.cpp
    phonefeature.cpp
    phoneinterface.cpp
    phonesmodel.cpp
    scrcpyinterface.cpp
)

ecm_add_qml_module(phonekompanion URI io.github.rkrajnc.phonekompanion)

ecm_target_qml_sources(phonekompanion SOURCES
    qml/Main.qml
    qml/PageTitle.qml
    qml/PhonePage.qml
    qml/AppsPage.qml
    qml/ScrcpySettingsPage.qml
    qml/DialerPage.qml
    qml/CallsSettingsPage.qml
)

target_include_directories(phonekompanion PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(phonekompanion PRIVATE
    Qt::DBus
    Qt::Quick
    Qt::Widgets
    KF6::CoreAddons
    KF6::DBusAddons
    KF6::I18n
    KF6::I18nQml
    KF6::WindowSystem
    phonekompanionsmsplugin
)

install(TARGETS phonekompanion ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES io.github.rkrajnc.phonekompanion.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(PROGRAMS io.github.rkrajnc.phonekompanion.desktop io.github.rkrajnc.phonekompanion.dialer.desktop io.github.rkrajnc.phonekompanion.messages.desktop DESTINATION ${KDE_INSTALL_APPDIR})
