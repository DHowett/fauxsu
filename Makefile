LIBRARY_NAME = libfauxsu
libfauxsu_FILES = fauxsu.cc
libfauxsu_INSTALL_PATH = /usr/libexec/fauxsu

#TOOL_NAME = test
test_FILES = test.c

ADDITIONAL_CFLAGS = -Wno-format -Wno-sign-compare

TARGET = native
DEBUG = 1
GO_EASY_ON_ME = 1
include /Users/dustin/Projects/iphone-framework/makefiles/common.mk
include $(FW_MAKEDIR)/library.mk
include $(FW_MAKEDIR)/tool.mk

internal-stage:: fauxsu.sh
	@mkdir -p $(FW_STAGING_DIR)/usr/bin
	@cp fauxsu.sh $(FW_STAGING_DIR)/usr/bin/fauxsu
