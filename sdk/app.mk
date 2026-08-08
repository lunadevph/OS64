SDK ?= ../sdk
BUILD ?= build
OUTPUT ?= $(BUILD)/app
SOURCES ?= app.c
CC ?= gcc
CXX ?= g++
LD ?= ld
LIBC ?= $(SDK)/../libc
LIBC_BUILD ?= $(BUILD)/libc
LIBC_ARCHIVE := $(LIBC_BUILD)/libos64c.a

__projroot := $(abspath $(CURDIR)/../../..)
__rel = $(patsubst $(__projroot)/%,%,$(1))
CFLAGS += -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -Os -Wall -Wextra -Werror -I$(SDK)/include -I$(LIBC)/include
CXXFLAGS += -std=gnu++17 -ffreestanding -nostdinc++ -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -Os -Wall -Wextra -Werror -I$(SDK)/include -I$(LIBC)/include
LDFLAGS += -nostdlib -z noexecstack -T $(SDK)/linker.ld
C_SOURCES := $(filter %.c,$(SOURCES))
CXX_SOURCES := $(filter %.cc %.cpp %.cxx,$(SOURCES))
OBJECTS := $(addprefix $(BUILD)/,$(C_SOURCES:.c=.o))
OBJECTS += $(addprefix $(BUILD)/,$(CXX_SOURCES:.cc=.o))
OBJECTS := $(OBJECTS:.cpp=.o)
OBJECTS := $(OBJECTS:.cxx=.o)
ifneq ($(strip $(CXX_SOURCES)),)
CXX_RUNTIME := $(BUILD)/os64-cxx-crt.o $(BUILD)/os64-cxx-runtime.o
endif

ifeq ($(V),1)
Q :=
else
Q := @
endif

.PHONY: all clean
all: $(OUTPUT)
$(BUILD):
	$(Q)printf "  %-7s %s\n" "MKDIR" "$(call __rel,$@)"
	$(Q)mkdir -p $@
$(BUILD)/%.o: %.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: %.cpp | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CXX" "$(call __rel,$@)"
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@
$(BUILD)/%.o: %.cc | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CXX" "$(call __rel,$@)"
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@
$(BUILD)/%.o: %.cxx | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CXX" "$(call __rel,$@)"
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@
$(BUILD)/os64-cxx-crt.o: $(SDK)/cxx/crt.cpp | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CXX" "$(call __rel,$@)"
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@
$(BUILD)/os64-cxx-runtime.o: $(SDK)/cxx/runtime.cpp | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CXX" "$(call __rel,$@)"
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@
$(LIBC_ARCHIVE):
	$(Q)$(MAKE) -C $(LIBC) BUILD=$(abspath $(LIBC_BUILD)) OUTPUT=$(abspath $@)
$(OUTPUT): $(OBJECTS) $(CXX_RUNTIME) $(LIBC_ARCHIVE)
	$(Q)printf "  %-7s %s\n" "LD" "$(call __rel,$@)"
	$(Q)$(LD) $(LDFLAGS) $(CXX_RUNTIME) $(OBJECTS) $(LIBC_ARCHIVE) -o $@
clean:
	$(Q)find $(BUILD) -depth -delete 2>/dev/null || true
