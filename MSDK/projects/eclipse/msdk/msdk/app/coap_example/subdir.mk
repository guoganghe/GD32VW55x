################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
D:/work/103/github/GD32VW55x/MSDK/lwip/libcoap/port/client-coap.c \
D:/work/103/github/GD32VW55x/MSDK/lwip/libcoap/port/server-coap.c 

C_DEPS += \
./app/coap_example/client-coap.d \
./app/coap_example/server-coap.d 

OBJS += \
./app/coap_example/client-coap.o \
./app/coap_example/server-coap.o 


# Each subdirectory must supply rules for building sources it contributes
app/coap_example/client-coap.o: D:/work/103/github/GD32VW55x/MSDK/lwip/libcoap/port/client-coap.c app/coap_example/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GD RISC-V MCU C Compiler'
	riscv-nuclei-elf-gcc -march=rv32imafcbp -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mabi=ilp32f -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-unroll-loops -Werror -Wunused -Wuninitialized -Wall -Wno-format -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable  -g -std=c99 -DCFG_RTOS -DPLATFORM_OS_FREERTOS -I"..\..\..\..\lwip\iperf3" -I"..\..\..\..\lwip\iperf" -I"..\..\..\..\macsw\export" -I"..\..\..\..\macsw\import" -I"..\..\..\..\plf\riscv\arch" -I"..\..\..\..\plf\riscv\arch\boot" -I"..\..\..\..\plf\riscv\arch\lib" -I"..\..\..\..\plf\riscv\arch\ll" -I"..\..\..\..\plf\riscv\arch\compiler" -I"..\..\..\..\plf\src" -I"..\..\..\..\plf\src\reg" -I"..\..\..\..\plf\src\raw_flash" -I"..\..\..\..\plf\src\qspi_flash" -I"..\..\..\..\plf\src\dma" -I"..\..\..\..\plf\src\time" -I"..\..\..\..\plf\src\trng" -I"..\..\..\..\plf\src\uart" -I"..\..\..\..\plf\src\spi" -I"..\..\..\..\plf\src\spi_i2s" -I"..\..\..\..\plf\src\nvds" -I"..\..\..\..\plf\src\rf" -I"..\..\..\..\plf\riscv\gd32vw55x" -I"..\..\..\..\plf\riscv\NMSIS\Core\Include" -I"..\..\..\..\plf\riscv\NMSIS\DSP\Include" -I"..\..\..\..\plf\riscv\NMSIS\DSP\Include\dsp" -I"..\..\..\..\plf\GD32VW55x_standard_peripheral" -I"..\..\..\..\plf\GD32VW55x_standard_peripheral\Include" -I"..\..\..\..\rtos\rtos_wrapper" -I"..\..\..\..\rtos\FreeRTOS\Source\include" -I"..\..\..\..\rtos\FreeRTOS\Source\portable\riscv32" -I"..\..\..\..\rtos\FreeRTOS\config" -I"..\..\..\..\lwip\lwip-2.2.0\src\include" -I"..\..\..\..\lwip\lwip-2.2.0\src\include\compat\posix" -I"..\..\..\..\lwip\lwip-2.2.0\src\include\lwip" -I"..\..\..\..\lwip\lwip-2.2.0\src\include\lwip\apps" -I"..\..\..\..\lwip\lwip-2.2.0\port" -I"..\..\..\..\lwip\libcoap\include" -I"..\..\..\..\lwip\libcoap\port" -I"..\..\..\..\FatFS\port" -I"..\..\..\..\FatFS\src" -I"..\..\..\..\mbedtls\mbedtls-3.6.2\include" -I"..\..\..\..\mbedtls\mbedtls-3.6.2\library" -I"..\..\..\..\mbedtls\mbedtls-3.6.2\tests\include\spe" -I"..\..\..\..\..\ROM-EXPORT\bootloader" -I"..\..\..\..\..\ROM-EXPORT\halcomm" -I"..\..\..\..\..\ROM-EXPORT\symbol" -I"..\..\..\..\..\config" -I"..\..\..\..\app" -I"..\..\..\..\app\mqtt_app" -I"..\..\..\..\wifi_manager" -I"..\..\..\..\wifi_manager\wpas" -I"..\..\..\..\blesw\src\export" -I"..\..\..\..\ble\app" -I"..\..\..\..\ble\profile" -I"..\..\..\..\ble\profile\datatrans" -I"..\..\..\..\ble\profile\dis" -I"..\..\..\..\ble\profile\sample" -I"..\..\..\..\ble\profile\throughput" -I"..\..\..\..\ble\profile\bas" -I"..\..\..\..\ble\profile\ota" -I"..\..\..\..\util\include" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -Wa,-adhlns=$@.lst   -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

