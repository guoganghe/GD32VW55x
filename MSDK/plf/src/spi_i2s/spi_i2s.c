/*!
    \file    spi_i2s.c
    \brief   SPI I2S for GD32VW55x SDK

    \version 2024-04-15, V1.0.0, firmware for GD32VW55x
*/

/*
    Copyright (c) 2024, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include <string.h>
#include "gd32vw55x.h"
#include "ll.h"
#include "dbg_print.h"
#include "spi_i2s.h"
#include "app_cfg.h"

#ifdef CONFIG_SPI_I2S

#if 1
#define SPI_DMA_CHNL        DMA_CH3
#define SPI_DMA_SUBPERI     DMA_SUBPERI3
#else
#define SPI_DMA_CHNL        DMA_CH4
#define SPI_DMA_SUBPERI     DMA_SUBPERI3
#endif

// is msb standard or philips type
#define I2S_MSB_STAND        0


static os_queue_t trans_q = NULL;
static pcm_buf_info_t next_pcm_buf_info;

static uint8_t buf_idx = 0;
static volatile uint8_t test_cmplt = 0;

void spi_i2s_dma_irqhandler(void)
{
    if (RESET != dma_interrupt_flag_get(SPI_DMA_CHNL, DMA_INT_FLAG_FTF)) {
        dma_interrupt_flag_clear(SPI_DMA_CHNL, DMA_INT_FLAG_FTF);
        next_pcm_buf_info.pcm_addr = buf_idx;
        if (trans_q != NULL && sys_queue_write(&trans_q, (void *)&next_pcm_buf_info, 0, true) != 0) {
            printf("spi_i2s can't write data\r\n");
        }

        if (buf_idx == 0) {
            buf_idx = 1;
        }
        else {
            buf_idx = 0;
        }
    }
}

static void spi_i2s_dma_config(uint32_t dma_addr0, uint32_t dma_addr1, uint32_t len)
{
    dma_single_data_parameter_struct dma_init_struct;
    dma_single_data_para_struct_init(&dma_init_struct);

    eclic_irq_enable(DMA_Channel3_IRQn, 10, 0);

    /* configure SPI transmit dma */
    dma_deinit(SPI_DMA_CHNL);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA;
    dma_init_struct.memory0_addr        = (uint32_t)dma_addr0;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_init_struct.priority            = DMA_PRIORITY_ULTRA_HIGH;
    dma_init_struct.number              = len;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(SPI_DMA_CHNL, &dma_init_struct);
    dma_channel_subperipheral_select(SPI_DMA_CHNL, SPI_DMA_SUBPERI);
    dma_interrupt_flag_clear(SPI_DMA_CHNL, DMA_INT_FLAG_FTF);

    dma_switch_buffer_mode_config(SPI_DMA_CHNL, (uint32_t)dma_addr1, DMA_MEMORY_0);
    dma_switch_buffer_mode_enable(SPI_DMA_CHNL);

    dma_interrupt_enable(SPI_DMA_CHNL, DMA_INT_FTF);
}

static void spi_i2s_spi_config(void)
{
    spi_parameter_struct spi_init_struct;
    /* deinitilize SPI and the parameters */
    spi_deinit();
    spi_struct_para_init(&spi_init_struct);

    /* configure SPI parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_SLAVE;
    //spi_init_struct.nss                  = SPI_NSS_HARD;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_16BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.prescale             = SPI_PSC_256;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;

    if (spi_init_struct.nss == SPI_NSS_SOFT) {
        spi_nss_internal_low();
    }
    spi_init(&spi_init_struct);
}


static void spi_i2s_gpio_config(void)
{
#if CONFIG_BOARD == PLATFORM_BOARD_32VW55X_START
    /*i2s clk Configure PA8(TIMER0 CH0 ) as alternate function*/
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_8);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_8);
    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_8);


    /*I2S WS Configure PB15(TIMER2 CH0 ) as alternate function*/
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_15);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_15);
    gpio_af_set(GPIOB, GPIO_AF_2, GPIO_PIN_15);

    /* configure SPI GPIO: SCK/PA2*/
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_2);
    gpio_af_set(GPIOA, GPIO_AF_5, GPIO_PIN_2);

    /* configure SPI GPIO: MISO/PA5*/
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_5);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_5);
    gpio_af_set(GPIOA, GPIO_AF_4, GPIO_PIN_5);

    /* configure MAX98357A: SD/PB2*/
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_2);
    gpio_bit_reset(GPIOB, GPIO_PIN_2);
