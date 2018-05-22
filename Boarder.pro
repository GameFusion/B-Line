
TEMPLATE = app
TARGET = Boarder

VERSION = 1.0.0
DEFINES += VERSION_STRING=\\\"$$VERSION\\\"

message(GameFusion env value set to $$(GameFusion))
GF=$$(GameFusion)
isEmpty(GF) {
	GF=../..
	message(Not found found GameFusion setting value to $$GF)
} else {
	message(Found GameFusion at $$(GameFusion))
	GF=$$(GameFusion)
}

CONFIG += no_batch

DEPENDPATH += .
INCLUDEPATH += .
INCLUDEPATH += ../../GameEngine/SoundServer ../../GameEngine/InputDevice ../../GameEngine/WindowDevice ../../GameEngine/DataStructures ../../GameEngine/GameCore ../../GameEngine/SceneGraph ../../GameEngine/Math3D ../../GameEngine/MaterialLighting ../../GameEngine/Geometry ../../GameEngine/AssetManagement ../../GameEngine/Texture ../../GameEngine/GraphicsDevice/GraphicsDeviceCore ../../GameEngine/GraphicsDevice/GraphicsDeviceOpenGL ../../GameEngine/Character ../../GameEngine/GameFramework ../../GameEngine/GUI/GUICore ../../GameEngine/GUI/GUIGame ../../GameEngine/GUI/GUIQT ../../GameEngine/Animation ../../GameEngine/GameLevel ../../GameEngine/GameThread ../../GameEngine/FontEngine
INCLUDEPATH += ../../GameEngine/LightmapFramework
INCLUDEPATH += ../../GameEngine/GamePlay
INCLUDEPATH += ../../GameEngine/HarmonicCoordinates
INCLUDEPATH += ../../GameEngine/Applications/CharacterHarmonics
INCLUDEPATH += ../../ExternalLibs/harmonicBlender
INCLUDEPATH += ../../GameEngine/GameFusion
INCLUDEPATH += ../../GameEngine/ParticleSystems
INCLUDEPATH += ../../GameEngine/Ez
INCLUDEPATH += ../../GameEngine/EzRun
INCLUDEPATH += ../../GameEngine/Collision
INCLUDEPATH += ../../GameEngine/GameWeb
INCLUDEPATH += ../../Projects/PhoneDEC/dec_phone_corr/
INCLUDEPATH += $$GF/GameEngine/Demos/Draw

RESOURCES += $$GF/Applications/CommonQt/qdarkstyle/style.qrc

macx {
	
	CONFIG(debug, debug|release) {
    		message("debug mode")
	    	LIBS += ../../GameEngine/XCode/GameFusion\\ Static\\ Library/build/Debug/libOSX\\ Library.a
	
	}else {
    		message("release mode")
    		LIBS += ../../GameEngine/XCode/GameFusion\\ Static\\ Library/build/Release/libOSX\\ Library.a
	}

	LIBS += -framework AudioToolbox -framework CoreFoundation
	LIBS += /Users/andreascarlen/ffmpeg/libavformat/libavformat.a  /Users/andreascarlen/ffmpeg/libavcodec/libavcodec.a  /Users/andreascarlen/ffmpeg/libavutil/libavutil.a   /Users/andreascarlen/ffmpeg/libswscale/libswscale.a
	LIBS += -L/Users/andreascarlen/x264 -lx264
	LIBS += ../../ExternalLibs/harmonicBlender/Xcode/Release/libharmonicBlender.a
	LIBS += -framework CoreVideo
	LIBS += -framework VideoDecodeAcceleration
	LIBS += -lbz2
	LIBS += -lz

	PRIVATE_FRAMEWORKS.files = data
	PRIVATE_FRAMEWORKS.path = Contents/Resources
	QMAKE_BUNDLE_DATA += PRIVATE_FRAMEWORKS
	
	QMAKE_CXXFLAGS += -std=c++11
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
   
   CONFIG(debug, debug|release) {
	   DEFINES += DEBUG

		LIBS	+= ..\..\GameEngine\x64\Debug\GameEngine.lib ..\..\GameEngine\x64\Debug\GameEngineGL.lib ..\..\GameEngine\x64\Debug\GameFramework.lib
		LIBS += ../../ExternalLibs/harmonicBlender/x64/Debug/harmonicBlender.lib
		LIBS += ../../ExternalLibs/libharu-2.1.0/x64/Debug/libharu.lib
		
		LIBS += comdlg32.lib
		SOURCES += ../../Projects/PhoneDEC/dec_phone_corr/Phone.cpp
		SOURCES += ../../Projects/PhoneDEC/dec_phone_corr/DEC.cpp
		 LIBS += ..\..\ExternalLibs\libjpeg-win64\x64\Debug\jpeg.lib
		LIBS += ../../ExternalLibs/libpng-1.6.24/build-win64/Debug/libpng16_staticd.lib
		LIBS += ../../ExternalLibs/zlib/Debug/zlib.lib
	} else {
		LIBS	+= ..\..\GameEngine\x64\Release\GameEngine.lib ..\..\GameEngine\x64\Release\GameEngineGL.lib ..\..\GameEngine\x64\Release\GameFramework.lib
		LIBS += ../../ExternalLibs/harmonicBlender/x64/Release/harmonicBlender.lib
		LIBS += ../../ExternalLibs/libharu-2.1.0/x64/Release/libharu.lib
		SOURCES += ../../Projects/PhoneDEC/dec_phone_corr/Phone.cpp
		SOURCES += ../../Projects/PhoneDEC/dec_phone_corr/DEC.cpp
		LIBS += ..\..\ExternalLibs\libjpeg-win64\x64\Release\jpeg.lib
		LIBS += ../../ExternalLibs/libpng-1.6.24/build-win64/Release/libpng16_static.lib
		LIBS += ../../ExternalLibs/zlib/Release/zlib.lib
		LIBS += comdlg32.lib
   }

   LIBS += ..\..\ExternalLibs\ffmpeg-git-41a097a-win64-dev\lib\avformat.lib
   LIBS += ..\..\ExternalLibs\ffmpeg-git-41a097a-win64-dev\lib\avcodec.lib
   LIBS += ..\..\ExternalLibs\ffmpeg-git-41a097a-win64-dev\lib\avutil.lib
   LIBS += ..\..\ExternalLibs\ffmpeg-git-41a097a-win64-dev\lib\swscale.lib
   SOURCES += ../../GameEngine/GenericDevice/GenericDevice.cpp
   SOURCES += ../../GameEngine/GenericDevice/GenericInput.cpp
}

#
# plug and paint tools


#
# Boarder main window

INCLUDEPATH += $$GF/Applications/CommonQt
SOURCES += $$GF/Applications/CommonQt/QtUtils.cpp

QT += opengl
CONFIG += qt thread release

# Input
FORMS += BoarderMainWindow.ui ShotPanelWidget.ui
HEADERS += MainWindow.h ShotPanelWidget.h
SOURCES += ./main.cpp MainWindow.cpp ShotPanelWidget.cpp


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
    	LIBS += -lpnp_basictoolsd
	}else{
		LIBS += -lpnp_basictoolsd
	}
	if(!debug_and_release|build_pass):CONFIG(debug, debug|release) {
        mac:LIBS = $$member(LIBS, 0) $$member(LIBS, 1)_debug
        ##### win32:LIBS = $$member(LIBS, 0) $$member(LIBS, 1)d
    }
}
