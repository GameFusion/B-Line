# Run from build-vs2019-qt6/build/<configuration> after building Boarder.
include Makefile
SMOKE_OBJECTS = PaintCanvasSmoke.o PlaybackSmoke.o MoviePlayerSmoke.o ExportMovieSmoke.o StrokeLatency.o ProjectPlaybackBenchmark.o TimelinePaintSmoke.o StrokePreviewSmoke.o
$(SMOKE_OBJECTS): ../../../tests/PaintCanvasSmoke.mk
-include $(SMOKE_OBJECTS:.o=.d)
StrokePreviewSmoke.o: ../../../tests/StrokePreviewSmoke.cpp
	$(CXX) -c -MMD -MP $(CXXFLAGS) $(INCPATH) -o $@ $<
StrokePreviewSmoke: StrokePreviewSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
TimelinePaintSmoke.o: ../../../tests/TimelinePaintSmoke.cpp
	$(CXX) -c -MMD -MP $(CXXFLAGS) $(INCPATH) -o $@ $<
TimelinePaintSmoke: TimelinePaintSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
ProjectPlaybackBenchmark.o: ../../../tests/ProjectPlaybackBenchmark.cpp ../../../MainWindow.h
	$(CXX) -c -MMD -MP $(CXXFLAGS) $(INCPATH) -o $@ $<
ProjectPlaybackBenchmark: ProjectPlaybackBenchmark.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
StrokeLatency.o: ../../../tests/StrokeLatency.cpp ../../../PaintCanvas.h
	$(CXX) -c -MMD -MP $(CXXFLAGS) $(INCPATH) -o $@ $<
StrokeLatency: StrokeLatency.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
PaintCanvasSmoke.o: ../../../tests/PaintCanvasSmoke.cpp ../../../PaintCanvas.h ../../../PlaybackTiming.h
	$(CXX) -c -MMD -MP $(CXXFLAGS) $(INCPATH) -o $@ $<
PaintCanvasSmoke: PaintCanvasSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
PlaybackSmoke.o: ../../../tests/PlaybackSmoke.cpp ../../../MainWindow.h ../../../PlaybackTiming.h
	$(CXX) -c -MMD -MP $(CXXFLAGS) $(INCPATH) -o $@ $<
PlaybackSmoke: PlaybackSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
MoviePlayerSmoke.o: ../../../tests/MoviePlayerSmoke.cpp ../../../MoviePlayerWindow.h
	$(CXX) -c -MMD -MP $(CXXFLAGS) $(INCPATH) -o $@ $<
MoviePlayerSmoke: MoviePlayerSmoke.o MoviePlayerWindow.o
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
ExportMovieSmoke.o: ../../../tests/ExportMovieSmoke.cpp ../../../MainWindow.h
	$(CXX) -c -MMD -MP $(CXXFLAGS) $(INCPATH) -o $@ $<
ExportMovieSmoke: ExportMovieSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
