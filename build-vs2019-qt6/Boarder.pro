
TEMPLATE = app
TARGET = Boarder

VERSION = 1.0.0
DEFINES += VERSION_STRING=\\\"$$VERSION\\\"

message(GameFusion env value set to $$(GameFusion))
GF=$$(GameFusion)
isEmpty(GF) {
	GF=../../..
	message(Not found found GameFusion setting value to $$GF)
} else {
	message(Found GameFusion at $$(GameFusion))
	GF=$$(GameFusion)
}

mac {
    GF=/Users/andreascarlen/GameFusion
}

CONFIG += no_batch

DEPENDPATH += .
INCLUDEPATH += ..
INCLUDEPATH += $$GF/GameEngine/SoundServer $$GF/GameEngine/InputDevice $$GF/GameEngine/WindowDevice $$GF/GameEngine/DataStructures $$GF/GameEngine/GameCore $$GF/GameEngine/SceneGraph $$GF/GameEngine/Math3D $$GF/GameEngine/MaterialLighting $$GF/GameEngine/Geometry $$GF/GameEngine/AssetManagement $$GF/GameEngine/Texture $$GF/GameEngine/GraphicsDevice/GraphicsDeviceCore $$GF/GameEngine/GraphicsDevice/GraphicsDeviceOpenGL $$GF/GameEngine/Character $$GF/GameEngine/GameFramework $$GF/GameEngine/GUI/GUICore $$GF/GameEngine/GUI/GUIGame $$GF/GameEngine/GUI/GUIQT $$GF/GameEngine/Animation $$GF/GameEngine/GameLevel $$GF/GameEngine/GameThread $$GF/GameEngine/FontEngine
INCLUDEPATH += $$GF/GameEngine/LightmapFramework
INCLUDEPATH += $$GF/GameEngine/GamePlay
INCLUDEPATH += $$GF/GameEngine/HarmonicCoordinates
INCLUDEPATH += $$GF/GameEngine/Applications/CharacterHarmonics
INCLUDEPATH += $$GF/ExternalLibs/harmonicBlender
INCLUDEPATH += $$GF/GameEngine/GameFusion
INCLUDEPATH += $$GF/GameEngine/ParticleSystems
INCLUDEPATH += $$GF/GameEngine/Ez
INCLUDEPATH += $$GF/GameEngine/EzRun
INCLUDEPATH += $$GF/GameEngine/Collision
INCLUDEPATH += $$GF/GameEngine/GameWeb
INCLUDEPATH += $$GF/Projects/PhoneDEC/dec_phone_corr/
INCLUDEPATH += $$GF/GameEngine/Demos/Draw

RESOURCES += $$GF/Applications/CommonQt/qdarkstyle/style.qrc

macx {
	
CONFIG(debug, debug|release) {
        message("+++++ debug mode")

        LIBS += -L$$GF/GameEngine/Libs/macOS/Debug -l"GameFusion Static Library Debug OSX"
        LIBS += -L$$GF/english2phoneme/Build/Products/Debug -lword2phone

}else {
        message("release mode")
        LIBS += -L$$GF/GameEngine/Libs/macOS/Debug -l"GameFusion Static Library Debug OSX"
        LIBS += -L$$GF/english2phoneme/Build/Products/Debug -lword2phone

}

	LIBS += -framework AudioToolbox -framework CoreFoundation
        #ffmpeg libs
        LIBS += -L/opt/local/lib/
        LIBS += -lswscale
        LIBS += -lavformat
        LIBS += -lavcodec
        LIBS += -lavutil
        LIBS += -lswscale
        LIBS += -lswresample
        #LIBS += ../../ExternalLibs/harmonicBlender/Xcode/Release/libharmonicBlender.a
	LIBS += -framework CoreVideo
	LIBS += -framework VideoDecodeAcceleration
	LIBS += -lbz2
	LIBS += -lz

        #PRIVATE_FRAMEWORKS.files = data
        #PRIVATE_FRAMEWORKS.path = Contents/Resources
        #QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS
	
    # Define the data directory for the bundle
    #DATA_DIR.files = $$PWD/data
    #DATA_DIR.path = Contents/Resources
    #QMAKE_BUNDLE_DATA += DATA_DIR

    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15

        QMAKE_CXXFLAGS += -std=c++20 -DUSE_NATIVE_MENU -Wregister -Wimplicit-function-declaration

        LIBS += ../../../../plugandpaint/plugins/build/plugins/libpnp_basictools_debug.a
}


