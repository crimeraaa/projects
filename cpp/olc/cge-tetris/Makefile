EXE := tetris
SRC := $(addsuffix .cpp, $(EXE))
# CXX := clang++
# CXXFLAGS := -std=c++17 -Wall -Wextra -Wshadow -pedantic \
# -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable

# C-Preprocessor macro defines and include path resolution
CPPFLAGS := -DUNICODE -D_UNICODE -I..

# -fsanitize=address requires libasan
# MinGW doesn't support this, use clang++, 
# In turn, MSYS clang doesn't support -fsanitize=leak for windows-gnu targets

# bruh: https://stackoverflow.com/a/75702513
# typical Windows L
# 
# To use fsanitize=address, need MSYS2 Clang64 environment shell w/ clang.exe!
# Can use UBsan (undefined behaviour sanitizer): https://stackoverflow.com/a/59083808
# -fsanitize=undefined -fsanitize-undefined-trap-on-error
# 
# HOWEVER, you can only run the executable inside of the Clang64 environment!
# DEBUGFLAGS := -Os 

all: $(EXE)

# CGE requires winmm.lib and user32.lib
$(EXE): $(SRC)
	$(CXX) -fdiagnostics-color=always -g $(CXXFLAGS:-Werror=) $(CPPFLAGS) -o $@ $< -lwinmm -luser32

clean:
	$(RM) $(EXE)
