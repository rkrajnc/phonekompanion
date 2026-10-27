add_executable(phonekompanion
    main.cpp
    callsinterface.cpp
    phonefeature.cpp
    phonesmodel.cpp
    scrcpyinterface.cpp
)

ecm_add_qml_module(phonekompanion URI io.github.rkrajnc.phonekompanion)

ecm_target_qml_sources(phonekompanion SOURCES
    qml/Main.qml
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
)

install(TARGETS phonekompanion ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS io.github.rkrajnc.phonekompanion.desktop io.github.rkrajnc.phonekompanion.dialer.desktop DESTINATION ${KDE_INSTALL_APPDIR})
