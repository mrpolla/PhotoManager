cmake_minimum_required(VERSION 3.16)
project(PhotoManager)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)
qt_standard_project_setup()

qt_add_executable(PhotoManager
    main.cpp
    mainwindow.h mainwindow.cpp
    imagegridwidget.h imagegridwidget.cpp
    foldermanager.h foldermanager.cpp
    zoomableimagelabel.h zoomableimagelabel.cpp
    thumbnailservice.h thumbnailservice.cpp
    projectmanager.h projectmanager.cpp
    syncdialog.h syncdialog.cpp
)

target_link_libraries(PhotoManager PRIVATE
    Qt6::Widgets
    Qt6::Sql
)
