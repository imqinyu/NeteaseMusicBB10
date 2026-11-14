TEMPLATE = app
TARGET = test_image_cache
QT += core gui network testlib
CONFIG += console
CONFIG -= app_bundle
INCLUDEPATH += ../src/nm
SOURCES += test_image_cache.cpp ../src/nm/util/NmImageCache.cpp
HEADERS += ../src/nm/util/NmImageCache.hpp
