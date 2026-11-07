# Include from core/makefile after OBJ_DIR / INCLUDES / COMPILER_OPTIONS are set.
# Pass FEATURE_MODULE_DIRS as a space-separated list of absolute or relative paths.
#
# Each module directory must contain module.mk:
#   MODULE_NAME     := performance-fx-scenes
#   MODULE_INSTANCE := performanceFxScenes
#   MODULE_HEADER   := PerformanceFxScenes.h
#   MODULE_SRCS     := PerformanceFxScenes.cpp   # optional; defaults to *.cpp

FEATURE_MODULE_DIRS ?=

FEATURE_MODULE_OBJS :=
GENERATED_FEATURE_MODULE_REGISTRY := $(BUILD_DIR)/feature_modules_registry.cpp
GENERATED_FEATURE_MODULE_REGISTRY_OBJ := $(OBJ_DIR)/feature_modules_registry.o

define _feature_module_add_src
FEATURE_MODULE_OBJS += $(OBJ_DIR)/ext/$(1)/$(basename $(2)).o
$(OBJ_DIR)/ext/$(1)/$(basename $(2)).o: $(3)/$(2)
	@mkdir -p $$(dir $$@)
	$$(TOOLCHAIN)c++ $$(DEFINES) $$(INCLUDES) $$(CPP_OPTIONS) $$(COMPILER_OPTIONS) -o $$@ $$<
endef

define _feature_module_load
MODULE_NAME :=
MODULE_INSTANCE :=
MODULE_HEADER :=
MODULE_SRCS :=
_fm_dir := $(abspath $(1))
include $$(_fm_dir)/module.mk
INCLUDES += -I$$(_fm_dir)
_fm_srcs := $$(MODULE_SRCS)
ifeq ($$(_fm_srcs),)
_fm_srcs := $$(notdir $$(wildcard $$(_fm_dir)/*.cpp))
endif
$$(foreach _src,$$(_fm_srcs),$$(eval $$(call _feature_module_add_src,$$(MODULE_NAME),$$(_src),$$(_fm_dir))))
endef

ifneq ($(strip $(FEATURE_MODULE_DIRS)),)
$(foreach _d,$(FEATURE_MODULE_DIRS),$(eval $(call _feature_module_load,$(_d))))
endif

$(GENERATED_FEATURE_MODULE_REGISTRY): modules/generate_registry.py \
		$(foreach _d,$(FEATURE_MODULE_DIRS),$(abspath $(_d))/module.mk)
	@mkdir -p $(dir $@)
	python3 modules/generate_registry.py -o $@ $(foreach _d,$(FEATURE_MODULE_DIRS),$(abspath $(_d)))

$(GENERATED_FEATURE_MODULE_REGISTRY_OBJ): $(GENERATED_FEATURE_MODULE_REGISTRY)
	@mkdir -p $(dir $@)
	$(TOOLCHAIN)c++ $(DEFINES) $(INCLUDES) $(CPP_OPTIONS) $(COMPILER_OPTIONS) -o "$@" "$<"
