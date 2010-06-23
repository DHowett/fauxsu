#!/bin/bash
export DYLD_INSERT_LIBRARIES=$PWD/obj/macosx/libfauxsu.dylib
exec $*
