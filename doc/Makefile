#! /bin/make

all: fsm.png

# Generate fsm.png from fsm.dot using the circo tool from the graphviz package,
# if available
fsm.png: fsm.dot
	if (which circo >/dev/null); then circo -Tpng $< -o $@ ; fi

