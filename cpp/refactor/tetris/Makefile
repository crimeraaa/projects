EXE := main
# Spaghetti but shut up
SRC := display player playingfield tetris
SRC := $(addprefix src/, $(SRC))
SRC := $(addsuffix .cpp, $(EXE) $(SRC))
OBJ := $(SRC:.cpp=.o)
# For MSVC, would need to use .pch extension
PCH := ./include/common.hpp.gch
PCH_SRC := $(PCH:.hpp.gch=.hpp)

# Use PCH by default and compile with debug information
all: CXXFLAGS := $(DEBUG_CXXFLAGS) $(CXXFLAGS) -I.
all: build

build: $(PCH) $(EXE)

debug: all

release: build

# Precompiled header is most important
# 
# Passing -H here is stupid as you dump ALL includes before use of PCH
$(PCH): $(PCH_SRC)
	$(CXX) $(CXXFLAGS) -Winvalid-pch -o $@ $<

# Executable may have multiple objects
$(EXE): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Objects need to have their respective sources.
# -H is used to check the include paths, especially for PCH's.
# if you get "! <name>.gch" and a rather short output it's working!
# Source: https://stackoverflow.com/a/2935536
# Detect changes in all project headers for now?
%.o: %.cpp *.hpp ./include/*.hpp
	$(CXX) $(CXXFLAGS) -Winvalid-pch -H -c -o $@ $<

# Remove compilation files
clean:
	$(RM) $(EXE) $(OBJ)

# PCH is huge so only remove it if you explicitly want to
uninstall: clean
	$(RM) $(PCH)

.PHONY: all build clean debug release uninstall
