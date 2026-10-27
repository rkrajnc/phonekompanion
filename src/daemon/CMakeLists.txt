find_package(PhoneNumber OPTIONAL_COMPONENTS PhoneNumber)
set_package_properties(PhoneNumber PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Matches caller numbers to contacts, also when written differently"
)
find_package(KF6PulseAudioQt)
set_package_properties(KF6PulseAudioQt PROPERTIES
    TYPE OPTIONAL
    PURPOSE "Mutes this computer's microphone in calls"
)
set(HAVE_PHONENUMBER ${PhoneNumber_PhoneNumber_FOUND})
set(HAVE_PULSEAUDIO ${KF6PulseAudioQt_FOUND})
configure_file(config-phonekompaniond.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-phonekompaniond.h)

add_executable(phonekompaniond
    main.cpp
    adb.cpp
    bluetooth.cpp
    calls.cpp
    kdeconnect.cpp
    phone.cpp
    phonemanager.cpp
    phonenumbermatch.cpp
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
    KF6::People
    KF6::PeopleBackend
    KF6::StatusNotifierItem
)
if (HAVE_PHONENUMBER)
    target_link_libraries(phonekompaniond PRIVATE PhoneNumber::PhoneNumber)
endif()
if (HAVE_PULSEAUDIO)
    target_link_libraries(phonekompaniond PRIVATE KF6::PulseAudioQt)
endif()

install(TARGETS phonekompaniond ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

configure_file(io.github.rkrajnc.phonekompaniond.service.in ${CMAKE_CURRENT_BINARY_DIR}/io.github.rkrajnc.phonekompaniond.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.rkrajnc.phonekompaniond.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
configure_file(io.github.rkrajnc.phonekompaniond.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/io.github.rkrajnc.phonekompaniond.desktop)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.rkrajnc.phonekompaniond.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.rkrajnc.phonekompaniond.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
install(FILES phonekompaniond.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
install(FILES phonekompanion_runner.desktop DESTINATION ${KDE_INSTALL_DATADIR}/krunner/dbusplugins)