#elif CONFIG_BOARD == PLATFORM_BOARD_32VW55X_EVAL
    /*i2s clk Configure PA2(TIMER0 CH0 ) as alternate function*/
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_2);
    gpio_af_set(GPIOA, GPIO_AF_6, GPIO_PIN_2);

    /*I2S WS Configure PB1(TIMER2 CH2 ) as alternate function*/
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_1);
    gpio_af_set(GPIOB, GPIO_AF_3, GPIO_PIN_1);

    /* configure SPI GPIO: SCK/PA11*/
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_11);
    gpio_af_set(GPIOA, GPIO_AF_0, GPIO_PIN_11);

    /* configure SPI GPIO: MISO/PA10*/
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_10);
    gpio_af_set(GPIOA, GPIO_AF_0, GPIO_PIN_10);

    /* configure MAX98357A: SD/PB2*/
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_MAX, GPIO_PIN_2);
    gpio_bit_reset(GPIOB, GPIO_PIN_2);
#endif
}

static void spi_i2s_clk_timer_config(uint16_t prescaler, uint32_t period, uint32_t init_cnt)
{
    timer_oc_parameter_struct timer_ocintpara;
    timer_parameter_struct timer_initpara;

    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL4);
    rcu_periph_clock_enable(RCU_TIMER0);

    timer_deinit(TIMER0);

    /* TIMER1 configuration */
    timer_initpara.prescaler         = prescaler - 1; //7
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = period - 1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER0, &timer_initpara);

    /* CH0 configuration in PWM mode */
    timer_ocintpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocintpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocintpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocintpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocintpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;

    timer_counter_value_config(TIMER0, init_cnt);

    /* CH0 configuration in PWM mode0,duty cycle 50% */
    timer_channel_output_config(TIMER0, TIMER_CH_0, &timer_ocintpara);
    timer_channel_output_pulse_value_config(TIMER0, TIMER_CH_0, period >> 1);
    timer_channel_output_mode_config(TIMER0, TIMER_CH_0, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER0, TIMER_CH_0, TIMER_OC_SHADOW_DISABLE);

    timer_primary_output_config(TIMER0, ENABLE);

    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER0);
}

static void spi_i2s_ws_timer_config(uint16_t prescaler, uint32_t period, uint32_t init_cnt)
{
    timer_oc_parameter_struct timer_ocintpara;
    timer_parameter_struct timer_initpara;

    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL4);
    rcu_periph_clock_enable(RCU_TIMER2);

    timer_deinit(TIMER2);

    /* TIMER2 configuration */
    timer_initpara.prescaler         = prescaler - 1;     // 7
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = period - 1;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;
    timer_init(TIMER2, &timer_initpara);

    /* CH0 configuration in PWM mode */
    timer_ocintpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocintpara.outputnstate = TIMER_CCXN_DISABLE;
    timer_ocintpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocintpara.ocnpolarity  = TIMER_OCN_POLARITY_HIGH;
    timer_ocintpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_ocintpara.ocnidlestate = TIMER_OCN_IDLE_STATE_LOW;


