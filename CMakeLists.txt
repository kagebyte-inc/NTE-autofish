cmake_minimum_required(VERSION 3.21)

project(autofish VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Widgets)

qt_standard_project_setup(REQUIRES 6.5)

add_compile_definitions(AUTOFISH_VERSION="${PROJECT_VERSION}")

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(autofish
    src/AppController.cpp
    src/AppControllerFishing.cpp
    src/AppController.h
    src/AppControllerInput.cpp
    src/AppControllerReel.cpp
    src/AppControllerSettings.cpp
    src/AppControllerVision.cpp
    src/main.cpp
)

qt_add_qml_module(autofish
    URI Autofish
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/GlassButton.qml
        qml/I18n.qml
        qml/SkinButton.qml
        qml/SkinComboBox.qml
    RESOURCES
        qml/assets/nte/app_icon.png
        qml/assets/nte/btn_primary_0.png
        qml/assets/nte/btn_primary_1.png
        qml/assets/nte/btn_primary_2.png
        qml/assets/nte/btn_primary_3.png
        qml/assets/nte/btn_secondary_0.png
        qml/assets/nte/btn_secondary_1.png
        qml/assets/nte/btn_secondary_2.png
        qml/assets/nte/btn_secondary_3.png
        qml/assets/nte/icon_close_0.png
        qml/assets/nte/icon_close_1.png
        qml/assets/nte/icon_close_2.png
        qml/assets/nte/icon_close_3.png
        qml/assets/nte/icon_min_0.png
        qml/assets/nte/icon_min_1.png
        qml/assets/nte/icon_min_2.png
        qml/assets/nte/icon_min_3.png
        qml/assets/nte/icon_setting_0.png
        qml/assets/nte/icon_setting_1.png
        qml/assets/nte/icon_setting_2.png
        qml/assets/nte/icon_setting_3.png
)

target_link_libraries(autofish
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
)
