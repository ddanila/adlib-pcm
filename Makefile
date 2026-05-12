# adlib-pcm — build a 16-bit DOS .EXE with OpenWatcom v2 and run it
# under DOSBox-Staging (NukedOPL). Plays Windows 3.x PCM WAVs through
# an AdLib (OPL2) by amplitude-modulating a sustained tone whose
# carrier is phase-locked to the chip's internal 49716 Hz DAC rate.

WATCOM_DIR ?= vendor/openwatcom-v2/current-build-2026-04-20
HOST_OS    := $(shell uname -s)
HOST_ARCH  := $(shell uname -m)
ifeq ($(HOST_OS),Darwin)
  ifeq ($(HOST_ARCH),arm64)
    WATCOM_BIN := $(WATCOM_DIR)/macos-arm64
  else
    WATCOM_BIN := $(WATCOM_DIR)/macos-x64
  endif
else
  WATCOM_BIN := $(WATCOM_DIR)/linux-amd64
endif

WCC        = $(WATCOM_BIN)/wcc
WLINK      = $(WATCOM_BIN)/wlink
WATCOM_H   = $(WATCOM_DIR)/h
WATCOM_LIB = $(WATCOM_DIR)/lib286/dos

# -0    = 8086 instruction set (universal)
# -ms   = small memory model (64K code + 64K data)
# -os   = optimize for size
# -s    = no stack overflow checks
# -za99 = C99 mode (mixed declarations, // comments)
# -w4 -we = max warnings, treat as errors
# -oi   = inline intrinsics
WCCFLAGS = -0 -ms -os -s -za99 -w4 -we -oi -i=$(WATCOM_H)

SRC     = src/main.c src/opl2.c src/timer.c src/pcm.c src/display.c \
          src/player_pcm.c
OBJ     = $(SRC:src/%.c=build/%.obj)
HEADERS = $(wildcard src/*.h)
EXE     = build/adlib.exe

# Windows 3.x sound clips live in the all-windows-sounds git submodule.
# Source WAVs -> raw PCM at the player's sample rate via the transcoder
# script. The submodule path "(1992) Windows 3x" contains spaces and
# parens that confuse Make's pattern matcher, so we stage symlinks into
# build/wavs/ and key the pattern rule off there. Each .RAW lands on
# the staged C: drive with an 8.3 uppercase filename so DOS picks them
# up by the compile-time names baked into player_pcm.c.
WIN3X_DIR    = sources/all-windows-sounds/(1992) Windows 3x
WAV_STAGE    = build/wavs
WAV_NAMES    = DING CHORD TADA CHIMES RINGIN RINGOUT
WAV_STAGED   = $(WAV_NAMES:%=$(WAV_STAGE)/%.WAV)
RAW_FILES    = $(WAV_NAMES:%=assets/%.RAW)

.PHONY: all clean run raws refresh-watcom help

all: $(EXE)

help:
	@echo "adlib-pcm targets:"
	@echo "  all             build $(EXE) (default)"
	@echo "  raws            transcode WAVs from the submodule to assets/*.RAW"
	@echo "  run             run $(EXE) under DOSBox-Staging (NukedOPL)"
	@echo "  refresh-watcom  vendor a fresh Open Watcom v2 snapshot"
	@echo "  clean           remove build/"

refresh-watcom:
	bash scripts/vendor_openwatcom.sh

build:
	@mkdir -p build

assets:
	@mkdir -p assets

# Conservative: rebuild every TU when any header changes. With ~6 TUs
# and tiny compile times this is fine.
build/%.obj: src/%.c $(HEADERS) | build
	$(WCC) $(WCCFLAGS) -fo=$@ $<

$(EXE): $(OBJ)
	$(WLINK) name $@ format dos $(addprefix file ,$(OBJ)) \
	  libpath $(WATCOM_LIB) library clibs.lib

$(WAV_STAGE):
	@mkdir -p $(WAV_STAGE)

# Stage symlinks of the submodule WAVs into a no-space directory.
# Note: $(abspath ...) tokenises on whitespace and would corrupt the
# "(1992) Windows 3x" path, so build the absolute target from $(CURDIR).
$(WAV_STAGE)/%.WAV: | $(WAV_STAGE)
	@ln -sf "$(CURDIR)/$(WIN3X_DIR)/$*.WAV" "$@"

# WAV -> RAW. The transcoder normalizes amplitude so quieter clips
# use the full 6-bit dynamic range of OPL TL writes.
assets/%.RAW: $(WAV_STAGE)/%.WAV scripts/transcode_wav.py | assets
	python3 scripts/transcode_wav.py "$<" "$@"

raws: $(RAW_FILES)

# DOSBox-Staging ships NukedOPL by default, which is cycle-accurate
# enough for the PCM-via-TL-modulation trick to actually produce audio.
# scripts/run-dosbox.sh stages build/ + .RAW files into a directory
# mounted as C: and runs ADLIB.EXE.
run: $(EXE) $(RAW_FILES)
	bash scripts/run-dosbox.sh

clean:
	rm -rf build
