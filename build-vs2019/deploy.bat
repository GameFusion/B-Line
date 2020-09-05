mkdir distrib
copy release\Boarder.exe distrib
cd distrib
windeployqt Boarder.exe
cd ..
copy ..\*.svg distrib
robocopy distrib ../../../Programmes/Boarder /e
