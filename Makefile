.PHONY: all clean rebuild

all:
	mkdir -p build
	cd build && cmake .. && cmake --build .

clean:
	rm -rf build

rebuild: clean all
