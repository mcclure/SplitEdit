QT += widgets
QT += xml
requires(qtConfig(filedialog))

HEADERS       = mainwindow.h \
                xmledit.h \
                watchers.h \
                TableWidgetNoScroll.h
SOURCES       = main.cpp \
                mainwindow.cpp \
                xmledit.cpp
#! [0]
RESOURCES     = application.qrc
#! [0]

RC_ICONS = icons/icon.ico
ICON = icons/mac.icns
VERSION = 0.1
DEFINES += PROJECT_VERSION=\\\"$$VERSION\\\"

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/mainwindows/application
INSTALLS += target
