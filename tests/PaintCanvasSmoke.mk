# Run from build-vs2019-qt6/build/<configuration> after building Boarder.
include Makefile
PaintCanvasSmoke.o: ../../../tests/PaintCanvasSmoke.cpp ../../../PaintCanvas.h ../../../PlaybackTiming.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<
PaintCanvasSmoke: PaintCanvasSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
PlaybackSmoke.o: ../../../tests/PlaybackSmoke.cpp ../../../MainWindow.h ../../../PlaybackTiming.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<
PlaybackSmoke: PlaybackSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
MoviePlayerSmoke.o: ../../../tests/MoviePlayerSmoke.cpp ../../../MoviePlayerWindow.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<
MoviePlayerSmoke: MoviePlayerSmoke.o MoviePlayerWindow.o
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
ExportMovieSmoke.o: ../../../tests/ExportMovieSmoke.cpp ../../../MainWindow.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<
ExportMovieSmoke: ExportMovieSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
