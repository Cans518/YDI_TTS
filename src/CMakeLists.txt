add_library(tts_core STATIC
    core/ttsprotocol.h core/ttsprotocol.cpp
    core/serialtransaction.h core/serialtransaction.cpp
)
target_include_directories(tts_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tts_core PUBLIC Qt6::Core)
if(NOT WIN32)
    target_link_libraries(tts_core PRIVATE Qt6::Core5Compat)
endif()
configure_tts_target(tts_core)

add_library(tts_ui STATIC
    ui/customlevelslider.h ui/customlevelslider.cpp
    ui/levelsliderwidget.h ui/levelsliderwidget.cpp
    ui/animatedbutton.h ui/animatedbutton.cpp
    ui/activityindicator.h ui/activityindicator.cpp
    ui/uitheme.h ui/uitheme.cpp
    ui/hoverhint.h ui/hoverhint.cpp
    ui/parameterpopup.h ui/parameterpopup.cpp
    ui/noticecarousel.h ui/noticecarousel.cpp
)
target_include_directories(tts_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tts_ui PUBLIC tts_core Qt6::Widgets Qt6::Svg)
configure_tts_target(tts_ui)

add_library(tts_app STATIC
    app/serialsender.h app/serialsender.cpp app/serialsender_view.cpp
)
target_link_libraries(tts_app PUBLIC tts_ui Qt6::SerialPort)
configure_tts_target(tts_app)
