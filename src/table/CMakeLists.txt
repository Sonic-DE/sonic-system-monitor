
add_library(PlasmaSystemMonitorTable)
ecm_add_qml_module(PlasmaSystemMonitorTable URI "org.kde.ksysguard.table" GENERATE_PLUGIN_SOURCE DEPENDENCIES "QtCore" "QtQml" "org.kde.ksysguard.process")

target_sources(PlasmaSystemMonitorTable PRIVATE
    ApplicationIdentityProvider.cpp
    ApplicationIdentityProvider.h
    ColumnDisplayModel.cpp
    ColumnDisplayModel.h
    ColumnSortFilterDisplayModel.cpp
    ColumnSortFilterDisplayModel.h
    ComponentCacheProxyModel.cpp
    ComponentCacheProxyModel.h
    ProcessSortFilterModel.cpp
    ProcessSortFilterModel.h
    ReverseColumnsProxyModel.cpp
    ReverseColumnsProxyModel.h
)

ecm_target_qml_sources(PlasmaSystemMonitorTable SOURCES
    BaseCellDelegate.qml
    BaseTableView.qml
    TextCellDelegate.qml
    ColumnConfigurationDialog.qml
    FirstCellDelegate.qml
    LineChartCellDelegate.qml
    TableViewHeader.qml
    UserCellDelegate.qml
    ReniceDialog.qml
    KillDialog.qml
)

target_link_libraries(PlasmaSystemMonitorTable PRIVATE Qt::Quick KSysGuard::Sensors KSysGuard::ProcessCore KF6::CoreAddons KF6::ItemModels KF6::WindowSystem)

include(CheckIncludeFileCXX)
set(CMAKE_REQUIRED_LIBRARIES KF6::WindowSystem)
check_include_file_cxx(KX11Extras HAVE_APPLICATION_X11)
unset(CMAKE_REQUIRED_LIBRARIES)
target_compile_definitions(PlasmaSystemMonitorTable PRIVATE HAVE_APPLICATION_X11=$<BOOL:${HAVE_APPLICATION_X11}>)

install(TARGETS PlasmaSystemMonitorTable ${KDE_INSTALL_DEFAULT_ARGS})

ecm_finalize_qml_module(PlasmaSystemMonitorTable)
