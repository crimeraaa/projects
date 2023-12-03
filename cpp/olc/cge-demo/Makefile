EXE = player
SRC = $(addsuffix .cpp, $(EXE))
CXXFLAGS := $(CXXFLAGS:-Werror=) -DUNICODE -D_UNICODE -I..

all: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< -lwinmm -luser32

clean:
	$(RM) $(EXE)
