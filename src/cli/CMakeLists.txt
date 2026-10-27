add_executable(phonekompanion-cli main.cpp)

target_include_directories(phonekompanion-cli PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(phonekompanion-cli PRIVATE
    Qt::DBus
    KF6::CoreAddons
    KF6::I18n
)

install(TARGETS phonekompanion-cli ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
