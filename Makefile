.PHONY: all clean rebuild test

all:
	mkdir -p build
	cd build && cmake .. && cmake --build .

clean:
	rm -rf build

rebuild: clean all

test:
	cd build && ctest
