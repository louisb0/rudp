.PHONY: all clean rebuild test examples lib

lib:
	mkdir -p build
	cd build && cmake .. && cmake --build . --target rudp

examples: lib
	cd build && cmake --build . --target rudp_server rudp_client

test: lib
	cd build && cmake --build . --target tests
	cd build && ctest --output-on-failure

all: lib examples test

clean:
	rm -rf build

rebuild: clean all
