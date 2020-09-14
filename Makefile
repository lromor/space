CXX=g++

CFLAGS=-g -O0 -Wall -DVK_USE_PLATFORM_XLIB_KHR -DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 -std=c++17
CFLAGS+=-I/usr/include/libevdev-1.0
LD_FLAGS=-lvulkan -lX11 -lXi -levdev -ldl
STATIC_LIBS=input/libspaceinput.a

OBJECTS=vulkan-core.o vulkan-rendering.o scene.o \
	vulkan-pipeline.o reference-grid.o curve.o camera.o interface-manager.o
MAIN_OBJECTS=space.o

DEPENDENCY_RULES=$(OBJECTS:=.d) $(MAIN_OBJECTS:=.d)

all: space

input/libspaceinput.a: FORCE
	$(MAKE) -C input

space: space.o $(OBJECTS) $(STATIC_LIBS)
	$(CXX) -o $@ $^ $(STATIC_LIBS) $(LD_FLAGS)

%.o: %.cc shaders
	$(CXX) $(CFLAGS) -c $< -o $@
	@$(CXX) $(CFLAGS) -MM $< > $@.d

-include $(DEPENDENCY_RULES)

shaders: FORCE
	$(MAKE) -C shaders/

clean:
	rm -rf *.o *.d space

.PHONY: all FORCE
