CWD = $(shell pwd -P)
NODE_WAF ?= node-waf

.PHONY: all msgpack

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

clean:
	rm -fr deps/msgpack/dist build
