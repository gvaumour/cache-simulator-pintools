EXEC = ./obj-intel64/roeval.so


FLAGS_DEBUGS = 
ifdef DEBUG
	FLAGS_DEBUGS = -DDEBUG
endif 

CPP   = g++
FLAGS = -DBIGARRAY_MULTIPLIER=1 -Wall -Werror -Wno-unknown-pragmas -D__PIN__=1 -DPIN_CRT=1 -fno-stack-protector -fno-exceptions -funwind-tables -fasynchronous-unwind-tables -fno-rtti -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX -fabi-version=2  -I/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/source/include/pin -I/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/source/include/pin/gen -isystem /home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/stlport/include -isystem /home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/libstdc++/include -isystem /home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/crt/include -isystem /home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/crt/include/arch-x86_64 -isystem /home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/crt/include/kernel/uapi -isystem /home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/crt/include/kernel/uapi/asm-x86 -I/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/components/include -I/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/xed-intel64/include/xed -I/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/source/tools/InstLib -O3 -fomit-frame-pointer -fno-strict-aliasing  -Wall -lz -std=c++11 -I./src/ 

LDFLAGS=-shared -Wl,--hash-style=sysv /home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/intel64/runtime/pincrt/crtbeginS.o -Wl,-Bsymbolic -Wl,--version-script=/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/source/include/pin/pintool.ver -fabi-version=2   -Wall    -L/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/intel64/runtime/pincrt -L/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/intel64/lib -L./bin/ -L/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/intel64/lib-ext -L/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/extras/xed-intel64/lib -lpin -lxed /home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/intel64/runtime/pincrt/crtendS.o -lpin3dwarf  -ldl-dynamic -nostdlib -lstlport-dynamic -lm-dynamic -lc-dynamic -lunwind-dynamic

SRC= $(wildcard src/*.cc)
OBJ= $(subst src/, obj-intel64/,  $(SRC:.cc=.o))

$(EXEC) : $(OBJ) Makefile 
	$(CPP) -o $(EXEC) $(OBJ) $(LDFLAGS) $(FLAGS_DEBUGS)

obj-intel64/%.o :  src/%.cc src/%.hh Makefile
	$(CPP) $(FLAGS) $(FLAGS_DEBUGS) -c $< -o $@
	
clean:
	rm -f obj-intel64/$(EXEC) *~ src/*~ obj-intel64/*.o pin.log

all : $(EXEC)

