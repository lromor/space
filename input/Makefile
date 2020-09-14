CXX=g++

CFLAGS=-g -O0 -Wall -std=c++17
CFLAGS+=-I/usr/include/libevdev-1.0

OBJECTS=xinput2.o gamepad.o
STATIC_LIB=libspaceinput.a

DEPENDENCY_RULES=$(OBJECTS:=.d) $(MAIN_OBJECTS:=.d)

all: $(STATIC_LIB)

$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs $@ $^

%.o: %.cc
	$(CXX) $(CFLAGS) -c $< -o $@
	@$(CXX) $(CFLAGS) -MM $< > $@.d

-include $(DEPENDENCY_RULES)

clean:
	rm -rf *.o *.d $(STATIC_LIB)

.PHONY: all FORCE
