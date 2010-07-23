CWD = $(shell pwd -P)
NODE_WAF ?= node-waf
NODE_SRC_DIR ?= $(HOME)/src/ry-node
CFLAGS ?= -g -Wall
CXXFLAGS ?= -g -Wall

# We need to build position-independent code regardless of platform
CFLAGS += -fPIC
CXXFLAGS += -fPIC

# These variables are respected by waf if we export them
export CFLAGS CXXFLAGS

.PHONY: all msgpack tags

all: msgpack

msgpack: deps/msgpack/dist/lib/libmsgpack.a
	cd src && \
		$(NODE_WAF) configure && \
		$(NODE_WAF) build

# Build the msgpack library
deps/msgpack/dist/lib/libmsgpack.a:
	cd deps/msgpack && \
		mkdir -p dist && \
		./configure --enable-static --disable-shared \
			--prefix=$(PWD)/deps/msgpack/dist && \
		make && \
		make install

tags:
	rm -f tags
	find . -name '*.cc' -or -name '*.c' -or -name '*.cpp' -or -name '*.h' -or \
		-name '*.hpp' -or -name '*.cpp' | ctags -L - -a
	test -d $(NODE_SRC_DIR) && \
		find $(NODE_SRC_DIR) -name '*.cc' -or -name '*.c' -or -name '*.h' | \
			ctags -L - -a

clean:
	cd deps/msgpack && (make distclean || true)
	rm -fr deps/msgpack/dist build
