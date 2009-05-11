# -------------------------------------------------
# Project created by QtCreator 2009-04-07T21:47:39
# -------------------------------------------------
TARGET = teQuilla
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp \
    qtorrentstablemodel.cpp \
    qpeerstablemodel.cpp
HEADERS += mainwindow.h \
    qtorrentstablemodel.h \
    qpeerstablemodel.h
FORMS += mainwindow.ui
INCLUDEPATH += ../include/tortilla
LIBS += ../lib/libtortilla.a \
    -lssl \
    -lcurl \
    -lncurses \
    -I../include/tortilla
