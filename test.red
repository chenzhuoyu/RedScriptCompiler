#!/usr/bin/env redscript

native "C" class stdio(ldflags = '-lz') { #include <stdio.h> }
print(dir(stdio))
