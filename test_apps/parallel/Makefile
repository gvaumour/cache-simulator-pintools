
EXEC=test

make: $(EXEC)

$(EXEC): Makefile main.cpp
	g++ main.cpp -o $(EXEC) -fopenmp -Wall

clean:
	rm $EXEC
