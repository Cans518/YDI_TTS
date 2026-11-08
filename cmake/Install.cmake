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
