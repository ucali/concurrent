TEMPLATE = app
CONFIG -= qt
CONFIG -= app_bundle
CONFIG += console
CONFIG += c++11

TARGET = TestConcurrent

INCLUDEPATH += $$_PRO_FILE_PWD_/include
INCLUDEPATH += $$_PRO_FILE_PWD_/test/Catch/include
#LIBS += -L$$_PRO_FILE_PWD_/lib

CONFIG(debug, debug|release) {
} else {
}

SOURCES +=  test/main.cpp   \
            test/pool.cpp   \
            test/queue.cpp  \

CONFIG(debug, debug|release) {
        TARGET = $$join(TARGET,,,d)
}

DESTDIR = $$_PRO_FILE_PWD_/bin

HEADERS += \
    include/pool.hpp \
    include/queue.hpp
