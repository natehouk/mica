NAME = Mica
FILES_DSP = MicaPlugin.cpp
FILES_UI  = MicaUI.cpp
DPF_PATH = ../../dpf
DPF_TARGET_DIR = ../../bin
DPF_BUILD_DIR = ../../build/$(NAME)
# the Help window's "Full manual" button uses ShellExecute on Windows
ifeq ($(WINDOWS),true)
EXTRA_UI_LIBS += -lshell32
endif

include ../../dpf/Makefile.plugins.mk

TARGETS += vst3 vst2 clap au

all: $(TARGETS)