unix:!macx{
  # linux only
    LIBS += -L../../GameEngine/Libs/LinuxGCC/ -L../../ExternalLibs/harmonicBlender/
    LIBS += -L../../ExternalLibs/x264/
    LIBS += -L../../ExternalLibs/ffmpeg/libavcodec/
    LIBS += -L../../ExternalLibs/ffmpeg/libavfilter/
    LIBS += -L../../ExternalLibs/ffmpeg/libavformat/
    LIBS += -L../../ExternalLibs/ffmpeg/libavutil/
    LIBS += -L../../ExternalLibs/ffmpeg/libswscale/
    LIBS += -lGamePlay -lharmonicBlender -lHarmonicCoordinates -lGameFramework -lCloth -lGameLevel -lEz -lEzRun -lGameNetwork  -lLightmapFramework -lGUIGame -lGUICore -lVideo -lSceneGraph -lGameCore -lParticleSystems -lCharacter -lAnimation -lFontEngine -lPhonemeRecognizer -lVisualFFT -lSoundServer -lGameThread -lGeometry -lTexture -lMaterialLighting -lInputDevice -lWindowDevice -lGraphicsDeviceOpenGL -lGraphicsDeviceCore -lAssetManagement -lMath3D -lDataStructures 
    LIBS += -lavformat -lavcodec -lavutil -lswscale -lx264 -lbz2 -lz
    LIBS += -lhpdf
    LIBS += -lpng -ljpeg
    LIBS += -lGLU -lGL
    LIBS += -lasound
    LIBS += -lstdc++
    
    SOURCES += ../../GameEngine/GenericDevice/GenericDevice.cpp
    SOURCES += ../../GameEngine/GenericDevice/GenericInput.cpp

    DEFINES += Linux
    DEFINES += LINUX
	QMAKE_CXXFLAGS += -std=c++11
	
	SOURCES += ../../Projects/PhoneDEC/dec_phone_corr/DEC.cpp
	SOURCES += ../../Projects/PhoneDEC/dec_phone_corr/Phone.cpp
}



