cmake_minimum_required(VERSION 3.19)
project(YDI_TTS VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)
option(YDI_SINGLE_FILE "Build a standalone Windows executable with static Qt" OFF)
option(YDI_DEPLOY_RUNTIME "Install Qt runtime dependencies with the application" ON)
if(YDI_SINGLE_FILE)
    set(QT_SKIP_AUTO_PLUGIN_INCLUSION ON)
endif()
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets SerialPort Svg)
if(NOT WIN32)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core5Compat)
endif()
qt_standard_project_setup()

if(YDI_SINGLE_FILE)
    get_target_property(qt_core_type Qt6::Core TYPE)
    if(NOT WIN32 OR NOT qt_core_type STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "YDI_SINGLE_FILE requires Windows and a static Qt installation.")
    endif()
    set(ydi_static_plugins)
    foreach(plugin QWindowsIntegrationPlugin QOffscreenIntegrationPlugin QSvgIconPlugin QSvgPlugin QICOPlugin)
        find_package(Qt6${plugin} CONFIG REQUIRED PATHS "${Qt6Gui_DIR}" NO_DEFAULT_PATH)
        list(APPEND ydi_static_plugins Qt6::${plugin})
    endforeach()
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

function(configure_tts_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/utf-8;/W4>")
    else()
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>")
    endif()
endfunction()

function(configure_tts_executable target)
    configure_tts_target(${target})
    if(YDI_SINGLE_FILE)
        target_sources(${target} PRIVATE src/app/staticplugins.cpp)
        target_link_libraries(${target} PRIVATE ${ydi_static_plugins})
        if(MINGW)
            target_link_options(${target} PRIVATE -static)
        endif()
    endif()
endfunction()

add_library(tts_ui STATIC
    src/app/serialsender.h src/app/serialsender.cpp
    src/core/ttsprotocol.h src/core/ttsprotocol.cpp
    src/core/serialtransaction.h src/core/serialtransaction.cpp
    src/ui/customlevelslider.h src/ui/customlevelslider.cpp
    src/ui/levelsliderwidget.h src/ui/levelsliderwidget.cpp
    src/ui/animatedbutton.h src/ui/animatedbutton.cpp
    src/ui/activityindicator.h src/ui/activityindicator.cpp
    src/ui/uitheme.h src/ui/uitheme.cpp
    src/ui/hoverhint.h src/ui/hoverhint.cpp
    src/ui/parameterpopup.h src/ui/parameterpopup.cpp
    src/ui/noticecarousel.h src/ui/noticecarousel.cpp
)
target_include_directories(tts_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(tts_ui PUBLIC Qt6::Core Qt6::Widgets Qt6::SerialPort Qt6::Svg)
if(NOT WIN32)
    target_link_libraries(tts_ui PUBLIC Qt6::Core5Compat)
endif()
configure_tts_target(tts_ui)

# Generate the large embedded font/image object once, without compiling its bytes as C++.
qt_add_big_resources(ydi_resource_objects resources/app.qrc)

qt_add_executable(YDI_TTS WIN32 MACOSX_BUNDLE src/app/main.cpp ${ydi_resource_objects})
if(WIN32)
    enable_language(RC)
    target_sources(YDI_TTS PRIVATE resources/app.rc)
    target_include_directories(YDI_TTS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()
target_link_libraries(YDI_TTS PRIVATE tts_ui)
if(APPLE)
    set_target_properties(YDI_TTS PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "cn.ydich.tts"
        MACOSX_BUNDLE_BUNDLE_NAME "长风 TTS"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "app.icns"
    )
    target_sources(YDI_TTS PRIVATE resources/icons/app.icns)
    set_source_files_properties(resources/icons/app.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()
target_compile_definitions(YDI_TTS PRIVATE YDI_TTS_VERSION="${PROJECT_VERSION}")
configure_tts_executable(YDI_TTS)
if(YDI_SINGLE_FILE)
    set_target_properties(YDI_TTS PROPERTIES OUTPUT_NAME YDI_TTS_Portable)
    if(MINGW)
        target_link_options(YDI_TTS PRIVATE -s)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(tts_tests tests/tst_tts.cpp ${ydi_resource_objects})
    set_target_properties(tts_tests PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
    target_link_libraries(tts_tests PRIVATE tts_ui Qt6::Test)
    configure_tts_executable(tts_tests)
    add_test(NAME tts_tests COMMAND tts_tests)
    set_tests_properties(tts_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
endif()

include(GNUInstallDirs)
install(TARGETS YDI_TTS
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if(APPLE)
    set(ydi_license_destination "YDI_TTS.app/Contents/Resources/licenses")
else()
    set(ydi_license_destination "${CMAKE_INSTALL_DATADIR}/YDI_TTS/licenses")
endif()
install(FILES LICENSE resources/licenses/remixicon-LICENSE.txt resources/licenses/font-OFL.txt
    resources/licenses/qt-LGPL-3.0.txt resources/licenses/THIRD_PARTY.md
    DESTINATION "${ydi_license_destination}"
)
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/cn.ydich.tts.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/cn.ydich.tts.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()
if(YDI_DEPLOY_RUNTIME AND NOT YDI_SINGLE_FILE)
    qt_generate_deploy_app_script(
        TARGET YDI_TTS OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
