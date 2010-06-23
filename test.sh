#!/bin/bash
export DYLD_INSERT_LIBRARIES=$PWD/obj/macosx/fauxsu.dylib
exec $*
