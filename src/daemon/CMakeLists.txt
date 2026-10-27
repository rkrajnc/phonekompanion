add_executable(phonekompaniond
    main.cpp
    adb.cpp
    kdeconnect.cpp
    phone.cpp
    phonemanager.cpp
    runner.cpp
    scrcpy.cpp
    tray.cpp
)

ecm_qt_declare_logging_category(phonekompaniond
    HEADER phonekompaniond_debug.h
    IDENTIFIER PHONEKOMPANIOND
    CATEGORY_NAME phonekompanion.daemon
    DESCRIPTION "Phone Kompanion daemon"
    EXPORT PHONEKOMPANION
)

target_include_directories(phonekompaniond PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(phonekompaniond PRIVATE
    Qt::DBus
    Qt::Network
    Qt::Widgets
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::DBusAddons
    KF6::I18n
    KF6::Notifications
    KF6::StatusNotifierItem
)

install(TARGETS phonekompaniond ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

configure_file(io.github.rkrajnc.phonekompaniond.service.in ${CMAKE_CURRENT_BINARY_DIR}/io.github.rkrajnc.phonekompaniond.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.rkrajnc.phonekompaniond.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
install(FILES io.github.rkrajnc.phonekompaniond.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES io.github.rkrajnc.phonekompaniond.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
install(FILES phonekompaniond.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
install(FILES phonekompanion_runner.desktop DESTINATION ${KDE_INSTALL_DATADIR}/krunner/dbusplugins)
