CXX=g++

CFLAGS=-g -O0 -Wall -DVK_USE_PLATFORM_XLIB_KHR -std=c++17
CFLAGS+=-I/usr/include/libevdev-1.0
LD_FLAGS=-lvulkan -lX11 -levdev

OBJECTS=vulkan-core.o vulkan-rendering.o simple-scene.o gamepad.o
MAIN_OBJECTS=space.o

DEPENDENCY_RULES=$(OBJECTS:=.d) $(MAIN_OBJECTS:=.d)

all: space

space: space.o $(OBJECTS)
	$(CXX) -o $@ $^ $(LD_FLAGS)


%.o: %.cc
	$(CXX) $(CFLAGS) -c $< -o $@
	@$(CXX) $(CFLAGS) -MM $< > $@.d

-include $(DEPENDENCY_RULES)

clean:
	rm -rf *.o *.d space

.PHONY: all
