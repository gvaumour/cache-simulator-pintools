EXEC = ./obj-intel64/roeval.so
EXEC_RELEASE = ./obj-intel64/roeval_release.so

OUTPUT_FILES=config.ini *.out pin.log
FLAGS_DEBUGS = 
ifdef TEST
	FLAGS_DEBUGS = -DTEST
endif 

PIN_ROOT=../pin-3.6-97554-g31f0a167d-gcc-linux/

CPP   = g++
FLAGS = -Wall -DBIGARRAY_MULTIPLIER=1 -Wno-unknown-pragmas -D__PIN__=1 -DPIN_CRT=1 -fno-stack-protector -fno-exceptions -funwind-tables -fasynchronous-unwind-tables -fno-rtti -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX -fabi-version=2  -I$(PIN_ROOT)source/include/pin -I$(PIN_ROOT)source/include/pin/gen -isystem $(PIN_ROOT)extras/stlport/include -isystem $(PIN_ROOT)extras/libstdc++/include -isystem $(PIN_ROOT)extras/crt/include -isystem $(PIN_ROOT)extras/crt/include/arch-x86_64 -isystem $(PIN_ROOT)extras/crt/include/kernel/uapi -isystem $(PIN_ROOT)extras/crt/include/kernel/uapi/asm-x86 -I$(PIN_ROOT)extras/components/include -I$(PIN_ROOT)extras/xed-intel64/include/xed -I$(PIN_ROOT)source/tools/InstLib -O3 -fomit-frame-pointer -fno-strict-aliasing -lz -std=c++11 -I./src/ 

LDFLAGS=-g -Wall -shared -Wl,--hash-style=sysv $(PIN_ROOT)intel64/runtime/pincrt/crtbeginS.o -Wl,-Bsymbolic -Wl,--version-script=$(PIN_ROOT)source/include/pin/pintool.ver -fabi-version=2 -L$(PIN_ROOT)intel64/runtime/pincrt -L$(PIN_ROOT)intel64/lib -L./bin/ -L$(PIN_ROOT)intel64/lib-ext -L$(PIN_ROOT)extras/xed-intel64/lib -lpin -lxed $(PIN_ROOT)intel64/runtime/pincrt/crtendS.o -lpin3dwarf  -ldl-dynamic -nostdlib -lstlport-dynamic -lm-dynamic -lc-dynamic -lunwind-dynamic

SRC= $(wildcard src/*.cc)
OBJ= $(subst src/, obj-intel64/,  $(SRC:.cc=.o))

$(EXEC) : $(OBJ) Makefile 
	$(CPP) -o $(EXEC) $(OBJ) $(LDFLAGS) $(FLAGS_DEBUGS)

$(EXEC_RELEASE) : $(EXEC)	
	cp $(EXEC) $(EXEC_RELEASE)

$(EXEC_RELEASE1) : $(EXEC)	
	cp $(EXEC) $(EXEC_RELEASE1)


obj-intel64/%.o :  src/%.cc src/%.hh Makefile
	$(CPP) $(FLAGS) $(FLAGS_DEBUGS) -c $< -o $@

clean:
	rm -f $(EXEC) $(OUTPUT_FILES) *~ src/*~ obj-intel64/*.o 

#debug: $(EXEC)
	

release: $(EXEC_RELEASE)

all : $(EXEC)

