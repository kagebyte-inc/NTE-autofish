cmake_minimum_required(VERSION 3.21)

project(autofish VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 QuickEffects Widgets)

qt_standard_project_setup(REQUIRES 6.5)

add_compile_definitions(AUTOFISH_VERSION="${PROJECT_VERSION}")

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(autofish
    src/AppController.cpp
    src/AppController.h
    src/AppControllerSettings.cpp
    src/FishingFlowController.cpp
    src/FishingFlowController.h
    src/ReelController.cpp
    src/ReelController.h
    src/AppSettings.cpp
    src/AppSettings.h
    src/I18nCatalog.cpp
    src/I18nCatalog.h
    src/InputDevice.cpp
    src/InputDevice.h
    src/VisionService.cpp
    src/VisionService.h
    src/main.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(autofish
    URI Autofish
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppShell.qml
        qml/Theme.qml
        qml/GlassButton.qml
        qml/SkinComboBox.qml
        qml/components/AppModal.qml
        qml/components/GlassCard.qml
        qml/components/InfoButton.qml
        qml/components/ReelModeCard.qml
        qml/components/SettingsNavItem.qml
        qml/components/SettingsRow.qml
        qml/components/SettingsSection.qml
        qml/components/SectionTitle.qml
        qml/components/SetupChoice.qml
        qml/components/SmallCaption.qml
        qml/components/TuningSlider.qml
        qml/components/TuningSpinBox.qml
        qml/pages/DebugPage.qml
        qml/pages/HomePage.qml
        qml/pages/SettingsPage.qml
        qml/pages/SetupPage.qml
    RESOURCES
        qml/assets/icons/all_reset.png
        qml/assets/icons/autofish-icon.png
        qml/assets/icons/debug.png
        qml/assets/icons/exp_reset.png
        qml/assets/icons/ez.png
        qml/assets/icons/home.png
        qml/assets/icons/info.png
        qml/assets/icons/lang.png
        qml/assets/icons/pro.png
        qml/assets/icons/settings.png
        qml/assets/icons/wayland.png
        qml/assets/icons/windows.png
        qml/assets/icons/x11.png
        qml/i18n/en.json
        qml/i18n/zh_CN.json
        qml/i18n/ja.json
        qml/i18n/ru.json
)

target_link_libraries(autofish
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickEffects
        Qt6::Widgets
)

set(AUTOFISH_DIST_DIR "${CMAKE_BINARY_DIR}/dist")

add_custom_command(TARGET autofish POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOFISH_DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:autofish>"
            "${AUTOFISH_DIST_DIR}/$<TARGET_FILE_NAME:autofish>"
    COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${AUTOFISH_DIST_DIR}/python/autofish_vision"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${AUTOFISH_DIST_DIR}/python"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/python/autofish_vision"
            "${AUTOFISH_DIST_DIR}/python/autofish_vision"
    COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${AUTOFISH_DIST_DIR}/python/autofish_vision/__pycache__"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/requirements.txt"
            "${AUTOFISH_DIST_DIR}/requirements.txt"
    COMMENT "Staging runnable Autofish bundle in ${AUTOFISH_DIST_DIR}"
)