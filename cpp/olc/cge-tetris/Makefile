EXE = tetris
SRC = $(addsuffix .cpp, $(EXE))
CXXFLAGS := $(CXXFLAGS:-Werror=) -Wno-unused-but-set-variable -DUNICODE -D_UNICODE -I..

all: $(EXE)

# CGE requires winmm.lib and user32.lib
$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< -lwinmm -luser32

clean:
	$(RM) $(EXE)
