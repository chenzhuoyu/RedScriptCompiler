#!/usr/bin/env redscript

import ffi
p = ffi.int32_t(12345)
print(id(p), p)
p.value = 999
print(id(p), p)
