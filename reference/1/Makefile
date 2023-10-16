CFLAGS = -std=c++17

LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

BUILD_PATH = ./build

build-debug: HelloTriangle.cpp
	g++ $(CFLAGS) -g -o ${BUILD_PATH}/build-debug HelloTriangle.cpp $(LDFLAGS)

build-release: HelloTriangle.cpp
	g++ $(CFLAGS) -O2 -o ${BUILD_PATH}/build-release HelloTriangle.cpp $(LDFLAGS)

.PHONY: test clean

run-debug: build-debug
		./build/build-debug

run-release: build-release
		./build/build-release

clean:
	rm -f ./build