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

COMMON_CXX    := src/platform/gpio.cpp src/platform/clock.cpp \
                 src/platform/fault.cpp src/platform/iap_flag.cpp
APP_CXX       := src/main.cpp src/platform/systick.cpp \
                 src/platform/flash.cpp src/boot/iap_update.cpp \
                 src/hal/led.cpp \
                 src/usb/ch341.cpp \
                 src/radio/rfip.cpp src/radio/rf_uart.cpp src/radio/rfend.cpp
IAP_CXX       := src/boot/iap_main.cpp src/boot/iap_update.cpp \
                 src/usb/ch341.cpp src/platform/systick.cpp

LDSCRIPT      := ld/ch572.ld
LD_DEFS_APP   := -Wl,--defsym=FLASH_ORIGIN=0x00002000 -Wl,--defsym=FLASH_LENGTH=0x0003A000
LD_DEFS_IAP   := -Wl,--defsym=FLASH_ORIGIN=0x00000000 -Wl,--defsym=FLASH_LENGTH=0x00002000
LIBS          := -lm

ifdef IAP_BOOT
CXX_SOURCES   := $(COMMON_CXX) $(IAP_CXX) src/platform/flash.cpp
LD_DEFS       := $(LD_DEFS_IAP)
else
CXX_SOURCES   := $(COMMON_CXX) $(APP_CXX)
LD_DEFS       := $(LD_DEFS_APP)
endif

ASM_SOURCES   := src/boot/startup_ch572.S
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

.PHONY: all build clean help iap_app iap_boot

# `make` (default) builds the application; `make iap_boot` builds the bootloader.
# Each driver target re-invokes make with the flags its build needs and runs the
# `build` rule, so the default target never recurses into itself.
all: iap_app

iap_app:
	$(MAKE) BUILD=build/app TARGET=rf_uart_app build

iap_boot:
	$(MAKE) BUILD=build/boot TARGET=rf_uart_iap IAP_BOOT=1 build

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
	@echo "make             build the application (same as make iap_app)"
	@echo "make iap_app     application image for the USB IAP bootloader"
	@echo "make iap_boot    release bootloader: auto-jumps to the app unless the app requested IAP"
	@echo "make clean       remove build outputs"

-include $(DEPS)
