EXE = demo
SRC = $(addsuffix .cpp, $(EXE))
# Ensure this is compiled with the exact same flags found in CXXFLAGS!
# https://stackoverflow.com/a/2935536
PCH = olcPixelGameEngine.hpp.gch
# Also use ALL OF THE SAME preprocessing options for the PCH as well.
# This includes the `-I`, `-D` and `-U` flags. Otherwise, won't work! 
# https://stackoverflow.com/a/12438040
CXXFLAGS := $(CXXFLAGS:-Werror=) -I. -Winvalid-pch -DUNICODE -D_UNICODE -DOLC_PGE_APPLICATION

# Dependencies of PixelGameEngine
LDLIBS = user32 gdi32 opengl32 gdiplus Shlwapi dwmapi stdc++fs

all: $(PCH) $(EXE)

%.hpp.gch: %.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# Executable may have multiple objects
# Use -H to dump all include paths, useful to test if PCH is working
$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) -H -o $@ $^ $(addprefix -l, $(LDLIBS))

clean:
	$(RM) $(EXE) $(OBJ)

# PCH is huge so only remove it if you absolutely want to
uninstall: clean
	$(RM) $(PCH)

.PHONY: all clean uninstall
