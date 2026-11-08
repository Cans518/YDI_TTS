cmake_minimum_required(VERSION 3.19)
project(YDI_TTS VERSION 1.2.1 LANGUAGES CXX)

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

include(cmake/CompilerOptions.cmake)

add_subdirectory(src)

# Generate the large embedded font/image object once, without compiling its bytes as C++.
qt_add_big_resources(ydi_resource_objects resources/app.qrc)

qt_add_executable(YDI_TTS WIN32 MACOSX_BUNDLE src/app/main.cpp ${ydi_resource_objects})
if(WIN32)
    enable_language(RC)
    target_sources(YDI_TTS PRIVATE resources/app.rc)
    target_include_directories(YDI_TTS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()
target_link_libraries(YDI_TTS PRIVATE tts_app)
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
    qt_add_executable(tts_tests
        tests/ttstests.h tests/tst_tts.cpp
        tests/tst_protocol.cpp tests/tst_widgets.cpp tests/tst_workflows.cpp
        tests/support/memoryport.h tests/support/urlcapture.h
        ${ydi_resource_objects}
    )
    set_target_properties(tts_tests PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)
    target_link_libraries(tts_tests PRIVATE tts_app Qt6::Test)
    configure_tts_executable(tts_tests)
    add_test(NAME tts_tests COMMAND tts_tests)
    set_tests_properties(tts_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
endif()

include(cmake/Install.cmake)
