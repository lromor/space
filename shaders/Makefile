
SHADERS=curve.vert.h curve.frag.h grid.vert.h grid.frag.h

all: $(SHADERS)

%.h: %
	glslangValidator -V --vn $(subst .,_,$<) $< -o $@

clean:
	rm -rf *.spv *.h
