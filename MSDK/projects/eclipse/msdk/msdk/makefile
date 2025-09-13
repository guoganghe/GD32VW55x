################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include wifi_manager/subdir.mk
-include util/subdir.mk
-include plf/src/subdir.mk
-include plf/gd32vw55x/subdir.mk
-include plf/env/subdir.mk
-include plf/GD32VW55x_standard_peripheral/subdir.mk
-include os/freertos/subdir.mk
-include os/subdir.mk
-include mbedtls/mbedtls-3.6.2/subdir.mk
-include mbedtls/demo/subdir.mk
-include lwip/ipv6/subdir.mk
-include lwip/subdir.mk
-include coap/subdir.mk
-include ble_profile/subdir.mk
-include ble_app/subdir.mk
-include app/usr/subdir.mk
-include app/coap_example/subdir.mk
-include app/subdir.mk
-include FatFS/subdir.mk
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


BUILD_ARTIFACT_NAME := MSDK
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH_HEX += \
MSDK.hex \

SECONDARY_FLASH_BIN += \
MSDK.bin \

SECONDARY_LIST += \
MSDK.lst \

SECONDARY_SIZE += \
MSDK.siz \


# All Target
all:
	+@$(MAKE) --no-print-directory pre-build && $(MAKE) --no-print-directory main-build && $(MAKE) --no-print-directory post-build

# Main-build Target
main-build: MSDK.elf secondary-outputs

# Tool invocations
MSDK.elf: $(OBJS) $(USER_OBJS) makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Building target: $@'
	@echo 'Invoking: GD RISC-V MCU Linker'
	riscv-nuclei-elf-gcc -march=rv32imafcbp -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mabi=ilp32f -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-unroll-loops -Werror -Wunused -Wuninitialized -Wall -Wno-format -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable  -g -Wl,-Map,"MSDK.map" -T "..\..\..\..\plf\riscv\env\gd32vw55x.lds" -nostartfiles -Xlinker --gc-sections -L"..\..\..\..\lib" -L"..\..\..\..\plf\riscv\NMSIS\Library\DSP\GCC" --specs=nano.specs -u_printf_float --specs=nosys.specs -o "MSDK.elf" $(OBJS) $(USER_OBJS) $(LIBS) -lble -lrftest -lrf -lwifi -lwpas -lm -lnmsis_dsp_rv32imafcbp -liperf3
	@echo 'Finished building target: $@'
	@echo ' '

MSDK.hex: MSDK.elf makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GD RISC-V MCU Flash Image(Hex)'
	riscv-nuclei-elf-objcopy -O ihex  "MSDK.elf"  "MSDK.hex"
	@echo 'Finished building: $@'
	@echo ' '

MSDK.bin: MSDK.elf makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GD RISC-V MCU Flash Image(Bin)'
	riscv-nuclei-elf-objcopy -O binary  "MSDK.elf"  "MSDK.bin"
	@echo 'Finished building: $@'
	@echo ' '

MSDK.lst: MSDK.elf makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GD RISC-V MCU Listing'
	riscv-nuclei-elf-objdump --source --all-headers --demangle --line-numbers --wide "MSDK.elf" > "MSDK.lst"
	@echo 'Finished building: $@'
	@echo ' '

MSDK.siz: MSDK.elf makefile $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GD RISC-V MCU Print Size'
	riscv-nuclei-elf-size --format=berkeley "MSDK.elf"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) MSDK.bin MSDK.elf MSDK.hex MSDK.lst MSDK.siz
	-@echo ' '

pre-build:
	-cmd /C "..\..\..\image_prebuild.bat riscv-nuclei-elf- D:/work/103/github/GD32VW55x/MSDK/projects/eclipse/msdk/../../../ "
	-@echo ' '

post-build:
	-cmd /C "..\..\..\image_afterbuild.bat riscv-nuclei-elf-  ECDSA256 CERT D:\work\GD32EmbeddedBuilder_v1.5.2.30854\GD32EmbeddedBuilder\/../Tools/OpenOCD/xpack-openocd-0.11.0-3/bin D:/work/103/github/GD32VW55x/MSDK/projects/eclipse/msdk/../../../../ "" "
	-@echo ' '

secondary-outputs: $(SECONDARY_FLASH_HEX) $(SECONDARY_FLASH_BIN) $(SECONDARY_LIST) $(SECONDARY_SIZE)

.PHONY: all clean dependents main-build pre-build post-build

-include ../makefile.targets
