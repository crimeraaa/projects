# CXXFLAGS := $(CXXFLAGS:c++11=c++14)

EXE = tetris
SRC = $(EXE).cpp

all: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	$(RM) $(EXE)

.PHONY: all clean
