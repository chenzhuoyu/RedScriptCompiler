#!/usr/bin/env redscript

native "C" class uv(ldflags = '-luv') { #include <uv.h> }
print(dir(uv))
