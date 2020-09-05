CXX=g++

CFLAGS=-g -O0 -Wall -DVK_USE_PLATFORM_XLIB_KHR -DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 -std=c++17
CFLAGS+=-I/usr/include/libevdev-1.0
LD_FLAGS=-lvulkan -lX11 -lXi -levdev -ldl

OBJECTS=vulkan-core.o vulkan-rendering.o scene.o gamepad.o \
	vulkan-pipeline.o reference-grid.o curve.o
MAIN_OBJECTS=space.o

DEPENDENCY_RULES=$(OBJECTS:=.d) $(MAIN_OBJECTS:=.d)

all: space

space: space.o $(OBJECTS)
	$(CXX) -o $@ $^ $(LD_FLAGS)

%.o: %.cc shaders
	$(CXX) $(CFLAGS) -c $< -o $@
	@$(CXX) $(CFLAGS) -MM $< > $@.d

-include $(DEPENDENCY_RULES)

shaders: FORCE
	$(MAKE) -C shaders/

clean:
	rm -rf *.o *.d space

.PHONY: all FORCE