win32 {
   LIBS	+= -lglu32 -lopengl32 -lUser32
   LIBS += legacy_stdio_definitions.lib 
   
   CONFIG(debug, debug|release) {
	   DEFINES += DEBUG

		LIBS += $$GF\GameEngine\build-vs2019\Debug\GameEngine.lib $$GF\GameEngine\build-vs2019\Debug\GameEngineGL.lib $$GF\GameEngine\build-vs2019\Debug\GameFramework.lib
		LIBS += $$GF/ExternalLibs/libharu-2.1.0/x64/Debug/libharu.lib
		
		LIBS += comdlg32.lib
		SOURCES += $$GF/Projects/PhoneDEC/dec_phone_corr/Phone.cpp
		SOURCES += $$GF/Projects/PhoneDEC/dec_phone_corr/DEC.cpp
		LIBS += $$GF\ExternalLibs\libjpeg-win64-vs2019\x64\Debug\jpeg.lib
		LIBS += $$GF/ExternalLibs/libpng-1.6.24/build-win64-vs2019/Debug/libpng16_staticd.lib
		LIBS += $$GF/ExternalLibs/zlib-vs2019/Debug/zlib.lib

		LIBS += -lpnp_basictoolsd
	} else {
		LIBS += $$GF\GameEngine\build-vs2019\Release\GameEngine.lib $$GF\GameEngine\build-vs2019\Release\GameEngineGL.lib $$GF\GameEngine\build-vs2019\Release\GameFramework.lib
		#LIBS += $$GF/ExternalLibs/harmonicBlender/x64/Release/harmonicBlender.lib
		LIBS += $$GF/ExternalLibs/libharu-2.1.0/x64/Release/libharu.lib
		SOURCES += $$GF/Projects/PhoneDEC/dec_phone_corr/Phone.cpp
		SOURCES += $$GF/Projects/PhoneDEC/dec_phone_corr/DEC.cpp
		LIBS += $$GF\ExternalLibs\libjpeg-win64-vs2019\Release\libjpeg.lib
		
		LIBS += $$GF/ExternalLibs/libpng-1.6.24/build-win64-vs2019/Release/libpng16_static.lib
		LIBS += "C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib\mariadbclient.lib"
		LIBS += crypt32.lib secur32.lib
		LIBS += $$GF/ExternalLibs/zlib-vs2019/Release/zlib.lib

		LIBS += -lpnp_basictools
		LIBS += comdlg32.lib
   }

   LIBS += $$GF\ExternalLibs\ffmpeg-20200806-2c35797-win64-dev\lib\avformat.lib
   LIBS += $$GF\ExternalLibs\ffmpeg-20200806-2c35797-win64-dev\lib\avcodec.lib
   LIBS += $$GF\ExternalLibs\ffmpeg-20200806-2c35797-win64-dev\lib\avutil.lib
   LIBS += $$GF\ExternalLibs\ffmpeg-20200806-2c35797-win64-dev\lib\swscale.lib
   SOURCES += $$GF/GameEngine/GenericDevice/GenericDevice.cpp
   SOURCES += $$GF/GameEngine/GenericDevice/GenericInput.cpp
}

#
# plug and paint tools


#
# Boarder main window

INCLUDEPATH += $$GF/Applications/CommonQt
SOURCES += $$GF/Applications/CommonQt/QtUtils.cpp

QT += opengl
QT += widgets
CONFIG += qt thread release

# Input
FORMS += ../BoarderMainWindow.ui ../ShotPanelWidget.ui
HEADERS += ../MainWindow.h ../ShotPanelWidget.h
SOURCES += ../main.cpp ../MainWindow.cpp ../ShotPanelWidget.cpp


#
# plug and paint tools
HEADERS       += $$GF/Applications/plugandpaint/app/interfaces.h \
                 $$GF/Applications/plugandpaint/app/mainwindowpaint.h \
                 $$GF/Applications/plugandpaint/app/paintarea.h \
                 $$GF/Applications/plugandpaint/app/plugindialog.h
				 
SOURCES       += $$GF/Applications/plugandpaint/app/mainwindowpaint.cpp \
                 $$GF/Applications/plugandpaint/app/paintarea.cpp \
                 $$GF/Applications/plugandpaint/app/plugindialog.cpp \
				 $$GF/GameEngine/Demos/Draw/FitCurves.c \
				 $$GF/GameEngine/Demos/Draw/GraphicsGems.c
INCLUDEPATH   += $$GF/Applications/plugandpaint/app
LIBS          += -L$$GF/Applications/plugandpaint/plugins
macx-xcode {
    LIBS += -lpnp_basictools$($${QMAKE_XCODE_LIBRARY_SUFFIX_SETTING})
} else {
	CONFIG(debug, debug|release) {
    	#LIBS += -lpnp_basictoolsd
	}else{
		#LIBS += -lpnp_basictools
	}
	if(!debug_and_release|build_pass):CONFIG(debug, debug|release) {
        mac:LIBS = $$member(LIBS, 0) $$member(LIBS, 1)_debug
        ##### win32:LIBS = $$member(LIBS, 0) $$member(LIBS, 1)d
    }
}
