################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
D:/work/103/github/GD32VW55x/MBL/mainboot/mbl.c \
D:/work/103/github/GD32VW55x/MBL/mainboot/mbl_image_validate.c 

C_DEPS += \
./mainboot/mbl.d \
./mainboot/mbl_image_validate.d 

OBJS += \
./mainboot/mbl.o \
./mainboot/mbl_image_validate.o 


# Each subdirectory must supply rules for building sources it contributes
mainboot/mbl.o: D:/work/103/github/GD32VW55x/MBL/mainboot/mbl.c mainboot/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GD RISC-V MCU C Compiler'
	riscv-nuclei-elf-gcc -march=rv32imafcbp -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mabi=ilp32f -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wuninitialized  -g -std=c99 -DEXEC_USING_STD_PRINTF -I../../../../config -I../../../../ROM-EXPORT/bootloader -I../../../../ROM-EXPORT/halcomm -I../../../../ROM-EXPORT/mbedtls-2.17.0-rom/include -I../../../../MSDK/plf/riscv/gd32vw55x -I../../../../MSDK/plf/riscv/NMSIS/Core/Include -I../../../../MSDK/plf/GD32VW55x_standard_peripheral/Include -I../../../../MSDK/plf/GD32VW55x_standard_peripheral -I../../../../MSDK/plf/src -I../../../../MSDK/macsw/export -I../../../../MSDK/app -I../../../mainboot/ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -Wa,-adhlns=$@.lst   -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

mainboot/mbl_image_validate.o: D:/work/103/github/GD32VW55x/MBL/mainboot/mbl_image_validate.c mainboot/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GD RISC-V MCU C Compiler'
	riscv-nuclei-elf-gcc -march=rv32imafcbp -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mabi=ilp32f -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wuninitialized  -g -std=c99 -DEXEC_USING_STD_PRINTF -I../../../../config -I../../../../ROM-EXPORT/bootloader -I../../../../ROM-EXPORT/halcomm -I../../../../ROM-EXPORT/mbedtls-2.17.0-rom/include -I../../../../MSDK/plf/riscv/gd32vw55x -I../../../../MSDK/plf/riscv/NMSIS/Core/Include -I../../../../MSDK/plf/GD32VW55x_standard_peripheral/Include -I../../../../MSDK/plf/GD32VW55x_standard_peripheral -I../../../../MSDK/plf/src -I../../../../MSDK/macsw/export -I../../../../MSDK/app -I../../../mainboot/ -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -Wa,-adhlns=$@.lst   -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-mainboot

clean-mainboot:
	-$(RM) ./mainboot/mbl.d ./mainboot/mbl.o ./mainboot/mbl_image_validate.d ./mainboot/mbl_image_validate.o

.PHONY: clean-mainboot

