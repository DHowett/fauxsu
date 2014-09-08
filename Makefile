TARGET = native
#DEBUG = 1
GO_EASY_ON_ME = 1
include theos/makefiles/common.mk

INSTALL_PREFIX ?= /usr
LIBRARY_NAME = libfauxsu
libfauxsu_FILES = fauxsu.cc
libfauxsu_INSTALL_PATH = $(INSTALL_PREFIX)/libexec/fauxsu

#TOOL_NAME = test
test_FILES = test.c

ADDITIONAL_CFLAGS = -Wno-format -Wno-sign-compare

include $(THEOS_MAKE_PATH)/library.mk
#include $(THEOS_MAKE_PATH)/tool.mk

after-all:: $(THEOS_OBJ_DIR)/fauxsu.sh

internal-stage::
	@mkdir -p $(THEOS_STAGING_DIR)$(INSTALL_PREFIX)/bin
	@cp $(THEOS_OBJ_DIR)/fauxsu.sh $(THEOS_STAGING_DIR)$(INSTALL_PREFIX)/bin/fauxsu
	@chmod +x $(THEOS_STAGING_DIR)$(INSTALL_PREFIX)/bin/fauxsu

$(THEOS_OBJ_DIR)/fauxsu.sh: fauxsu.sh.in
	@sed -e 's|@@FAUXSU_INSTALL_PATH@@|$(libfauxsu_INSTALL_PATH)|g' $< > $@
