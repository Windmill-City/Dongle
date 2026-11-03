MRS_TOOLCHAIN ?= C:/MounRiver/MounRiver_Studio3/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC15/bin
PREFIX        := $(MRS_TOOLCHAIN)/riscv32-wch-elf-
CXX           := "$(PREFIX)g++.exe"
OBJCOPY       := "$(PREFIX)objcopy.exe"
SIZE          := "$(PREFIX)size.exe"

TARGET        := rf_uart_dongle
BUILD         := build

ARCH          := -march=rv32imcb -mabi=ilp32 -mcmodel=medany
CXXFLAGS      := $(ARCH) -Os -std=c++17 \
                 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
                 -fno-use-cxa-atexit -ffunction-sections -fdata-sections \
                 -fno-common -fsigned-char -fmessage-length=0 -Wall
ASFLAGS       := $(ARCH) -x assembler-with-cpp

INCLUDES      := -Iinclude \
                 -Iinclude/app \
                 -Iinclude/drivers \
                 -Iinclude/chip

LDSCRIPT      := link.ld
LDFLAGS       := $(ARCH) -T$(LDSCRIPT) -nostartfiles \
                 -Wl,--gc-sections -Wl,--print-memory-usage \
                 -Wl,-Map=$(BUILD)/$(TARGET).map \
                 --specs=nano.specs --specs=nosys.specs \
                 -Llib

LIBS          := -Wl,--start-group -lch57x_rf -lm -Wl,--end-group

CXX_SOURCES   := $(wildcard src/*.cpp src/app/*.cpp src/drivers/*.cpp)
ASM_SOURCES   := $(wildcard *.s)
OBJECTS       := $(patsubst src/%.cpp,$(BUILD)/%.o,$(CXX_SOURCES)) \
                 $(patsubst %.s,$(BUILD)/%.o,$(ASM_SOURCES))
DEPS          := $(OBJECTS:.o=.d)

ifeq ($(OS),Windows_NT)
SHELL         := cmd.exe
MKDIR_P       = if not exist "$(subst /,\,$(patsubst %/,%,$(1)))" mkdir "$(subst /,\,$(patsubst %/,%,$(1)))"
RM_RF         = if exist "$(subst /,\,$(1))" rmdir /s /q "$(subst /,\,$(1))"
else
MKDIR_P       = mkdir -p "$(1)"
RM_RF         = rm -rf "$(1)"
endif

.PHONY: all clean

all: $(BUILD)/$(TARGET).hex

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	$(SIZE) $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
	$(OBJCOPY) -O binary $< $(BUILD)/$(TARGET).bin
	@echo "built $@"

$(BUILD)/%.o: src/%.cpp
	@$(call MKDIR_P,$(dir $@))
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.s
	@$(call MKDIR_P,$(dir $@))
	$(CXX) $(ASFLAGS) $(INCLUDES) -c $< -o $@

-include $(DEPS)

clean:
	@$(call RM_RF,$(BUILD))
