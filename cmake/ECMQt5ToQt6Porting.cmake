#.rst:
# ECMQt5ToQt6Porting
# ------------------
#
# When included in a Qt5 context this module creates version-less targets
# for Qt5 library targets. Qt6 provides such targets by default already.
# This helps with supporting both versions with significantly reduced
# conditional code in the build system.
#
# Since 5.78.0.
#=============================================================================
# SPDX-FileCopyrightText: 2020 Volker Krause <vkrause@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

macro(_create_versionless_target LIB)
    if (NOT TARGET Qt::${LIB} AND TARGET Qt5::${LIB})
        add_library(Qt::${LIB} INTERFACE IMPORTED)
        set_target_properties(Qt::${LIB} PROPERTIES
            INTERFACE_LINK_LIBRARIES Qt5::${LIB}
            _qt_is_versionless_target "TRUE"
        )
    endif()
    if (NOT TARGET Qt::${LIB}Private AND TARGET Qt5::${LIB}Private)
        add_library(Qt::${LIB}Private INTERFACE IMPORTED)
        set_target_properties(Qt::${LIB}Private PROPERTIES
            INTERFACE_LINK_LIBRARIES Qt5::${LIB}Private
        )
    endif()

    set(_vars
        VERSION_MAJOR
        VERSION_MINOR
        VERSION_PATCH
        FOUND
    )

    foreach (var ${_vars})
        if (NOT DEFINED Qt${LIB}_${var} AND DEFINED Qt5${LIB}_${var})
            set(Qt${LIB}_${var} ${Qt5${LIB}_${var}})
        endif()
        if (NOT DEFINED Qt${LIB}_${var} AND DEFINED Qt6${LIB}_${var})
            set(Qt${LIB}_${var} ${Qt6${LIB}_${var}})
        endif()
    endforeach()
endmacro()

set(_libs
    3DAnimation
    3DCore
    3DExtras
    3DInput
    3DLogic
    3DQuick
    3DQuickAnimation
    3DQuickExtras
    3DQuickInput
    3DQuickRender
    3DQuickScene2D
    3DRender
    AndroidExtras
    Bluetooth
    Charts
    Concurrent
    Core
    DBus
    Designer
    DesignerComponents
    EglFSDeviceIntegration
    EglFsKmsSupport
    Gui
    Help
    HunspellInputMethod
    Location
    Mqtt
    Multimedia
    MultimediaGstTools
    MultimediaQuick
    MultimediaWidgets
    Network
    NetworkAuth
    Nfc
    OpenGL
    Pdf
    PdfWidgets
    Positioning
    PositioningQuick
    PrintSupport
    Qml
    QmlModels
    QmlWorkerScript
    Quick
    QuickControls2
    QuickParticles
    QuickShapes
    QuickTemplates2
    QuickTest
    QuickWidgets
    RemoteObjects
    Scxml
    Sensors
    SerialBus
    SerialPort
    Sql
    Svg
    Test
    TextToSpeech
    VirtualKeyboard
    UiPlugin
    UiTools
    WaylandClient
    WaylandCompositor
    WebChannel
    WebEngine
    WebEngineCore
    WebEngineWidgets
    WebSockets
    Widgets
    X11Extras
    Xml
)

foreach(lib ${_libs})
    _create_versionless_target(${lib})
endforeach()
