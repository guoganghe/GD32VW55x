################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include platform/subdir.mk
-include mainboot/subdir.mk
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := MBL
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH_HEX += \
MBL.hex \

SECONDARY_FLASH_BIN += \
MBL.bin \

SECONDARY_LIST += \
MBL.lst \

SECONDARY_SIZE += \
MBL.siz \


# All Target
all:
	+@$(MAKE) --no-print-directory pre-build && $(MAKE) --no-print-directory main-build && $(MAKE) --no-print-directory post-build

# Main-build Target
main-build: MBL.elf secondary-outputs

# Tool invocations
MBL.elf: $(OBJS) $(USER_OBJS) makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Building target: $@'
	@echo 'Invoking: GD RISC-V MCU Linker'
	riscv-nuclei-elf-gcc -march=rv32imafcbp -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mabi=ilp32f -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wuninitialized  -g -Wl,-Map,"MBL.map" -T "..\mbl.lds" -nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs -Wl,--just-symbols=../../../../ROM-EXPORT/symbol/rom_symbol_m.gcc -o "MBL.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

MBL.hex: MBL.elf makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GD RISC-V MCU Flash Image(Hex)'
	riscv-nuclei-elf-objcopy -O ihex  "MBL.elf"  "MBL.hex"
	@echo 'Finished building: $@'
	@echo ' '

MBL.bin: MBL.elf makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GD RISC-V MCU Flash Image(Bin)'
	riscv-nuclei-elf-objcopy -O binary  "MBL.elf"  "MBL.bin"
	@echo 'Finished building: $@'
	@echo ' '

MBL.lst: MBL.elf makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GD RISC-V MCU Listing'
	riscv-nuclei-elf-objdump --source --all-headers --demangle --line-numbers --wide "MBL.elf" > "MBL.lst"
	@echo 'Finished building: $@'
	@echo ' '

MBL.siz: MBL.elf makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GD RISC-V MCU Print Size'
	riscv-nuclei-elf-size --format=berkeley "MBL.elf"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) MBL.bin MBL.elf MBL.hex MBL.lst MBL.siz
	-@echo ' '

pre-build:
	-cmd /C "..\..\mbl_prebuild.bat riscv-nuclei-elf-"
	-@echo ' '

post-build:
	-cmd /C "..\..\mbl_afterbuild.bat  riscv-nuclei-elf- D:/work/WIFI_GD32VW/Github_GD32VW55x/MBL/project/eclipse/../../../  ECDSA256 CERT D:\work\GD32EmbeddedBuilder_v1.5.2.30854\GD32EmbeddedBuilder\/../Tools/OpenOCD/xpack-openocd-0.11.0-3/bin "00ffeeddccbbaa998877665544332211" "
	-@echo ' '

secondary-outputs: $(SECONDARY_FLASH_HEX) $(SECONDARY_FLASH_BIN) $(SECONDARY_LIST) $(SECONDARY_SIZE)

.PHONY: all clean dependents main-build pre-build post-build

-include ../makefile.targets
