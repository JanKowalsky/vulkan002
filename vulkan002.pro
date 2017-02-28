#-------------------------------------------------
#
# Project created by QtCreator 2017-02-25T00:20:29
#
#-------------------------------------------------

QT          += core gui
QT          += x11extras

CONFIG      += c++14

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = vulkan002
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES +=\
        VulkanWindow.cpp \
    camera.cpp \
    debug.cpp \
    engine.cpp \
    myscene_utils.cpp \
    myscene.cpp \
    recorder.cpp \
    shader.cpp \
    Source.cpp \
    Timer.cpp \
    model.cpp

HEADERS  += VulkanWindow.h \
    camera.h \
    debug.h \
    engine.h \
    myscene_utils.h \
    myscene.h \
    recorder.h \
    Scene.h \
    shader.h \
    Timer.h \
    vulkan_math.h \
    model.h

FORMS    += VulkanWindow.ui

DISTFILES += \
    shader_code/fs.frag \
    shader_code/vs.vert

unix:!macx: LIBS += -L$$PWD/../../VulkanSDK/1.0.39.1/x86_64/lib/ -lvulkan

INCLUDEPATH += $$PWD/../../VulkanSDK/1.0.39.1/x86_64/include
DEPENDPATH += $$PWD/../../VulkanSDK/1.0.39.1/x86_64/include

unix:!macx: LIBS += -lswscale
unix:!macx: LIBS += -lx264
unix:!macx: LIBS += -lavcodec
unix:!macx: LIBS += -lassimp
