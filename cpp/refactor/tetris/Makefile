EXE = tetris
SRC = $(addsuffix .cpp, $(EXE))
OBJ = $(SRC:.cpp=.o)
PCH = ./include/common.hpp.gch
PCH_SRC = $(PCH:.hpp.gch=.hpp)

# Don't use PCH by default and compile with debug information
all: CXXFLAGS := $(DEBUG_CXXFLAGS) $(CXXFLAGS)
all: build

debug: all

release: build

build: $(EXE)

use_pch: $(PCH) all

# Precompiled header is most important
$(PCH): $(PCH_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Executable may have multiple objects
# -H is used to check the include paths
$(EXE): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Objects need to have their respective sources
# -H is used to check the include paths
# Source: https://stackoverflow.com/a/2935536
# 
# the -include flag instructs the compiler to act as if lib.hpp
# were the first header in every source file
# Source: https://stackoverflow.com/a/53970380
%.o: %.cpp *.hpp ./include/*.hpp
	$(CXX) $(CXXFLAGS) -Winvalid-pch -c -o $@ $<

# Detect changes in all project headers for now?
# %.cpp: *.hpp ./include/*.hpp

clean:
	$(RM) $(EXE) $(OBJ) $(PCH)

.PHONY: all build clean debug release use_pch
