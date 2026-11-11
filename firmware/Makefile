# CH570/CH572 RF UART dongle

MRS_TOOLCHAIN ?= C:/MounRiver/MounRiver_Studio3/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC15/bin
PREFIX        := $(MRS_TOOLCHAIN)/riscv32-wch-elf-
CXX           := "$(PREFIX)g++.exe"
OBJCOPY       := "$(PREFIX)objcopy.exe"
SIZE          := "$(PREFIX)size.exe"

TARGET        := rf_uart_app
BUILD         := build

ARCH          := -march=rv32imcb -mabi=ilp32 -mcmodel=medany
CXXFLAGS      := $(ARCH) -Os -std=c++17 \
                 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
                 -fno-use-cxa-atexit -ffunction-sections -fdata-sections \
                 -fno-common -fsigned-char -fmessage-length=0 \
                 -Wall -Wextra
ASFLAGS       := $(ARCH) -x assembler-with-cpp
INCLUDES      := -Iinclude

COMMON_CXX    := src/platform/clock.cpp src/platform/pins.cpp
APP_CXX       := src/main.cpp \
                 src/usb/usb_device.cpp \
                 src/radio/rfip.cpp src/radio/rf_uart.cpp src/radio/rfend.cpp

LDSCRIPT      := ld/ch572.ld
LD_DEFS       := -Wl,--defsym=FLASH_ORIGIN=0x00000000 -Wl,--defsym=FLASH_LENGTH=0x0003C000
LIBS          := -lm

CXX_SOURCES   := $(COMMON_CXX) $(APP_CXX)
ASM_SOURCES   := src/platform/startup_ch572.S
OBJECTS       := $(patsubst %.cpp,$(BUILD)/%.o,$(CXX_SOURCES)) \
                 $(patsubst %.S,$(BUILD)/%.o,$(ASM_SOURCES))
DEPS          := $(OBJECTS:.o=.d)

LDFLAGS       := $(ARCH) -T$(LDSCRIPT) $(LD_DEFS) -nostartfiles \
                 -Wl,--gc-sections -Wl,--print-memory-usage \
                 -Wl,-Map=$(BUILD)/$(TARGET).map \
                 --specs=nano.specs --specs=nosys.specs

ifeq ($(OS),Windows_NT)
SHELL         := cmd.exe
MKDIR_P        = if not exist "$(subst /,\,$(patsubst %/,%,$(1)))" mkdir "$(subst /,\,$(patsubst %/,%,$(1)))"
RM_RF          = if exist "$(subst /,\,$(1))" rmdir /s /q "$(subst /,\,$(1))"
else
MKDIR_P        = mkdir -p "$(1)"
RM_RF          = rm -rf "$(1)"
endif

.PHONY: all build clean help

all: build

build: $(BUILD)/$(TARGET).hex

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	$(SIZE) $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
	$(OBJCOPY) -O binary $< $(BUILD)/$(TARGET).bin
	@echo "built $@"

$(BUILD)/%.o: %.cpp
	@$(call MKDIR_P,$(dir $@))
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@$(call MKDIR_P,$(dir $@))
	$(CXX) $(ASFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@$(call RM_RF,build)

help:
	@echo "make             build the application image at 0x00000000"
	@echo "make clean       remove build outputs"

-include $(DEPS)
