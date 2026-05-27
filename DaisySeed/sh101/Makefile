# SH-101 hybrid synth — Daisy Seed

TARGET = sh101

CPP_SOURCES = sh101.cpp \
              src/Voice.cpp \
              src/Lfo.cpp \
              src/Controls.cpp

# Clone libDaisy and DaisySP at the repo root (or adjust these paths):
#   git clone https://github.com/electro-smith/libDaisy
#   git clone https://github.com/electro-smith/DaisySP
# then build each library once with `make` before building this project.
LIBDAISY_DIR ?= ../../libDaisy
DAISYSP_DIR  ?= ../../DaisySP

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
