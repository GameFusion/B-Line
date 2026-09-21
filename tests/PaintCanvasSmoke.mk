# Run from build-vs2019-qt6/build/<configuration> after building Boarder.
include Makefile
PaintCanvasSmoke.o: ../../../tests/PaintCanvasSmoke.cpp ../../../PaintCanvas.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<
PaintCanvasSmoke: PaintCanvasSmoke.o $(filter-out main.o,$(OBJECTS))
	$(LINK) $(LFLAGS) -o $@ $^ $(LIBS)