app/coap_example/server-coap.o: D:/work/103/github/GD32VW55x/MSDK/lwip/libcoap/port/server-coap.c app/coap_example/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GD RISC-V MCU C Compiler'
	riscv-nuclei-elf-gcc -march=rv32imafcbp -mcmodel=medlow -msmall-data-limit=8 -msave-restore -mabi=ilp32f -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -fno-unroll-loops -Werror -Wunused -Wuninitialized -Wall -Wno-format -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable  -g -std=c99 -DCFG_RTOS -DPLATFORM_OS_FREERTOS -I"..\..\..\..\lwip\iperf3" -I"..\..\..\..\lwip\iperf" -I"..\..\..\..\macsw\export" -I"..\..\..\..\macsw\import" -I"..\..\..\..\plf\riscv\arch" -I"..\..\..\..\plf\riscv\arch\boot" -I"..\..\..\..\plf\riscv\arch\lib" -I"..\..\..\..\plf\riscv\arch\ll" -I"..\..\..\..\plf\riscv\arch\compiler" -I"..\..\..\..\plf\src" -I"..\..\..\..\plf\src\reg" -I"..\..\..\..\plf\src\raw_flash" -I"..\..\..\..\plf\src\qspi_flash" -I"..\..\..\..\plf\src\dma" -I"..\..\..\..\plf\src\time" -I"..\..\..\..\plf\src\trng" -I"..\..\..\..\plf\src\uart" -I"..\..\..\..\plf\src\spi" -I"..\..\..\..\plf\src\spi_i2s" -I"..\..\..\..\plf\src\nvds" -I"..\..\..\..\plf\src\rf" -I"..\..\..\..\plf\riscv\gd32vw55x" -I"..\..\..\..\plf\riscv\NMSIS\Core\Include" -I"..\..\..\..\plf\riscv\NMSIS\DSP\Include" -I"..\..\..\..\plf\riscv\NMSIS\DSP\Include\dsp" -I"..\..\..\..\plf\GD32VW55x_standard_peripheral" -I"..\..\..\..\plf\GD32VW55x_standard_peripheral\Include" -I"..\..\..\..\rtos\rtos_wrapper" -I"..\..\..\..\rtos\FreeRTOS\Source\include" -I"..\..\..\..\rtos\FreeRTOS\Source\portable\riscv32" -I"..\..\..\..\rtos\FreeRTOS\config" -I"..\..\..\..\lwip\lwip-2.2.0\src\include" -I"..\..\..\..\lwip\lwip-2.2.0\src\include\compat\posix" -I"..\..\..\..\lwip\lwip-2.2.0\src\include\lwip" -I"..\..\..\..\lwip\lwip-2.2.0\src\include\lwip\apps" -I"..\..\..\..\lwip\lwip-2.2.0\port" -I"..\..\..\..\lwip\libcoap\include" -I"..\..\..\..\lwip\libcoap\port" -I"..\..\..\..\FatFS\port" -I"..\..\..\..\FatFS\src" -I"..\..\..\..\mbedtls\mbedtls-3.6.2\include" -I"..\..\..\..\mbedtls\mbedtls-3.6.2\library" -I"..\..\..\..\mbedtls\mbedtls-3.6.2\tests\include\spe" -I"..\..\..\..\..\ROM-EXPORT\bootloader" -I"..\..\..\..\..\ROM-EXPORT\halcomm" -I"..\..\..\..\..\ROM-EXPORT\symbol" -I"..\..\..\..\..\config" -I"..\..\..\..\app" -I"..\..\..\..\app\mqtt_app" -I"..\..\..\..\wifi_manager" -I"..\..\..\..\wifi_manager\wpas" -I"..\..\..\..\blesw\src\export" -I"..\..\..\..\ble\app" -I"..\..\..\..\ble\profile" -I"..\..\..\..\ble\profile\datatrans" -I"..\..\..\..\ble\profile\dis" -I"..\..\..\..\ble\profile\sample" -I"..\..\..\..\ble\profile\throughput" -I"..\..\..\..\ble\profile\bas" -I"..\..\..\..\ble\profile\ota" -I"..\..\..\..\util\include" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -Wa,-adhlns=$@.lst   -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-app-2f-coap_example

clean-app-2f-coap_example:
	-$(RM) ./app/coap_example/client-coap.d ./app/coap_example/client-coap.o ./app/coap_example/server-coap.d ./app/coap_example/server-coap.o

.PHONY: clean-app-2f-coap_example

