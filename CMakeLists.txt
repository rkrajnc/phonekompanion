cmake_minimum_required(VERSION 3.16)

project(phonekompanion VERSION 0.1.0)

set(QT_MIN_VERSION "6.8.0")
set(KF_MIN_VERSION "6.8.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMSetupVersion)
include(ECMQtDeclareLoggingCategory)
include(ECMQmlModule)
include(ECMFindQmlModule)
include(FeatureSummary)
include(KDEClangFormat)
include(KDEGitCommitHooks)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui Network Qml Quick Widgets)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons DBusAddons I18n Notifications StatusNotifierItem WindowSystem)

ecm_find_qmlmodule(org.kde.kirigami REQUIRED)
ecm_find_qmlmodule(org.kde.prison REQUIRED)

ecm_setup_version(PROJECT
    VARIABLE_PREFIX PHONEKOMPANION
    VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/phonekompanion-version.h
)

add_definitions(-DTRANSLATION_DOMAIN=\"phonekompanion\")
# As in KDE Connect, where most of the code comes from
add_definitions(-DQT_NO_URL_CAST_FROM_STRING -DQT_NO_KEYWORDS -DQT_NO_CAST_FROM_ASCII -DQT_NO_CONTEXTLESS_CONNECT)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(src/daemon)
add_subdirectory(src/app)
add_subdirectory(src/cli)

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES src/*.cpp src/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
