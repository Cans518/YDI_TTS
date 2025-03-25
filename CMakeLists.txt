cmake_minimum_required(VERSION 3.19)
project(tts_ui LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools SerialPort)



qt_standard_project_setup()

set(APP_ICON_RESOURCE "logo.rc")


qt_add_executable(tts_ui
    WIN32 MACOSX_BUNDLE
    main.cpp
    SerialSender.h
    SerialSender.cpp
    imges.qrc
    logo.rc
)

qt_add_resources(resources imges.qrc)

qt_add_translations(
    TARGETS tts_ui
    TS_FILES tts_ui_zh_CN.ts
)

target_link_libraries(tts_ui
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::SerialPort
    ${resources}
)

include(GNUInstallDirs)

install(TARGETS tts_ui
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET tts_ui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
