# Build from build-vs2019-qt6 with qmake ../tests/Smoke.pro
# Pass SMOKE_TEST=PaintCanvasSmoke (or another smoke source name) to select a test.
include(../build-vs2019-qt6/Boarder.pro)
isEmpty(SMOKE_TEST): SMOKE_TEST = PlaybackSmoke
TARGET = $$SMOKE_TEST
CONFIG += console
CONFIG -= app_bundle
SOURCES -= ../main.cpp
SOURCES += $$PWD/$${SMOKE_TEST}.cpp
equals(SMOKE_TEST, UndoHistorySmoke): QT += testlib
