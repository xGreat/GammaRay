# probe plugin
if (NOT GAMMARAY_CLIENT_ONLY_BUILD)
    set(gammaray_a11y_srcs
        accessibility.cpp
        accessibilitymodel.cpp
    )
    gammaray_add_plugin(gammaray_accessibility JSON gammaray_accessibility.json SOURCES ${gammaray_a11y_srcs})
    target_link_libraries(gammaray_accessibility gammaray_core gammaray_kitemmodels Qt5::Gui)
endif()

# ui plugin
if(GAMMARAY_BUILD_UI)
    set(gammaray_a11y_ui_srcs
        accessibilitywidget.cpp
  )
  gammaray_add_plugin(gammaray_accessibility_ui JSON gammaray_accessibility.json SOURCES ${gammaray_a11y_ui_srcs})
  target_link_libraries(gammaray_accessibility_ui gammaray_ui)
endif()