#if CONFIG_BOARD == PLATFORM_BOARD_32VW55X_START
    timer_channel_output_config(TIMER2, TIMER_CH_0, &timer_ocintpara);

    timer_counter_value_config(TIMER2, init_cnt);

    /* CH0 configuration in PWM mode0,duty cycle 50% */
    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_0, period >> 1);
    timer_channel_output_mode_config(TIMER2, TIMER_CH_0, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER2, TIMER_CH_0, TIMER_OC_SHADOW_DISABLE);
#elif CONFIG_BOARD == PLATFORM_BOARD_32VW55X_EVAL
    timer_channel_output_config(TIMER2, TIMER_CH_2, &timer_ocintpara);

    timer_counter_value_config(TIMER2, init_cnt);

    /* CH2 configuration in PWM mode0,duty cycle 50% */
    timer_channel_output_pulse_value_config(TIMER2, TIMER_CH_2, period >> 1);
    timer_channel_output_mode_config(TIMER2, TIMER_CH_2, TIMER_OC_MODE_PWM0);
    timer_channel_output_shadow_config(TIMER2, TIMER_CH_2, TIMER_OC_SHADOW_DISABLE);
#endif
    /* auto-reload preload enable */
    timer_auto_reload_shadow_enable(TIMER2);
}

void spi_i2s_start_send(os_queue_t queue, uint32_t dma_addr0, uint32_t dma_addr1, uint32_t len)
{
    trans_q = queue;

    gpio_bit_set(GPIOB, GPIO_PIN_2);
    sys_ms_sleep(200);

    sys_enter_critical();
    spi_i2s_dma_config(dma_addr0, dma_addr1, len);

    dma_channel_enable(SPI_DMA_CHNL);
    /* enable SPI DMA */
    spi_dma_enable(SPI_DMA_TRANSMIT);
    spi_enable();
    /* auto-reload preload enable */
    //TIMER_CTL0(TIMER2) |= (uint32_t)TIMER_CTL0_CEN;
    TIMER_CTL0(TIMER0) |= (uint32_t)TIMER_CTL0_CEN;
    TIMER_CTL0(TIMER2) |= (uint32_t)TIMER_CTL0_CEN;

    //timer_enable(TIMER1);
    sys_exit_critical();
}

void spi_i2s_stop_send(void)
{
    gpio_bit_reset(GPIOB, GPIO_PIN_2);

    sys_enter_critical();
    /* auto-reload preload enable */
    timer_disable(TIMER2);
    timer_disable(TIMER0);
    //timer_disable(TIMER1);
    spi_disable();
    spi_dma_disable(SPI_DMA_TRANSMIT);
    dma_channel_disable(SPI_DMA_CHNL);
    trans_q = NULL;
    sys_exit_critical();
}

void spi_i2s_init_config(void)
{
    rcu_periph_clock_enable(RCU_DMA);
    rcu_periph_clock_enable(RCU_SPI);
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

    spi_i2s_gpio_config();
    spi_i2s_spi_config();
}

uint8_t spi_i2s_init_sample_rate(uint16_t sample_rate)
{
    trans_q = NULL;
    buf_idx = 0;

    if (sample_rate == 16000) {
#if I2S_MSB_STAND
        spi_i2s_clk_timer_config(8, 39, 37);
        //i2s_ws_timer_config(8, 1248, 1231);
        spi_i2s_ws_timer_config(8, 1248, 1225);
#else
        // timer 0 first and then timer 2
        spi_i2s_clk_timer_config(8, 39, 19);
        spi_i2s_ws_timer_config(8, 1248, 624);
#endif
    }
    else if (sample_rate == 48000) {
#if I2S_MSB_STAND
        spi_i2s_clk_timer_config(8, 13, 12);
        spi_i2s_ws_timer_config(8, 416, 409);
#else
        // timer 0 first and then timer 2
        spi_i2s_clk_timer_config(8, 13, 7);
        spi_i2s_ws_timer_config(8, 416, 208);
#endif
    }
    else {
        app_print("unsupport sample rate %d\r\n", sample_rate);
        return 1;
    }

    return 0;
}
#endif
