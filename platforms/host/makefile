# Host desktop emulator (PRODUCT=HOST / PLATFORM=HOST).
# Builds with TRACKER_MINI so the hold-to-show Module Menu works.

PRODUCT		?= HOST
TRACKER_DIR	?= ../../core
POLYUTIL_DIR	?= ../../util
POLYSYNTH_DIR	?= ../../synth
ROOT_DIR	?= ../..
-include ../../version.mk

BUILD_DIR	:= build
OBJ_DIR		:= $(BUILD_DIR)/obj
OUT_BIN		:= $(ROOT_DIR)/build/tracker-host
RM		:= rm -rf

HOST_CXX	?= c++
HOST_CC		?= cc

SDL_CFLAGS	:= $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS	:= $(shell sdl2-config --libs 2>/dev/null)
ifeq ($(SDL_CFLAGS),)
  $(error SDL2 not found. Install with: brew install sdl2)
endif

CPP_SRCS	:= $(shell find ./source -name '*.cpp')
_CPP_OBJS	:= $(patsubst ./%.cpp,$(OBJ_DIR)/%.o,$(CPP_SRCS))

INCLUDES	:= -I./source \
		   -I$(TRACKER_DIR)/include \
		   -I$(TRACKER_DIR)/config \
		   -I$(TRACKER_DIR)/source \
		   -I$(TRACKER_DIR)/libs \
		   -I$(TRACKER_DIR)/libs/Audio \
		   -I$(TRACKER_DIR)/libs/ElapsedMillis \
		   -I$(TRACKER_DIR)/libs/FT812 \
		   -I$(TRACKER_DIR)/libs/NewDisplay \
		   -I$(TRACKER_DIR)/interface \
		   -I$(TRACKER_DIR)/interface/core \
		   -I$(TRACKER_DIR)/AudioEngine \
		   -I$(TRACKER_DIR)/FileManager \
		   -I$(TRACKER_DIR)/effector \
		   -I$(TRACKER_DIR)/synth \
		   -I$(TRACKER_DIR)/modules \
		   -I$(POLYUTIL_DIR)/include \
		   -I$(POLYUTIL_DIR)/3rdparty \
		   -I$(POLYSYNTH_DIR)/include

DEFINES		:= -DPLATFORM_HOST -DPRODUCT_HOST -DTRACKER_MINI -DBUILD_ID=\"host-emulator\" \
		   -D__MK66FX1M0__ -DF_CPU=180000000 $(FW_VERSION_DEFINES)

CXXFLAGS	:= -std=gnu++20 -g -O2 -Wall -Wextra -fno-rtti -fno-exceptions \
		   -include cstdint $(DEFINES) $(INCLUDES) $(SDL_CFLAGS)

CORE_LIB	:= $(TRACKER_DIR)/build-HOST-HOST/libTracker.a

.DEFAULT_GOAL := build

.PHONY: build clean pre-build main-build

build: pre-build main-build

pre-build:
	@mkdir -p $(OBJ_DIR)/source $(dir $(OUT_BIN))

main-build: $(OUT_BIN)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(HOST_CXX) $(CXXFLAGS) -c -o "$@" "$<"

$(OUT_BIN): $(_CPP_OBJS) $(CORE_LIB)
	$(HOST_CXX) -o "$@" $(_CPP_OBJS) $(CORE_LIB) $(SDL_LIBS) -pthread

clean:
	-$(RM) $(BUILD_DIR)
