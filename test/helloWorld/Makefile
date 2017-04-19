
EXEC=test

make: $(EXEC)

$(EXEC): Makefile main.cpp
	g++ main.cpp -static -o $(EXEC)

clean:
	rm $EXEC
