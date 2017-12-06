QT       -= core gui

TARGET = VBE-Profiler
TEMPLATE = lib
CONFIG += staticlib

unix {
    target.path = /usr/lib
    INSTALLS += target
}

INCLUDEPATH += include src

include(../VBE/VBE.pri)
include(../VBE-Scenegraph/VBE-Scenegraph.pri)

LIBS += -lGLEW -lGL -lSDL2
QMAKE_CXXFLAGS += -std=c++0x -fno-exceptions

OTHER_FILES += \
    VBE-Profiler.pri

HEADERS += \
    include/VBE-Profiler/profiler.hpp \
    include/VBE-Profiler/VBE-Profiler.hpp \
    include/VBE-Profiler/profiler/Profiler.hpp \
    include/VBE-Profiler/profiler/imgui.h \
    include/VBE-Profiler/profiler/imgui_internal.h \
    include/VBE-Profiler/profiler/imconfig.h \
    include/VBE-Profiler/profiler/stb_textedit.h \
    include/VBE-Profiler/profiler/stb_truetype.h \
    include/VBE-Profiler/profiler/stb_rect_pack.h

SOURCES += \
    src/VBE-Profiler/profiler/Profiler.cpp \
    src/VBE-Profiler/profiler/imgui.cpp \
    src/VBE-Profiler/profiler/imgui_demo.cpp \
    src/VBE-Profiler/profiler/imgui_draw.cpp
