MRS_TOOLCHAIN ?= C:/MounRiver/MounRiver_Studio3/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC15/bin
PREFIX        := $(MRS_TOOLCHAIN)/riscv32-wch-elf-
CC            := "$(PREFIX)gcc.exe"
OBJCOPY       := "$(PREFIX)objcopy.exe"
SIZE          := "$(PREFIX)size.exe"

TARGET        := RF_UartDongle
BUILD         := build

ARCH          := -march=rv32imcb -mabi=ilp32 -mcmodel=medany
CFLAGS        := $(ARCH) -Os -std=gnu99 \
                 -ffunction-sections -fdata-sections -fno-common -fsigned-char \
                 -fmessage-length=0 -Wall \
                 -DDEBUG
ASFLAGS       := $(ARCH) -x assembler-with-cpp

INCLUDES      := -Iinclude \
                 -Iinclude/app \
                 -Iinclude/drivers \
                 -Iinclude/chip

LDSCRIPT      := ld/Link.ld
LDFLAGS       := $(ARCH) -T$(LDSCRIPT) -nostartfiles \
                 -Wl,--gc-sections -Wl,--print-memory-usage \
                 -Wl,-Map=$(BUILD)/$(TARGET).map \
                 --specs=nano.specs --specs=nosys.specs \
                 -Llib

LIBS          := -Wl,--start-group -lCH57xRF -lISP572 -lm -Wl,--end-group

C_SOURCES     := $(wildcard src/*.c src/app/*.c src/drivers/*.c)
ASM_SOURCES   := $(wildcard src/startup/*.S)
OBJECTS       := $(patsubst src/%.c,$(BUILD)/%.o,$(C_SOURCES)) \
                 $(patsubst src/%.S,$(BUILD)/%.o,$(ASM_SOURCES))

ifeq ($(OS),Windows_NT)
MKDIR_P       = if not exist "$(subst /,\,$(patsubst %/,%,$(1)))" mkdir "$(subst /,\,$(patsubst %/,%,$(1)))"
RM_RF         = if exist "$(subst /,\,$(1))" rmdir /s /q "$(subst /,\,$(1))"
else
MKDIR_P       = mkdir -p "$(1)"
RM_RF         = rm -rf "$(1)"
endif

.PHONY: all clean

all: $(BUILD)/$(TARGET).hex

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)
	$(SIZE) $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@
	$(OBJCOPY) -O binary $< $(BUILD)/$(TARGET).bin
	@echo "built $@"

$(BUILD)/%.o: src/%.c
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/%.o: src/%.S
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(ASFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@$(call RM_RF,$(BUILD))
