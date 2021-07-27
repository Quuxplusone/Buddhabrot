all: buddha

buddha: main.cpp xoshiro256ss.h
	g++ -std=c++14 -O2 -Wall -Wextra main.cpp -o buddha

clean:
	rm -f buddha color.ppm red.pgm green.pgm blue.pgm
