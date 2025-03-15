.PHONY: all clean rebuild test unit integration examples lib

lib:
	mkdir -p build
	cd build && cmake .. && cmake --build . --target rudp

examples: lib
	cd build && cmake --build . --target rudp_server rudp_client

test: lib
	cd build && cmake --build . --target tests

unit: test 
	cd build && ctest --output-on-failure -R ".*UnitTest.*"

integration: test
	cd build && ctest --output-on-failure -R ".*IntegrationTest.*"

all: lib examples test

clean:
	rm -rf build

rebuild: clean all
