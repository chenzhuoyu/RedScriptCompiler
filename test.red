#!/usr/bin/env redscript

import ffi
p = ffi.int32_t(12345)
print(id(p), p)
p.value = 999
print(id(p), p)
p = ffi.char_p('hello, world')
print(id(p), p)
print(dir(p))
print(p.auto_release)
