find_package(Qt6 REQUIRED CONFIG COMPONENTS Test)

# Resolve package inputs once, then expose a uniform build-local test runtime.
# Tests themselves use no installed-prefix or distribution-specific paths.
file(RELATIVE_PATH _application_qml_relative "${KDE_INSTALL_FULL_LIBDIR}" "${KDE_INSTALL_FULL_QMLDIR}")
file(RELATIVE_PATH _application_plugins_relative "${KDE_INSTALL_FULL_LIBDIR}" "${KDE_INSTALL_FULL_PLUGINDIR}")
file(RELATIVE_PATH _application_data_relative "${KDE_INSTALL_FULL_LIBDIR}" "${KDE_INSTALL_FULL_DATAROOTDIR}")
foreach(_kind qml plugins data)
    set(_application_sdk_${_kind} "$<TARGET_FILE_DIR:KSysGuard::ProcessCore>/${_application_${_kind}_relative}")
    set(_application_framework_${_kind} "$<TARGET_FILE_DIR:KF6::WindowSystem>/${_application_${_kind}_relative}")
endforeach()
find_program(DBUS_RUN_SESSION_EXECUTABLE dbus-run-session REQUIRED)
configure_file(applications_test_dbus.conf applications_test_dbus.conf COPYONLY)
configure_file(application_test_runtime.cmake.in application_test_runtime.configured @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/application_test_runtime-$<CONFIG>.cmake"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/application_test_runtime.configured")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/applications-runtime/$<CONFIG>/tmp/.anchor" CONTENT "")
set(_application_runtime_launcher "${CMAKE_CURRENT_BINARY_DIR}/application_test_runtime-$<CONFIG>.cmake")
file(RELATIVE_PATH _application_information_qml "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    "${CMAKE_CURRENT_BINARY_DIR}/applications-ui-data/ksysguard/sensorfaces/org.kde.ksysguard.applicationstable/contents/ui/ApplicationInformation.qml")

add_executable(TestApplicationIdentity TestApplicationIdentity.cpp ../src/table/ApplicationIdentityProvider.cpp)
target_link_libraries(TestApplicationIdentity PRIVATE Qt::Test Qt::Quick KSysGuard::ProcessCore KF6::WindowSystem)
ecm_mark_as_test(TestApplicationIdentity)
target_include_directories(TestApplicationIdentity PRIVATE ../src)
target_compile_definitions(TestApplicationIdentity PRIVATE
    QT_FORCE_ASSERTS
    HAVE_APPLICATION_X11=$<BOOL:${HAVE_APPLICATION_X11}>
    APPLICATION_INFORMATION_QML="${_application_information_qml}")
add_test(NAME TestApplicationIdentity COMMAND "${CMAKE_COMMAND}"
    -DAPPLICATION_TEST_EXECUTABLE=$<TARGET_FILE:TestApplicationIdentity> -DAPPLICATION_TEST_PLATFORM=offscreen
    -P "${_application_runtime_launcher}")

add_executable(TestApplicationsUi TestApplicationsUi.cpp)
target_link_libraries(TestApplicationsUi PRIVATE Qt::Test Qt::Quick KSysGuard::ProcessCore KSysGuard::SensorFaces KF6::ConfigCore KF6::CoreAddons KF6::I18nQml)
ecm_mark_as_test(TestApplicationsUi)
add_test(NAME TestApplicationsUi COMMAND "${CMAKE_COMMAND}"
    -DAPPLICATION_TEST_EXECUTABLE=$<TARGET_FILE:TestApplicationsUi> -DAPPLICATION_TEST_PLATFORM=offscreen
    -DAPPLICATION_TEST_USE_DBUS=ON -P "${_application_runtime_launcher}")
add_dependencies(TestApplicationsUi PlasmaSystemMonitorTableplugin)
file(GLOB_RECURSE _application_face_files CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_SOURCE_DIR}/src/faces/applicationstable" "${CMAKE_SOURCE_DIR}/src/faces/applicationstable/*")
foreach(_file IN LISTS _application_face_files)
    configure_file("${CMAKE_SOURCE_DIR}/src/faces/applicationstable/${_file}"
        "${CMAKE_CURRENT_BINARY_DIR}/applications-ui-data/ksysguard/sensorfaces/org.kde.ksysguard.applicationstable/${_file}" COPYONLY)
endforeach()
set_tests_properties(TestApplicationIdentity TestApplicationsUi PROPERTIES TIMEOUT 30)
find_program(XVFB_RUN_EXECUTABLE xvfb-run)
if(XVFB_RUN_EXECUTABLE AND HAVE_APPLICATION_X11)
    add_test(NAME TestApplicationsX11 COMMAND "${XVFB_RUN_EXECUTABLE}" -a "${CMAKE_COMMAND}"
        -DAPPLICATION_TEST_EXECUTABLE=$<TARGET_FILE:TestApplicationsUi> -DAPPLICATION_TEST_PLATFORM=xcb
        -DAPPLICATION_TEST_USE_DBUS=ON -P "${_application_runtime_launcher}")
    set_tests_properties(TestApplicationsX11 PROPERTIES TIMEOUT 30
        ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}/applications-runtime/$<CONFIG>/tmp")
endif()

include_directories(../src/page ${CMAKE_BINARY_DIR}/src/page)

file(GLOB _global_pages CONFIGURE_DEPENDS data/global/*.page)
foreach(_page ${_global_pages})
    get_filename_component(_file ${_page} NAME)
    configure_file("${_page}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestPageManager_data/plasma-systemmonitor/${_file}" FILE_PERMISSIONS OWNER_READ GROUP_READ WORLD_READ COPYONLY)
endforeach()

add_executable(TestPageManager
    TestPageManager.cpp
    ../src/page/PageManager.cpp
    ../src/page/PageController.cpp
    ../src/page/PageDataObject.cpp
    ../src/page/FaceLoader.cpp
)
target_link_libraries(TestPageManager PRIVATE
    Qt6::Quick
    Qt6::Test
    KF6::ConfigCore
    KF6::NewStuffCore
    KSysGuard::Sensors
    KSysGuard::SensorFaces
)
target_compile_definitions(TestPageManager PRIVATE QT_FORCE_ASSERTS)
ecm_mark_as_test(TestPageManager)
add_test(NAME TestPageManager COMMAND "${CMAKE_COMMAND}"
    -DAPPLICATION_TEST_EXECUTABLE=$<TARGET_FILE:TestPageManager> -DAPPLICATION_TEST_PLATFORM=offscreen
    -DAPPLICATION_TEST_USE_DBUS=ON -P "${_application_runtime_launcher}")
set_tests_properties(TestPageManager PROPERTIES TIMEOUT 30)

kconfig_target_kcfg_file(TestPageManager FILE ${CMAKE_SOURCE_DIR}/src/page/systemmonitor.kcfg CLASS_NAME Configuration MUTATORS GENERATE_PROPERTIES GENERATE_MOC SINGLETON QML_REGISTRATION)
