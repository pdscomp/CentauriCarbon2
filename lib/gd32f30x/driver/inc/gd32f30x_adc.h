/*!
    \file    gd32f30x_adc.h
    \brief   definitions for the ADC

    \version 2023-12-30, V2.2.0, firmware for GD32F30x
*/

/*
    Copyright (c) 2020, GigaDevice Semiconductor Inc.

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

#ifndef GD32F30X_ADC_H
#define GD32F30X_ADC_H

#include "gd32f30x.h"

/* ADC definitions */
#define ADC0                            ADC_BASE
#define ADC1                            (ADC_BASE + 0x400U)
#if (defined(GD32F30X_HD) || defined(GD32F30X_XD))
#define ADC2                            (ADC_BASE + 0x1800U)
#define ADC3_BASE             (ADC2)
#endif

/* registers definitions */
#define ADC_STAT(adcx)                  REG32((adcx) + 0x00U)            /*!< ADC status register */
#define ADC_CTL0(adcx)                  REG32((adcx) + 0x04U)            /*!< ADC control register 0 */
#define ADC_CTL1(adcx)                  REG32((adcx) + 0x08U)            /*!< ADC control register 1 */
#define ADC_SAMPT0(adcx)                REG32((adcx) + 0x0CU)            /*!< ADC sampling time register 0 */
#define ADC_SAMPT1(adcx)                REG32((adcx) + 0x10U)            /*!< ADC sampling time register 1 */
#define ADC_IOFF0(adcx)                 REG32((adcx) + 0x14U)            /*!< ADC inserted channel data offset register 0 */
#define ADC_IOFF1(adcx)                 REG32((adcx) + 0x18U)            /*!< ADC inserted channel data offset register 1 */
#define ADC_IOFF2(adcx)                 REG32((adcx) + 0x1CU)            /*!< ADC inserted channel data offset register 2 */
#define ADC_IOFF3(adcx)                 REG32((adcx) + 0x20U)            /*!< ADC inserted channel data offset register 3 */
#define ADC_WDHT(adcx)                  REG32((adcx) + 0x24U)            /*!< ADC watchdog high threshold register */
#define ADC_WDLT(adcx)                  REG32((adcx) + 0x28U)            /*!< ADC watchdog low threshold register */
#define ADC_RSQ0(adcx)                  REG32((adcx) + 0x2CU)            /*!< ADC regular sequence register 0 */
#define ADC_RSQ1(adcx)                  REG32((adcx) + 0x30U)            /*!< ADC regular sequence register 1 */
#define ADC_RSQ2(adcx)                  REG32((adcx) + 0x34U)            /*!< ADC regular sequence register 2 */
#define ADC_ISQ(adcx)                   REG32((adcx) + 0x38U)            /*!< ADC inserted sequence register */
#define ADC_IDATA0(adcx)                REG32((adcx) + 0x3CU)            /*!< ADC inserted data register 0 */
#define ADC_IDATA1(adcx)                REG32((adcx) + 0x40U)            /*!< ADC inserted data register 1 */
#define ADC_IDATA2(adcx)                REG32((adcx) + 0x44U)            /*!< ADC inserted data register 2 */
#define ADC_IDATA3(adcx)                REG32((adcx) + 0x48U)            /*!< ADC inserted data register 3 */
#define ADC_RDATA(adcx)                 REG32((adcx) + 0x4CU)            /*!< ADC regular data register */
#define ADC_OVSAMPCTL(adcx)             REG32((adcx) + 0x80U)            /*!< ADC oversampling control register */

/* bits definitions */
/* ADC_STAT */
#define ADC_STAT_WDE                    BIT(0)                           /*!< analog watchdog event flag */
#define ADC_STAT_EOC                    BIT(1)                           /*!< end of conversion */
#define ADC_STAT_EOIC                   BIT(2)                           /*!< inserted channel end of conversion */
#define ADC_STAT_STIC                   BIT(3)                           /*!< inserted channel start flag */
#define ADC_STAT_STRC                   BIT(4)                           /*!< regular channel start flag */

/* ADC_CTL0 */
#define ADC_CTL0_WDCHSEL                BITS(0,4)                        /*!< analog watchdog channel select bits */
#define ADC_CTL0_EOCIE                  BIT(5)                           /*!< interrupt enable for EOC */
#define ADC_CTL0_WDEIE                  BIT(6)                           /*!< analog watchdog interrupt enable */
#define ADC_CTL0_EOICIE                 BIT(7)                           /*!< interrupt enable for inserted channels */
#define ADC_CTL0_SM                     BIT(8)                           /*!< scan mode */
#define ADC_CTL0_WDSC                   BIT(9)                           /*!< when in scan mode, analog watchdog is effective on a single channel */
#define ADC_CTL0_ICA                    BIT(10)                          /*!< automatic inserted group conversion */
#define ADC_CTL0_DISRC                  BIT(11)                          /*!< discontinuous mode on regular channels */
#define ADC_CTL0_DISIC                  BIT(12)                          /*!< discontinuous mode on inserted channels */
#define ADC_CTL0_DISNUM                 BITS(13,15)                      /*!< discontinuous mode channel count */
#define ADC_CTL0_SYNCM                  BITS(16,19)                      /*!< sync mode selection */
#define ADC_CTL0_IWDEN                  BIT(22)                          /*!< analog watchdog enable on inserted channels */
#define ADC_CTL0_RWDEN                  BIT(23)                          /*!< analog watchdog enable on regular channels */
#define ADC_CTL0_DRES                   BITS(24,25)                      /*!< ADC data resolution */

/* ADC_CTL1 */
#define ADC_CTL1_ADCON                  BIT(0)                           /*!< ADC converter on */
#define ADC_CTL1_CTN                    BIT(1)                           /*!< continuous conversion */
#define ADC_CTL1_CLB                    BIT(2)                           /*!< ADC calibration */
#define ADC_CTL1_RSTCLB                 BIT(3)                           /*!< reset calibration */
#define ADC_CTL1_DMA                    BIT(8)                           /*!< direct memory access mode */
#define ADC_CTL1_DAL                    BIT(11)                          /*!< data alignment */
#define ADC_CTL1_ETSIC                  BITS(12,14)                      /*!< external trigger select for inserted channel */
#define ADC_CTL1_ETEIC                  BIT(15)                          /*!< external trigger enable for inserted channel */
#define ADC_CTL1_ETSRC                  BITS(17,19)                      /*!< external trigger select for regular channel */
#define ADC_CTL1_ETERC                  BIT(20)                          /*!< external trigger conversion mode for inserted channels */
#define ADC_CTL1_SWICST                 BIT(21)                          /*!< start on inserted channel */
#define ADC_CTL1_SWRCST                 BIT(22)                          /*!< start on regular channel */
#define ADC_CTL1_TSVREN                 BIT(23)                          /*!< channel 16 and 17 enable of ADC0 */

/* ADC_SAMPTx x=0..1 */
#define ADC_SAMPTX_SPTN                 BITS(0,2)                        /*!< channel x sample time selection */

/* ADC_IOFFx x=0..3 */
#define ADC_IOFFX_IOFF                  BITS(0,11)                       /*!< data offset for inserted channel x */

/* ADC_WDHT */
#define ADC_WDHT_WDHT                   BITS(0,11)                       /*!< analog watchdog high threshold */

/* ADC_WDLT */
#define ADC_WDLT_WDLT                   BITS(0,11)                       /*!< analog watchdog low threshold */

/* ADC_RSQx */
#define ADC_RSQX_RSQN                   BITS(0,4)                        /*!< x conversion in regular sequence */
#define ADC_RSQ0_RL                     BITS(20,23)                      /*!< regular channel sequence length */

/* ADC_ISQ */
#define ADC_ISQ_ISQN                    BITS(0,4)                        /*!< x conversion in regular sequence */
#define ADC_ISQ_IL                      BITS(20,21)                      /*!< inserted sequence length */

/* ADC_IDATAx x=0..3*/
#define ADC_IDATAX_IDATAN               BITS(0,15)                       /*!< inserted data x */

/* ADC_RDATA */
#define ADC_RDATA_RDATA                 BITS(0,15)                       /*!< regular data */
#define ADC_RDATA_ADC1RDTR              BITS(16,31)                      /*!< ADC1 regular channel data */

/* ADC_OVSAMPCTL */
#define ADC_OVSAMPCTL_OVSEN             BIT(0)                           /*!< oversampling enable */
#define ADC_OVSAMPCTL_OVSR              BITS(2,4)                        /*!< oversampling ratio */
#define ADC_OVSAMPCTL_OVSS              BITS(5,8)                        /*!< oversampling shift */
#define ADC_OVSAMPCTL_TOVS              BIT(9)                           /*!< triggered oversampling */
#define ADC_OVSAMPCTL_DRES              BITS(12,13)                      /*!< oversampling shift */


/* constants definitions */
/* ADC status flag */
#define ADC_FLAG_WDE                    ADC_STAT_WDE                     /*!< analog watchdog event flag */
#define ADC_FLAG_EOC                    ADC_STAT_EOC                     /*!< end of conversion */
#define ADC_FLAG_EOIC                   ADC_STAT_EOIC                    /*!< inserted channel end of conversion */
#define ADC_FLAG_STIC                   ADC_STAT_STIC                    /*!< inserted channel start flag */
#define ADC_FLAG_STRC                   ADC_STAT_STRC                    /*!< regular channel start flag */

/* adc_ctl0 register value */
#define CTL0_DISNUM(regval)             (BITS(13,15) & ((uint32_t)(regval) << 13))   /*!< write value to ADC_CTL0_DISNUM bit field */

/* ADC special function definitions */
#define ADC_SCAN_MODE                   ADC_CTL0_SM                                  /*!< scan mode */
#define ADC_INSERTED_CHANNEL_AUTO       ADC_CTL0_ICA                                 /*!< inserted channel group convert automatically */
#define ADC_CONTINUOUS_MODE             ADC_CTL1_CTN                                 /*!< continuous mode */

/* ADC synchronization mode */
#define CTL0_SYNCM(regval)              (BITS(16,19) & ((uint32_t)(regval) << 16))   /*!< write value to ADC_CTL0_SYNCM bit field */
#define ADC_MODE_FREE                                        CTL0_SYNCM(0)           /*!< all the ADCs work independently */
#define ADC_DAUL_REGULAL_PARALLEL_INSERTED_PARALLEL          CTL0_SYNCM(1)           /*!< ADC0 and ADC1 work in combined regular parallel + inserted parallel mode */
#define ADC_DAUL_REGULAL_PARALLEL_INSERTED_ROTATION          CTL0_SYNCM(2)           /*!< ADC0 and ADC1 work in combined regular parallel + trigger rotation mode */
#define ADC_DAUL_INSERTED_PARALLEL_REGULAL_FOLLOWUP_FAST     CTL0_SYNCM(3)           /*!< ADC0 and ADC1 work in combined inserted parallel + follow-up fast mode */
#define ADC_DAUL_INSERTED_PARALLEL_REGULAL_FOLLOWUP_SLOW     CTL0_SYNCM(4)           /*!< ADC0 and ADC1 work in combined inserted parallel + follow-up slow mode */
#define ADC_DAUL_INSERTED_PARALLEL                           CTL0_SYNCM(5)           /*!< ADC0 and ADC1 work in inserted parallel mode only */
#define ADC_DAUL_REGULAL_PARALLEL                            CTL0_SYNCM(6)           /*!< ADC0 and ADC1 work in regular parallel mode only */
#define ADC_DAUL_REGULAL_FOLLOWUP_FAST                       CTL0_SYNCM(7)           /*!< ADC0 and ADC1 work in follow-up fast mode only */
#define ADC_DAUL_REGULAL_FOLLOWUP_SLOW                       CTL0_SYNCM(8)           /*!< ADC0 and ADC1 work in follow-up slow mode only */
#define ADC_DAUL_INSERTED_TRRIGGER_ROTATION                  CTL0_SYNCM(9)           /*!< ADC0 and ADC1 work in trigger rotation mode only */

/* ADC data alignment */
#define ADC_DATAALIGN_RIGHT              ((uint32_t)0x00000000U)                     /*!< LSB alignment */
#define ADC_DATAALIGN_LEFT               ADC_CTL1_DAL                                /*!< MSB alignment */

/* ADC external trigger select for regular channel */
#define CTL1_ETSRC(regval)               (BITS(17,19) & ((uint32_t)(regval) << 17))  /*!< write value to ADC_CTL1_ETSRC bit field */
#define ADC0_1_EXTTRIG_REGULAR_T0_CH0    CTL1_ETSRC(0)                               /*!< timer 0 CC0 event select */
#define ADC0_1_EXTTRIG_REGULAR_T0_CH1    CTL1_ETSRC(1)                               /*!< timer 0 CC1 event select */
#define ADC0_1_EXTTRIG_REGULAR_T0_CH2    CTL1_ETSRC(2)                               /*!< timer 0 CC2 event select */
#define ADC0_1_EXTTRIG_REGULAR_T1_CH1    CTL1_ETSRC(3)                               /*!< timer 1 CC1 event select */
#define ADC0_1_EXTTRIG_REGULAR_T2_TRGO   CTL1_ETSRC(4)                               /*!< timer 2 TRGO event select */
#define ADC0_1_EXTTRIG_REGULAR_T3_CH3    CTL1_ETSRC(5)                               /*!< timer 3 CC3 event select */
#define ADC0_1_EXTTRIG_REGULAR_T7_TRGO   CTL1_ETSRC(6)                               /*!< timer 7 TRGO event select */
#define ADC0_1_EXTTRIG_REGULAR_EXTI_11   CTL1_ETSRC(6)                               /*!< external interrupt line 11 */
#define ADC0_1_2_EXTTRIG_REGULAR_NONE    CTL1_ETSRC(7)                               /*!< software trigger */

#define ADC2_EXTTRIG_REGULAR_T2_CH0      CTL1_ETSRC(0)                               /*!< timer 2 CC0 event select */
#define ADC2_EXTTRIG_REGULAR_T1_CH2      CTL1_ETSRC(1)                               /*!< timer 1 CC2 event select */
#define ADC2_EXTTRIG_REGULAR_T0_CH2      CTL1_ETSRC(2)                               /*!< timer 0 CC2 event select */
#define ADC2_EXTTRIG_REGULAR_T7_CH0      CTL1_ETSRC(3)                               /*!< timer 7 CC0 event select */
#define ADC2_EXTTRIG_REGULAR_T7_TRGO     CTL1_ETSRC(4)                               /*!< timer 7 TRGO event select */
#define ADC2_EXTTRIG_REGULAR_T4_CH0      CTL1_ETSRC(5)                               /*!< timer 4 CC0 event select */
#define ADC2_EXTTRIG_REGULAR_T4_CH2      CTL1_ETSRC(6)                               /*!< timer 4 CC2 event select */

/* ADC external trigger select for inserted channel */
#define CTL1_ETSIC(regval)               (BITS(12,14) & ((uint32_t)(regval) << 12))  /*!< write value to ADC_CTL1_ETSIC bit field */
#define ADC0_1_EXTTRIG_INSERTED_T0_TRGO  CTL1_ETSIC(0)                               /*!< timer 0 TRGO event select */
#define ADC0_1_EXTTRIG_INSERTED_T0_CH3   CTL1_ETSIC(1)                               /*!< timer 0 CC3 event select */
#define ADC0_1_EXTTRIG_INSERTED_T1_TRGO  CTL1_ETSIC(2)                               /*!< timer 1 TRGO event select */
#define ADC0_1_EXTTRIG_INSERTED_T1_CH0   CTL1_ETSIC(3)                               /*!< timer 1 CC0 event select */
#define ADC0_1_EXTTRIG_INSERTED_T2_CH3   CTL1_ETSIC(4)                               /*!< timer 2 CC3 event select */
#define ADC0_1_EXTTRIG_INSERTED_T3_TRGO  CTL1_ETSIC(5)                               /*!< timer 3 TRGO event select */
#define ADC0_1_EXTTRIG_INSERTED_EXTI_15  CTL1_ETSIC(6)                               /*!< external interrupt line 15 */
#define ADC0_1_EXTTRIG_INSERTED_T7_CH3   CTL1_ETSIC(6)                               /*!< timer 7 CC3 event select */
#define ADC0_1_2_EXTTRIG_INSERTED_NONE   CTL1_ETSIC(7)                               /*!< software trigger */

#define ADC2_EXTTRIG_INSERTED_T0_TRGO    CTL1_ETSIC(0)                               /*!< timer 0 TRGO event select */
#define ADC2_EXTTRIG_INSERTED_T0_CH3     CTL1_ETSIC(1)                               /*!< timer 0 CC3 event select */
#define ADC2_EXTTRIG_INSERTED_T3_CH2     CTL1_ETSIC(2)                               /*!< timer 3 CC2 event select */
#define ADC2_EXTTRIG_INSERTED_T7_CH1     CTL1_ETSIC(3)                               /*!< timer 7 CC1 event select */
#define ADC2_EXTTRIG_INSERTED_T7_CH3     CTL1_ETSIC(4)                               /*!< timer 7 CC3 event select */
#define ADC2_EXTTRIG_INSERTED_T4_TRGO    CTL1_ETSIC(5)                               /*!< timer 4 TRGO event select */
#define ADC2_EXTTRIG_INSERTED_T4_CH3     CTL1_ETSIC(6)                               /*!< timer 4 CC3 event select */

/* ADC channel sample time */
#define SAMPTX_SPT(regval)               (BITS(0,2) & ((uint32_t)(regval) << 0))     /*!< write value to ADC_SAMPTX_SPT bit field */
#define ADC_SAMPLETIME_1POINT5           SAMPTX_SPT(0)                               /*!< 1.5 sampling cycles */
#define ADC_SAMPLETIME_7POINT5           SAMPTX_SPT(1)                               /*!< 7.5 sampling cycles */
#define ADC_SAMPLETIME_13POINT5          SAMPTX_SPT(2)                               /*!< 13.5 sampling cycles */
#define ADC_SAMPLETIME_28POINT5          SAMPTX_SPT(3)                               /*!< 28.5 sampling cycles */
#define ADC_SAMPLETIME_41POINT5          SAMPTX_SPT(4)                               /*!< 41.5 sampling cycles */
#define ADC_SAMPLETIME_55POINT5          SAMPTX_SPT(5)                               /*!< 55.5 sampling cycles */
#define ADC_SAMPLETIME_71POINT5          SAMPTX_SPT(6)                               /*!< 71.5 sampling cycles */
#define ADC_SAMPLETIME_239POINT5         SAMPTX_SPT(7)                               /*!< 239.5 sampling cycles */

/* adc_ioffx register value */
#define IOFFX_IOFF(regval)               (BITS(0,11) & ((uint32_t)(regval) << 0))    /*!< write value to ADC_IOFFX_IOFF bit field */

/* adc_wdht register value */
#define WDHT_WDHT(regval)                (BITS(0,11) & ((uint32_t)(regval) << 0))    /*!< write value to ADC_WDHT_WDHT bit field */

/* adc_wdlt register value */
#define WDLT_WDLT(regval)                (BITS(0,11) & ((uint32_t)(regval) << 0))    /*!< write value to ADC_WDLT_WDLT bit field */

/* adc_rsqx register value */
#define RSQ0_RL(regval)                  (BITS(20,23) & ((uint32_t)(regval) << 20))  /*!< write value to ADC_RSQ0_RL bit field */

/* adc_isq register value */
#define ISQ_IL(regval)                   (BITS(20,21) & ((uint32_t)(regval) << 20))  /*!< write value to ADC_ISQ_IL bit field */

/* adc_ovsampctl register value */
/* ADC resolution */
#define OVSAMPCTL_DRES(regval)           (BITS(12,13) & ((uint32_t)(regval) << 12))  /*!< write value to ADC_OVSAMPCTL_DRES bit field */
#define ADC_RESOLUTION_12B               OVSAMPCTL_DRES(0)                           /*!< 12-bit ADC resolution */
#define ADC_RESOLUTION_10B               OVSAMPCTL_DRES(1)                           /*!< 10-bit ADC resolution */
#define ADC_RESOLUTION_8B                OVSAMPCTL_DRES(2)                           /*!< 8-bit ADC resolution */
#define ADC_RESOLUTION_6B                OVSAMPCTL_DRES(3)                           /*!< 6-bit ADC resolution */

/* oversampling shift */
#define OVSAMPCTL_OVSS(regval)           (BITS(5,8) & ((uint32_t)(regval) << 5))     /*!< write value to ADC_OVSAMPCTL_OVSS bit field */
#define ADC_OVERSAMPLING_SHIFT_NONE      OVSAMPCTL_OVSS(0)                           /*!< no oversampling shift */
#define ADC_OVERSAMPLING_SHIFT_1B        OVSAMPCTL_OVSS(1)                           /*!< 1-bit oversampling shift */
#define ADC_OVERSAMPLING_SHIFT_2B        OVSAMPCTL_OVSS(2)                           /*!< 2-bit oversampling shift */
#define ADC_OVERSAMPLING_SHIFT_3B        OVSAMPCTL_OVSS(3)                           /*!< 3-bit oversampling shift */
#define ADC_OVERSAMPLING_SHIFT_4B        OVSAMPCTL_OVSS(4)                           /*!< 4-bit oversampling shift */
#define ADC_OVERSAMPLING_SHIFT_5B        OVSAMPCTL_OVSS(5)                           /*!< 5-bit oversampling shift */
#define ADC_OVERSAMPLING_SHIFT_6B        OVSAMPCTL_OVSS(6)                           /*!< 6-bit oversampling shift */
#define ADC_OVERSAMPLING_SHIFT_7B        OVSAMPCTL_OVSS(7)                           /*!< 7-bit oversampling shift */
#define ADC_OVERSAMPLING_SHIFT_8B        OVSAMPCTL_OVSS(8)                           /*!< 8-bit oversampling shift */

/* oversampling ratio */
#define OVSAMPCTL_OVSR(regval)           (BITS(2,4) & ((uint32_t)(regval) << 2))     /*!< write value to ADC_OVSAMPCTL_OVSR bit field */
#define ADC_OVERSAMPLING_RATIO_MUL2      OVSAMPCTL_OVSR(0)                           /*!< oversampling ratio multiple 2 */
#define ADC_OVERSAMPLING_RATIO_MUL4      OVSAMPCTL_OVSR(1)                           /*!< oversampling ratio multiple 4 */
#define ADC_OVERSAMPLING_RATIO_MUL8      OVSAMPCTL_OVSR(2)                           /*!< oversampling ratio multiple 8 */
#define ADC_OVERSAMPLING_RATIO_MUL16     OVSAMPCTL_OVSR(3)                           /*!< oversampling ratio multiple 16 */
#define ADC_OVERSAMPLING_RATIO_MUL32     OVSAMPCTL_OVSR(4)                           /*!< oversampling ratio multiple 32 */
#define ADC_OVERSAMPLING_RATIO_MUL64     OVSAMPCTL_OVSR(5)                           /*!< oversampling ratio multiple 64 */
#define ADC_OVERSAMPLING_RATIO_MUL128    OVSAMPCTL_OVSR(6)                           /*!< oversampling ratio multiple 128 */
#define ADC_OVERSAMPLING_RATIO_MUL256    OVSAMPCTL_OVSR(7)                           /*!< oversampling ratio multiple 256 */

/* triggered oversampling */
#define ADC_OVERSAMPLING_ALL_CONVERT     ((uint32_t)0x00000000U)                     /*!< all oversampled conversions for a channel are done consecutively after a trigger */
#define ADC_OVERSAMPLING_ONE_CONVERT     ADC_OVSAMPCTL_TOVS                          /*!< each oversampled conversion for a channel needs a trigger */

/* ADC channel group definitions */
#define ADC_REGULAR_CHANNEL              ((uint8_t)0x01U)                            /*!< adc regular channel group */
#define ADC_INSERTED_CHANNEL             ((uint8_t)0x02U)                            /*!< adc inserted channel group */
#define ADC_REGULAR_INSERTED_CHANNEL     ((uint8_t)0x03U)                            /*!< both regular and inserted channel group */

#define ADC_CHANNEL_DISCON_DISABLE       ((uint8_t)0x04U)                            /*!< disable discontinuous mode of regular & inserted channel */

/* ADC inserted channel definitions */
#define ADC_INSERTED_CHANNEL_0           ((uint8_t)0x00U)                            /*!< adc inserted channel 0 */
#define ADC_INSERTED_CHANNEL_1           ((uint8_t)0x01U)                            /*!< adc inserted channel 1 */
#define ADC_INSERTED_CHANNEL_2           ((uint8_t)0x02U)                            /*!< adc inserted channel 2 */
#define ADC_INSERTED_CHANNEL_3           ((uint8_t)0x03U)                            /*!< adc inserted channel 3 */

/* ADC channel definitions */
#define ADC_CHANNEL_0                    ((uint8_t)0x00U)                            /*!< ADC channel 0 */
#define ADC_CHANNEL_1                    ((uint8_t)0x01U)                            /*!< ADC channel 1 */
#define ADC_CHANNEL_2                    ((uint8_t)0x02U)                            /*!< ADC channel 2 */
#define ADC_CHANNEL_3                    ((uint8_t)0x03U)                            /*!< ADC channel 3 */
#define ADC_CHANNEL_4                    ((uint8_t)0x04U)                            /*!< ADC channel 4 */
#define ADC_CHANNEL_5                    ((uint8_t)0x05U)                            /*!< ADC channel 5 */
#define ADC_CHANNEL_6                    ((uint8_t)0x06U)                            /*!< ADC channel 6 */
#define ADC_CHANNEL_7                    ((uint8_t)0x07U)                            /*!< ADC channel 7 */
#define ADC_CHANNEL_8                    ((uint8_t)0x08U)                            /*!< ADC channel 8 */
#define ADC_CHANNEL_9                    ((uint8_t)0x09U)                            /*!< ADC channel 9 */
#define ADC_CHANNEL_10                   ((uint8_t)0x0AU)                            /*!< ADC channel 10 */
#define ADC_CHANNEL_11                   ((uint8_t)0x0BU)                            /*!< ADC channel 11 */
#define ADC_CHANNEL_12                   ((uint8_t)0x0CU)                            /*!< ADC channel 12 */
#define ADC_CHANNEL_13                   ((uint8_t)0x0DU)                            /*!< ADC channel 13 */
#define ADC_CHANNEL_14                   ((uint8_t)0x0EU)                            /*!< ADC channel 14 */
#define ADC_CHANNEL_15                   ((uint8_t)0x0FU)                            /*!< ADC channel 15 */
#define ADC_CHANNEL_16                   ((uint8_t)0x10U)                            /*!< ADC channel 16 */
#define ADC_CHANNEL_17                   ((uint8_t)0x11U)                            /*!< ADC channel 17 */

/* ADC interrupt */
#define ADC_INT_WDE                      ADC_STAT_WDE                                /*!< analog watchdog event interrupt */
#define ADC_INT_EOC                      ADC_STAT_EOC                                /*!< end of group conversion interrupt */
#define ADC_INT_EOIC                     ADC_STAT_EOIC                               /*!< end of inserted group conversion interrupt */

/* ADC interrupt flag */
#define ADC_INT_FLAG_WDE                 ADC_STAT_WDE                                /*!< analog watchdog event interrupt flag */
#define ADC_INT_FLAG_EOC                 ADC_STAT_EOC                                /*!< end of group conversion interrupt flag */
#define ADC_INT_FLAG_EOIC                ADC_STAT_EOIC                               /*!< end of inserted group conversion interrupt flag */


/******************************************************************************/
/*                                                                            */
/*                      Analog to Digital Converter (ADC)                     */
/*                                                                            */
/******************************************************************************/

/*
 * @brief Specific device feature definitions (not present on all devices in the STM32F1 family)
 */
#define ADC_MULTIMODE_SUPPORT                          /*!< ADC feature available only on specific devices: multimode available on devices with several ADC instances */

/********************  Bit definition for ADC_SR register  ********************/
#define ADC_SR_AWD_Pos                      (0U)                               
#define ADC_SR_AWD_Msk                      (0x1UL << ADC_SR_AWD_Pos)           /*!< 0x00000001 */
#define ADC_SR_AWD                          ADC_SR_AWD_Msk                     /*!< ADC analog watchdog 1 flag */
#define ADC_SR_EOS_Pos                      (1U)                               
#define ADC_SR_EOS_Msk                      (0x1UL << ADC_SR_EOS_Pos)           /*!< 0x00000002 */
#define ADC_SR_EOS                          ADC_SR_EOS_Msk                     /*!< ADC group regular end of sequence conversions flag */
#define ADC_SR_JEOS_Pos                     (2U)                               
#define ADC_SR_JEOS_Msk                     (0x1UL << ADC_SR_JEOS_Pos)          /*!< 0x00000004 */
#define ADC_SR_JEOS                         ADC_SR_JEOS_Msk                    /*!< ADC group injected end of sequence conversions flag */
#define ADC_SR_JSTRT_Pos                    (3U)                               
#define ADC_SR_JSTRT_Msk                    (0x1UL << ADC_SR_JSTRT_Pos)         /*!< 0x00000008 */
#define ADC_SR_JSTRT                        ADC_SR_JSTRT_Msk                   /*!< ADC group injected conversion start flag */
#define ADC_SR_STRT_Pos                     (4U)                               
#define ADC_SR_STRT_Msk                     (0x1UL << ADC_SR_STRT_Pos)          /*!< 0x00000010 */
#define ADC_SR_STRT                         ADC_SR_STRT_Msk                    /*!< ADC group regular conversion start flag */

/* Legacy defines */
#define  ADC_SR_EOC                          (ADC_SR_EOS)
#define  ADC_SR_JEOC                         (ADC_SR_JEOS)

/*******************  Bit definition for ADC_CR1 register  ********************/
#define ADC_CR1_AWDCH_Pos                   (0U)                               
#define ADC_CR1_AWDCH_Msk                   (0x1FUL << ADC_CR1_AWDCH_Pos)       /*!< 0x0000001F */
#define ADC_CR1_AWDCH                       ADC_CR1_AWDCH_Msk                  /*!< ADC analog watchdog 1 monitored channel selection */
#define ADC_CR1_AWDCH_0                     (0x01UL << ADC_CR1_AWDCH_Pos)       /*!< 0x00000001 */
#define ADC_CR1_AWDCH_1                     (0x02UL << ADC_CR1_AWDCH_Pos)       /*!< 0x00000002 */
#define ADC_CR1_AWDCH_2                     (0x04UL << ADC_CR1_AWDCH_Pos)       /*!< 0x00000004 */
#define ADC_CR1_AWDCH_3                     (0x08UL << ADC_CR1_AWDCH_Pos)       /*!< 0x00000008 */
#define ADC_CR1_AWDCH_4                     (0x10UL << ADC_CR1_AWDCH_Pos)       /*!< 0x00000010 */

#define ADC_CR1_EOSIE_Pos                   (5U)                               
#define ADC_CR1_EOSIE_Msk                   (0x1UL << ADC_CR1_EOSIE_Pos)        /*!< 0x00000020 */
#define ADC_CR1_EOSIE                       ADC_CR1_EOSIE_Msk                  /*!< ADC group regular end of sequence conversions interrupt */
#define ADC_CR1_AWDIE_Pos                   (6U)                               
#define ADC_CR1_AWDIE_Msk                   (0x1UL << ADC_CR1_AWDIE_Pos)        /*!< 0x00000040 */
#define ADC_CR1_AWDIE                       ADC_CR1_AWDIE_Msk                  /*!< ADC analog watchdog 1 interrupt */
#define ADC_CR1_JEOSIE_Pos                  (7U)                               
#define ADC_CR1_JEOSIE_Msk                  (0x1UL << ADC_CR1_JEOSIE_Pos)       /*!< 0x00000080 */
#define ADC_CR1_JEOSIE                      ADC_CR1_JEOSIE_Msk                 /*!< ADC group injected end of sequence conversions interrupt */
#define ADC_CR1_SCAN_Pos                    (8U)                               
#define ADC_CR1_SCAN_Msk                    (0x1UL << ADC_CR1_SCAN_Pos)         /*!< 0x00000100 */
#define ADC_CR1_SCAN                        ADC_CR1_SCAN_Msk                   /*!< ADC scan mode */
#define ADC_CR1_AWDSGL_Pos                  (9U)                               
#define ADC_CR1_AWDSGL_Msk                  (0x1UL << ADC_CR1_AWDSGL_Pos)       /*!< 0x00000200 */
#define ADC_CR1_AWDSGL                      ADC_CR1_AWDSGL_Msk                 /*!< ADC analog watchdog 1 monitoring a single channel or all channels */
#define ADC_CR1_JAUTO_Pos                   (10U)                              
#define ADC_CR1_JAUTO_Msk                   (0x1UL << ADC_CR1_JAUTO_Pos)        /*!< 0x00000400 */
#define ADC_CR1_JAUTO                       ADC_CR1_JAUTO_Msk                  /*!< ADC group injected automatic trigger mode */
#define ADC_CR1_DISCEN_Pos                  (11U)                              
#define ADC_CR1_DISCEN_Msk                  (0x1UL << ADC_CR1_DISCEN_Pos)       /*!< 0x00000800 */
#define ADC_CR1_DISCEN                      ADC_CR1_DISCEN_Msk                 /*!< ADC group regular sequencer discontinuous mode */
#define ADC_CR1_JDISCEN_Pos                 (12U)                              
#define ADC_CR1_JDISCEN_Msk                 (0x1UL << ADC_CR1_JDISCEN_Pos)      /*!< 0x00001000 */
#define ADC_CR1_JDISCEN                     ADC_CR1_JDISCEN_Msk                /*!< ADC group injected sequencer discontinuous mode */

#define ADC_CR1_DISCNUM_Pos                 (13U)                              
#define ADC_CR1_DISCNUM_Msk                 (0x7UL << ADC_CR1_DISCNUM_Pos)      /*!< 0x0000E000 */
#define ADC_CR1_DISCNUM                     ADC_CR1_DISCNUM_Msk                /*!< ADC group regular sequencer discontinuous number of ranks */
#define ADC_CR1_DISCNUM_0                   (0x1UL << ADC_CR1_DISCNUM_Pos)      /*!< 0x00002000 */
#define ADC_CR1_DISCNUM_1                   (0x2UL << ADC_CR1_DISCNUM_Pos)      /*!< 0x00004000 */
#define ADC_CR1_DISCNUM_2                   (0x4UL << ADC_CR1_DISCNUM_Pos)      /*!< 0x00008000 */

#define ADC_CR1_DUALMOD_Pos                 (16U)                              
#define ADC_CR1_DUALMOD_Msk                 (0xFUL << ADC_CR1_DUALMOD_Pos)      /*!< 0x000F0000 */
#define ADC_CR1_DUALMOD                     ADC_CR1_DUALMOD_Msk                /*!< ADC multimode mode selection */
#define ADC_CR1_DUALMOD_0                   (0x1UL << ADC_CR1_DUALMOD_Pos)      /*!< 0x00010000 */
#define ADC_CR1_DUALMOD_1                   (0x2UL << ADC_CR1_DUALMOD_Pos)      /*!< 0x00020000 */
#define ADC_CR1_DUALMOD_2                   (0x4UL << ADC_CR1_DUALMOD_Pos)      /*!< 0x00040000 */
#define ADC_CR1_DUALMOD_3                   (0x8UL << ADC_CR1_DUALMOD_Pos)      /*!< 0x00080000 */

#define ADC_CR1_JAWDEN_Pos                  (22U)                              
#define ADC_CR1_JAWDEN_Msk                  (0x1UL << ADC_CR1_JAWDEN_Pos)       /*!< 0x00400000 */
#define ADC_CR1_JAWDEN                      ADC_CR1_JAWDEN_Msk                 /*!< ADC analog watchdog 1 enable on scope ADC group injected */
#define ADC_CR1_AWDEN_Pos                   (23U)                              
#define ADC_CR1_AWDEN_Msk                   (0x1UL << ADC_CR1_AWDEN_Pos)        /*!< 0x00800000 */
#define ADC_CR1_AWDEN                       ADC_CR1_AWDEN_Msk                  /*!< ADC analog watchdog 1 enable on scope ADC group regular */

/* Legacy defines */
#define  ADC_CR1_EOCIE                       (ADC_CR1_EOSIE)
#define  ADC_CR1_JEOCIE                      (ADC_CR1_JEOSIE)

/*******************  Bit definition for ADC_CR2 register  ********************/
#define ADC_CR2_ADON_Pos                    (0U)                               
#define ADC_CR2_ADON_Msk                    (0x1UL << ADC_CR2_ADON_Pos)         /*!< 0x00000001 */
#define ADC_CR2_ADON                        ADC_CR2_ADON_Msk                   /*!< ADC enable */
#define ADC_CR2_CONT_Pos                    (1U)                               
#define ADC_CR2_CONT_Msk                    (0x1UL << ADC_CR2_CONT_Pos)         /*!< 0x00000002 */
#define ADC_CR2_CONT                        ADC_CR2_CONT_Msk                   /*!< ADC group regular continuous conversion mode */
#define ADC_CR2_CAL_Pos                     (2U)                               
#define ADC_CR2_CAL_Msk                     (0x1UL << ADC_CR2_CAL_Pos)          /*!< 0x00000004 */
#define ADC_CR2_CAL                         ADC_CR2_CAL_Msk                    /*!< ADC calibration start */
#define ADC_CR2_RSTCAL_Pos                  (3U)                               
#define ADC_CR2_RSTCAL_Msk                  (0x1UL << ADC_CR2_RSTCAL_Pos)       /*!< 0x00000008 */
#define ADC_CR2_RSTCAL                      ADC_CR2_RSTCAL_Msk                 /*!< ADC calibration reset */
#define ADC_CR2_DMA_Pos                     (8U)                               
#define ADC_CR2_DMA_Msk                     (0x1UL << ADC_CR2_DMA_Pos)          /*!< 0x00000100 */
#define ADC_CR2_DMA                         ADC_CR2_DMA_Msk                    /*!< ADC DMA transfer enable */
#define ADC_CR2_ALIGN_Pos                   (11U)                              
#define ADC_CR2_ALIGN_Msk                   (0x1UL << ADC_CR2_ALIGN_Pos)        /*!< 0x00000800 */
#define ADC_CR2_ALIGN                       ADC_CR2_ALIGN_Msk                  /*!< ADC data alignement */

#define ADC_CR2_JEXTSEL_Pos                 (12U)                              
#define ADC_CR2_JEXTSEL_Msk                 (0x7UL << ADC_CR2_JEXTSEL_Pos)      /*!< 0x00007000 */
#define ADC_CR2_JEXTSEL                     ADC_CR2_JEXTSEL_Msk                /*!< ADC group injected external trigger source */
#define ADC_CR2_JEXTSEL_0                   (0x1UL << ADC_CR2_JEXTSEL_Pos)      /*!< 0x00001000 */
#define ADC_CR2_JEXTSEL_1                   (0x2UL << ADC_CR2_JEXTSEL_Pos)      /*!< 0x00002000 */
#define ADC_CR2_JEXTSEL_2                   (0x4UL << ADC_CR2_JEXTSEL_Pos)      /*!< 0x00004000 */

#define ADC_CR2_JEXTTRIG_Pos                (15U)                              
#define ADC_CR2_JEXTTRIG_Msk                (0x1UL << ADC_CR2_JEXTTRIG_Pos)     /*!< 0x00008000 */
#define ADC_CR2_JEXTTRIG                    ADC_CR2_JEXTTRIG_Msk               /*!< ADC group injected external trigger enable */

#define ADC_CR2_EXTSEL_Pos                  (17U)                              
#define ADC_CR2_EXTSEL_Msk                  (0x7UL << ADC_CR2_EXTSEL_Pos)       /*!< 0x000E0000 */
#define ADC_CR2_EXTSEL                      ADC_CR2_EXTSEL_Msk                 /*!< ADC group regular external trigger source */
#define ADC_CR2_EXTSEL_0                    (0x1UL << ADC_CR2_EXTSEL_Pos)       /*!< 0x00020000 */
#define ADC_CR2_EXTSEL_1                    (0x2UL << ADC_CR2_EXTSEL_Pos)       /*!< 0x00040000 */
#define ADC_CR2_EXTSEL_2                    (0x4UL << ADC_CR2_EXTSEL_Pos)       /*!< 0x00080000 */

#define ADC_CR2_EXTTRIG_Pos                 (20U)                              
#define ADC_CR2_EXTTRIG_Msk                 (0x1UL << ADC_CR2_EXTTRIG_Pos)      /*!< 0x00100000 */
#define ADC_CR2_EXTTRIG                     ADC_CR2_EXTTRIG_Msk                /*!< ADC group regular external trigger enable */
#define ADC_CR2_JSWSTART_Pos                (21U)                              
#define ADC_CR2_JSWSTART_Msk                (0x1UL << ADC_CR2_JSWSTART_Pos)     /*!< 0x00200000 */
#define ADC_CR2_JSWSTART                    ADC_CR2_JSWSTART_Msk               /*!< ADC group injected conversion start */
#define ADC_CR2_SWSTART_Pos                 (22U)                              
#define ADC_CR2_SWSTART_Msk                 (0x1UL << ADC_CR2_SWSTART_Pos)      /*!< 0x00400000 */
#define ADC_CR2_SWSTART                     ADC_CR2_SWSTART_Msk                /*!< ADC group regular conversion start */
#define ADC_CR2_TSVREFE_Pos                 (23U)                              
#define ADC_CR2_TSVREFE_Msk                 (0x1UL << ADC_CR2_TSVREFE_Pos)      /*!< 0x00800000 */
#define ADC_CR2_TSVREFE                     ADC_CR2_TSVREFE_Msk                /*!< ADC internal path to VrefInt and temperature sensor enable */

/******************  Bit definition for ADC_SMPR1 register  *******************/
#define ADC_SMPR1_SMP10_Pos                 (0U)                               
#define ADC_SMPR1_SMP10_Msk                 (0x7UL << ADC_SMPR1_SMP10_Pos)      /*!< 0x00000007 */
#define ADC_SMPR1_SMP10                     ADC_SMPR1_SMP10_Msk                /*!< ADC channel 10 sampling time selection  */
#define ADC_SMPR1_SMP10_0                   (0x1UL << ADC_SMPR1_SMP10_Pos)      /*!< 0x00000001 */
#define ADC_SMPR1_SMP10_1                   (0x2UL << ADC_SMPR1_SMP10_Pos)      /*!< 0x00000002 */
#define ADC_SMPR1_SMP10_2                   (0x4UL << ADC_SMPR1_SMP10_Pos)      /*!< 0x00000004 */

#define ADC_SMPR1_SMP11_Pos                 (3U)                               
#define ADC_SMPR1_SMP11_Msk                 (0x7UL << ADC_SMPR1_SMP11_Pos)      /*!< 0x00000038 */
#define ADC_SMPR1_SMP11                     ADC_SMPR1_SMP11_Msk                /*!< ADC channel 11 sampling time selection  */
#define ADC_SMPR1_SMP11_0                   (0x1UL << ADC_SMPR1_SMP11_Pos)      /*!< 0x00000008 */
#define ADC_SMPR1_SMP11_1                   (0x2UL << ADC_SMPR1_SMP11_Pos)      /*!< 0x00000010 */
#define ADC_SMPR1_SMP11_2                   (0x4UL << ADC_SMPR1_SMP11_Pos)      /*!< 0x00000020 */

#define ADC_SMPR1_SMP12_Pos                 (6U)                               
#define ADC_SMPR1_SMP12_Msk                 (0x7UL << ADC_SMPR1_SMP12_Pos)      /*!< 0x000001C0 */
#define ADC_SMPR1_SMP12                     ADC_SMPR1_SMP12_Msk                /*!< ADC channel 12 sampling time selection  */
#define ADC_SMPR1_SMP12_0                   (0x1UL << ADC_SMPR1_SMP12_Pos)      /*!< 0x00000040 */
#define ADC_SMPR1_SMP12_1                   (0x2UL << ADC_SMPR1_SMP12_Pos)      /*!< 0x00000080 */
#define ADC_SMPR1_SMP12_2                   (0x4UL << ADC_SMPR1_SMP12_Pos)      /*!< 0x00000100 */

#define ADC_SMPR1_SMP13_Pos                 (9U)                               
#define ADC_SMPR1_SMP13_Msk                 (0x7UL << ADC_SMPR1_SMP13_Pos)      /*!< 0x00000E00 */
#define ADC_SMPR1_SMP13                     ADC_SMPR1_SMP13_Msk                /*!< ADC channel 13 sampling time selection  */
#define ADC_SMPR1_SMP13_0                   (0x1UL << ADC_SMPR1_SMP13_Pos)      /*!< 0x00000200 */
#define ADC_SMPR1_SMP13_1                   (0x2UL << ADC_SMPR1_SMP13_Pos)      /*!< 0x00000400 */
#define ADC_SMPR1_SMP13_2                   (0x4UL << ADC_SMPR1_SMP13_Pos)      /*!< 0x00000800 */

#define ADC_SMPR1_SMP14_Pos                 (12U)                              
#define ADC_SMPR1_SMP14_Msk                 (0x7UL << ADC_SMPR1_SMP14_Pos)      /*!< 0x00007000 */
#define ADC_SMPR1_SMP14                     ADC_SMPR1_SMP14_Msk                /*!< ADC channel 14 sampling time selection  */
#define ADC_SMPR1_SMP14_0                   (0x1UL << ADC_SMPR1_SMP14_Pos)      /*!< 0x00001000 */
#define ADC_SMPR1_SMP14_1                   (0x2UL << ADC_SMPR1_SMP14_Pos)      /*!< 0x00002000 */
#define ADC_SMPR1_SMP14_2                   (0x4UL << ADC_SMPR1_SMP14_Pos)      /*!< 0x00004000 */

#define ADC_SMPR1_SMP15_Pos                 (15U)                              
#define ADC_SMPR1_SMP15_Msk                 (0x7UL << ADC_SMPR1_SMP15_Pos)      /*!< 0x00038000 */
#define ADC_SMPR1_SMP15                     ADC_SMPR1_SMP15_Msk                /*!< ADC channel 15 sampling time selection  */
#define ADC_SMPR1_SMP15_0                   (0x1UL << ADC_SMPR1_SMP15_Pos)      /*!< 0x00008000 */
#define ADC_SMPR1_SMP15_1                   (0x2UL << ADC_SMPR1_SMP15_Pos)      /*!< 0x00010000 */
#define ADC_SMPR1_SMP15_2                   (0x4UL << ADC_SMPR1_SMP15_Pos)      /*!< 0x00020000 */

#define ADC_SMPR1_SMP16_Pos                 (18U)                              
#define ADC_SMPR1_SMP16_Msk                 (0x7UL << ADC_SMPR1_SMP16_Pos)      /*!< 0x001C0000 */
#define ADC_SMPR1_SMP16                     ADC_SMPR1_SMP16_Msk                /*!< ADC channel 16 sampling time selection  */
#define ADC_SMPR1_SMP16_0                   (0x1UL << ADC_SMPR1_SMP16_Pos)      /*!< 0x00040000 */
#define ADC_SMPR1_SMP16_1                   (0x2UL << ADC_SMPR1_SMP16_Pos)      /*!< 0x00080000 */
#define ADC_SMPR1_SMP16_2                   (0x4UL << ADC_SMPR1_SMP16_Pos)      /*!< 0x00100000 */

#define ADC_SMPR1_SMP17_Pos                 (21U)                              
#define ADC_SMPR1_SMP17_Msk                 (0x7UL << ADC_SMPR1_SMP17_Pos)      /*!< 0x00E00000 */
#define ADC_SMPR1_SMP17                     ADC_SMPR1_SMP17_Msk                /*!< ADC channel 17 sampling time selection  */
#define ADC_SMPR1_SMP17_0                   (0x1UL << ADC_SMPR1_SMP17_Pos)      /*!< 0x00200000 */
#define ADC_SMPR1_SMP17_1                   (0x2UL << ADC_SMPR1_SMP17_Pos)      /*!< 0x00400000 */
#define ADC_SMPR1_SMP17_2                   (0x4UL << ADC_SMPR1_SMP17_Pos)      /*!< 0x00800000 */

/******************  Bit definition for ADC_SMPR2 register  *******************/
#define ADC_SMPR2_SMP0_Pos                  (0U)                               
#define ADC_SMPR2_SMP0_Msk                  (0x7UL << ADC_SMPR2_SMP0_Pos)       /*!< 0x00000007 */
#define ADC_SMPR2_SMP0                      ADC_SMPR2_SMP0_Msk                 /*!< ADC channel 0 sampling time selection  */
#define ADC_SMPR2_SMP0_0                    (0x1UL << ADC_SMPR2_SMP0_Pos)       /*!< 0x00000001 */
#define ADC_SMPR2_SMP0_1                    (0x2UL << ADC_SMPR2_SMP0_Pos)       /*!< 0x00000002 */
#define ADC_SMPR2_SMP0_2                    (0x4UL << ADC_SMPR2_SMP0_Pos)       /*!< 0x00000004 */

#define ADC_SMPR2_SMP1_Pos                  (3U)                               
#define ADC_SMPR2_SMP1_Msk                  (0x7UL << ADC_SMPR2_SMP1_Pos)       /*!< 0x00000038 */
#define ADC_SMPR2_SMP1                      ADC_SMPR2_SMP1_Msk                 /*!< ADC channel 1 sampling time selection  */
#define ADC_SMPR2_SMP1_0                    (0x1UL << ADC_SMPR2_SMP1_Pos)       /*!< 0x00000008 */
#define ADC_SMPR2_SMP1_1                    (0x2UL << ADC_SMPR2_SMP1_Pos)       /*!< 0x00000010 */
#define ADC_SMPR2_SMP1_2                    (0x4UL << ADC_SMPR2_SMP1_Pos)       /*!< 0x00000020 */

#define ADC_SMPR2_SMP2_Pos                  (6U)                               
#define ADC_SMPR2_SMP2_Msk                  (0x7UL << ADC_SMPR2_SMP2_Pos)       /*!< 0x000001C0 */
#define ADC_SMPR2_SMP2                      ADC_SMPR2_SMP2_Msk                 /*!< ADC channel 2 sampling time selection  */
#define ADC_SMPR2_SMP2_0                    (0x1UL << ADC_SMPR2_SMP2_Pos)       /*!< 0x00000040 */
#define ADC_SMPR2_SMP2_1                    (0x2UL << ADC_SMPR2_SMP2_Pos)       /*!< 0x00000080 */
#define ADC_SMPR2_SMP2_2                    (0x4UL << ADC_SMPR2_SMP2_Pos)       /*!< 0x00000100 */

#define ADC_SMPR2_SMP3_Pos                  (9U)                               
#define ADC_SMPR2_SMP3_Msk                  (0x7UL << ADC_SMPR2_SMP3_Pos)       /*!< 0x00000E00 */
#define ADC_SMPR2_SMP3                      ADC_SMPR2_SMP3_Msk                 /*!< ADC channel 3 sampling time selection  */
#define ADC_SMPR2_SMP3_0                    (0x1UL << ADC_SMPR2_SMP3_Pos)       /*!< 0x00000200 */
#define ADC_SMPR2_SMP3_1                    (0x2UL << ADC_SMPR2_SMP3_Pos)       /*!< 0x00000400 */
#define ADC_SMPR2_SMP3_2                    (0x4UL << ADC_SMPR2_SMP3_Pos)       /*!< 0x00000800 */

#define ADC_SMPR2_SMP4_Pos                  (12U)                              
#define ADC_SMPR2_SMP4_Msk                  (0x7UL << ADC_SMPR2_SMP4_Pos)       /*!< 0x00007000 */
#define ADC_SMPR2_SMP4                      ADC_SMPR2_SMP4_Msk                 /*!< ADC channel 4 sampling time selection  */
#define ADC_SMPR2_SMP4_0                    (0x1UL << ADC_SMPR2_SMP4_Pos)       /*!< 0x00001000 */
#define ADC_SMPR2_SMP4_1                    (0x2UL << ADC_SMPR2_SMP4_Pos)       /*!< 0x00002000 */
#define ADC_SMPR2_SMP4_2                    (0x4UL << ADC_SMPR2_SMP4_Pos)       /*!< 0x00004000 */

#define ADC_SMPR2_SMP5_Pos                  (15U)                              
#define ADC_SMPR2_SMP5_Msk                  (0x7UL << ADC_SMPR2_SMP5_Pos)       /*!< 0x00038000 */
#define ADC_SMPR2_SMP5                      ADC_SMPR2_SMP5_Msk                 /*!< ADC channel 5 sampling time selection  */
#define ADC_SMPR2_SMP5_0                    (0x1UL << ADC_SMPR2_SMP5_Pos)       /*!< 0x00008000 */
#define ADC_SMPR2_SMP5_1                    (0x2UL << ADC_SMPR2_SMP5_Pos)       /*!< 0x00010000 */
#define ADC_SMPR2_SMP5_2                    (0x4UL << ADC_SMPR2_SMP5_Pos)       /*!< 0x00020000 */

#define ADC_SMPR2_SMP6_Pos                  (18U)                              
#define ADC_SMPR2_SMP6_Msk                  (0x7UL << ADC_SMPR2_SMP6_Pos)       /*!< 0x001C0000 */
#define ADC_SMPR2_SMP6                      ADC_SMPR2_SMP6_Msk                 /*!< ADC channel 6 sampling time selection  */
#define ADC_SMPR2_SMP6_0                    (0x1UL << ADC_SMPR2_SMP6_Pos)       /*!< 0x00040000 */
#define ADC_SMPR2_SMP6_1                    (0x2UL << ADC_SMPR2_SMP6_Pos)       /*!< 0x00080000 */
#define ADC_SMPR2_SMP6_2                    (0x4UL << ADC_SMPR2_SMP6_Pos)       /*!< 0x00100000 */

#define ADC_SMPR2_SMP7_Pos                  (21U)                              
#define ADC_SMPR2_SMP7_Msk                  (0x7UL << ADC_SMPR2_SMP7_Pos)       /*!< 0x00E00000 */
#define ADC_SMPR2_SMP7                      ADC_SMPR2_SMP7_Msk                 /*!< ADC channel 7 sampling time selection  */
#define ADC_SMPR2_SMP7_0                    (0x1UL << ADC_SMPR2_SMP7_Pos)       /*!< 0x00200000 */
#define ADC_SMPR2_SMP7_1                    (0x2UL << ADC_SMPR2_SMP7_Pos)       /*!< 0x00400000 */
#define ADC_SMPR2_SMP7_2                    (0x4UL << ADC_SMPR2_SMP7_Pos)       /*!< 0x00800000 */

#define ADC_SMPR2_SMP8_Pos                  (24U)                              
#define ADC_SMPR2_SMP8_Msk                  (0x7UL << ADC_SMPR2_SMP8_Pos)       /*!< 0x07000000 */
#define ADC_SMPR2_SMP8                      ADC_SMPR2_SMP8_Msk                 /*!< ADC channel 8 sampling time selection  */
#define ADC_SMPR2_SMP8_0                    (0x1UL << ADC_SMPR2_SMP8_Pos)       /*!< 0x01000000 */
#define ADC_SMPR2_SMP8_1                    (0x2UL << ADC_SMPR2_SMP8_Pos)       /*!< 0x02000000 */
#define ADC_SMPR2_SMP8_2                    (0x4UL << ADC_SMPR2_SMP8_Pos)       /*!< 0x04000000 */

#define ADC_SMPR2_SMP9_Pos                  (27U)                              
#define ADC_SMPR2_SMP9_Msk                  (0x7UL << ADC_SMPR2_SMP9_Pos)       /*!< 0x38000000 */
#define ADC_SMPR2_SMP9                      ADC_SMPR2_SMP9_Msk                 /*!< ADC channel 9 sampling time selection  */
#define ADC_SMPR2_SMP9_0                    (0x1UL << ADC_SMPR2_SMP9_Pos)       /*!< 0x08000000 */
#define ADC_SMPR2_SMP9_1                    (0x2UL << ADC_SMPR2_SMP9_Pos)       /*!< 0x10000000 */
#define ADC_SMPR2_SMP9_2                    (0x4UL << ADC_SMPR2_SMP9_Pos)       /*!< 0x20000000 */

/******************  Bit definition for ADC_JOFR1 register  *******************/
#define ADC_JOFR1_JOFFSET1_Pos              (0U)                               
#define ADC_JOFR1_JOFFSET1_Msk              (0xFFFUL << ADC_JOFR1_JOFFSET1_Pos) /*!< 0x00000FFF */
#define ADC_JOFR1_JOFFSET1                  ADC_JOFR1_JOFFSET1_Msk             /*!< ADC group injected sequencer rank 1 offset value */

/******************  Bit definition for ADC_JOFR2 register  *******************/
#define ADC_JOFR2_JOFFSET2_Pos              (0U)                               
#define ADC_JOFR2_JOFFSET2_Msk              (0xFFFUL << ADC_JOFR2_JOFFSET2_Pos) /*!< 0x00000FFF */
#define ADC_JOFR2_JOFFSET2                  ADC_JOFR2_JOFFSET2_Msk             /*!< ADC group injected sequencer rank 2 offset value */

/******************  Bit definition for ADC_JOFR3 register  *******************/
#define ADC_JOFR3_JOFFSET3_Pos              (0U)                               
#define ADC_JOFR3_JOFFSET3_Msk              (0xFFFUL << ADC_JOFR3_JOFFSET3_Pos) /*!< 0x00000FFF */
#define ADC_JOFR3_JOFFSET3                  ADC_JOFR3_JOFFSET3_Msk             /*!< ADC group injected sequencer rank 3 offset value */

/******************  Bit definition for ADC_JOFR4 register  *******************/
#define ADC_JOFR4_JOFFSET4_Pos              (0U)                               
#define ADC_JOFR4_JOFFSET4_Msk              (0xFFFUL << ADC_JOFR4_JOFFSET4_Pos) /*!< 0x00000FFF */
#define ADC_JOFR4_JOFFSET4                  ADC_JOFR4_JOFFSET4_Msk             /*!< ADC group injected sequencer rank 4 offset value */

/*******************  Bit definition for ADC_HTR register  ********************/
#define ADC_HTR_HT_Pos                      (0U)                               
#define ADC_HTR_HT_Msk                      (0xFFFUL << ADC_HTR_HT_Pos)         /*!< 0x00000FFF */
#define ADC_HTR_HT                          ADC_HTR_HT_Msk                     /*!< ADC analog watchdog 1 threshold high */

/*******************  Bit definition for ADC_LTR register  ********************/
#define ADC_LTR_LT_Pos                      (0U)                               
#define ADC_LTR_LT_Msk                      (0xFFFUL << ADC_LTR_LT_Pos)         /*!< 0x00000FFF */
#define ADC_LTR_LT                          ADC_LTR_LT_Msk                     /*!< ADC analog watchdog 1 threshold low */

/*******************  Bit definition for ADC_SQR1 register  *******************/
#define ADC_SQR1_SQ13_Pos                   (0U)                               
#define ADC_SQR1_SQ13_Msk                   (0x1FUL << ADC_SQR1_SQ13_Pos)       /*!< 0x0000001F */
#define ADC_SQR1_SQ13                       ADC_SQR1_SQ13_Msk                  /*!< ADC group regular sequencer rank 13 */
#define ADC_SQR1_SQ13_0                     (0x01UL << ADC_SQR1_SQ13_Pos)       /*!< 0x00000001 */
#define ADC_SQR1_SQ13_1                     (0x02UL << ADC_SQR1_SQ13_Pos)       /*!< 0x00000002 */
#define ADC_SQR1_SQ13_2                     (0x04UL << ADC_SQR1_SQ13_Pos)       /*!< 0x00000004 */
#define ADC_SQR1_SQ13_3                     (0x08UL << ADC_SQR1_SQ13_Pos)       /*!< 0x00000008 */
#define ADC_SQR1_SQ13_4                     (0x10UL << ADC_SQR1_SQ13_Pos)       /*!< 0x00000010 */

#define ADC_SQR1_SQ14_Pos                   (5U)                               
#define ADC_SQR1_SQ14_Msk                   (0x1FUL << ADC_SQR1_SQ14_Pos)       /*!< 0x000003E0 */
#define ADC_SQR1_SQ14                       ADC_SQR1_SQ14_Msk                  /*!< ADC group regular sequencer rank 14 */
#define ADC_SQR1_SQ14_0                     (0x01UL << ADC_SQR1_SQ14_Pos)       /*!< 0x00000020 */
#define ADC_SQR1_SQ14_1                     (0x02UL << ADC_SQR1_SQ14_Pos)       /*!< 0x00000040 */
#define ADC_SQR1_SQ14_2                     (0x04UL << ADC_SQR1_SQ14_Pos)       /*!< 0x00000080 */
#define ADC_SQR1_SQ14_3                     (0x08UL << ADC_SQR1_SQ14_Pos)       /*!< 0x00000100 */
#define ADC_SQR1_SQ14_4                     (0x10UL << ADC_SQR1_SQ14_Pos)       /*!< 0x00000200 */

#define ADC_SQR1_SQ15_Pos                   (10U)                              
#define ADC_SQR1_SQ15_Msk                   (0x1FUL << ADC_SQR1_SQ15_Pos)       /*!< 0x00007C00 */
#define ADC_SQR1_SQ15                       ADC_SQR1_SQ15_Msk                  /*!< ADC group regular sequencer rank 15 */
#define ADC_SQR1_SQ15_0                     (0x01UL << ADC_SQR1_SQ15_Pos)       /*!< 0x00000400 */
#define ADC_SQR1_SQ15_1                     (0x02UL << ADC_SQR1_SQ15_Pos)       /*!< 0x00000800 */
#define ADC_SQR1_SQ15_2                     (0x04UL << ADC_SQR1_SQ15_Pos)       /*!< 0x00001000 */
#define ADC_SQR1_SQ15_3                     (0x08UL << ADC_SQR1_SQ15_Pos)       /*!< 0x00002000 */
#define ADC_SQR1_SQ15_4                     (0x10UL << ADC_SQR1_SQ15_Pos)       /*!< 0x00004000 */

#define ADC_SQR1_SQ16_Pos                   (15U)                              
#define ADC_SQR1_SQ16_Msk                   (0x1FUL << ADC_SQR1_SQ16_Pos)       /*!< 0x000F8000 */
#define ADC_SQR1_SQ16                       ADC_SQR1_SQ16_Msk                  /*!< ADC group regular sequencer rank 16 */
#define ADC_SQR1_SQ16_0                     (0x01UL << ADC_SQR1_SQ16_Pos)       /*!< 0x00008000 */
#define ADC_SQR1_SQ16_1                     (0x02UL << ADC_SQR1_SQ16_Pos)       /*!< 0x00010000 */
#define ADC_SQR1_SQ16_2                     (0x04UL << ADC_SQR1_SQ16_Pos)       /*!< 0x00020000 */
#define ADC_SQR1_SQ16_3                     (0x08UL << ADC_SQR1_SQ16_Pos)       /*!< 0x00040000 */
#define ADC_SQR1_SQ16_4                     (0x10UL << ADC_SQR1_SQ16_Pos)       /*!< 0x00080000 */

#define ADC_SQR1_L_Pos                      (20U)                              
#define ADC_SQR1_L_Msk                      (0xFUL << ADC_SQR1_L_Pos)           /*!< 0x00F00000 */
#define ADC_SQR1_L                          ADC_SQR1_L_Msk                     /*!< ADC group regular sequencer scan length */
#define ADC_SQR1_L_0                        (0x1UL << ADC_SQR1_L_Pos)           /*!< 0x00100000 */
#define ADC_SQR1_L_1                        (0x2UL << ADC_SQR1_L_Pos)           /*!< 0x00200000 */
#define ADC_SQR1_L_2                        (0x4UL << ADC_SQR1_L_Pos)           /*!< 0x00400000 */
#define ADC_SQR1_L_3                        (0x8UL << ADC_SQR1_L_Pos)           /*!< 0x00800000 */

/*******************  Bit definition for ADC_SQR2 register  *******************/
#define ADC_SQR2_SQ7_Pos                    (0U)                               
#define ADC_SQR2_SQ7_Msk                    (0x1FUL << ADC_SQR2_SQ7_Pos)        /*!< 0x0000001F */
#define ADC_SQR2_SQ7                        ADC_SQR2_SQ7_Msk                   /*!< ADC group regular sequencer rank 7 */
#define ADC_SQR2_SQ7_0                      (0x01UL << ADC_SQR2_SQ7_Pos)        /*!< 0x00000001 */
#define ADC_SQR2_SQ7_1                      (0x02UL << ADC_SQR2_SQ7_Pos)        /*!< 0x00000002 */
#define ADC_SQR2_SQ7_2                      (0x04UL << ADC_SQR2_SQ7_Pos)        /*!< 0x00000004 */
#define ADC_SQR2_SQ7_3                      (0x08UL << ADC_SQR2_SQ7_Pos)        /*!< 0x00000008 */
#define ADC_SQR2_SQ7_4                      (0x10UL << ADC_SQR2_SQ7_Pos)        /*!< 0x00000010 */

#define ADC_SQR2_SQ8_Pos                    (5U)                               
#define ADC_SQR2_SQ8_Msk                    (0x1FUL << ADC_SQR2_SQ8_Pos)        /*!< 0x000003E0 */
#define ADC_SQR2_SQ8                        ADC_SQR2_SQ8_Msk                   /*!< ADC group regular sequencer rank 8 */
#define ADC_SQR2_SQ8_0                      (0x01UL << ADC_SQR2_SQ8_Pos)        /*!< 0x00000020 */
#define ADC_SQR2_SQ8_1                      (0x02UL << ADC_SQR2_SQ8_Pos)        /*!< 0x00000040 */
#define ADC_SQR2_SQ8_2                      (0x04UL << ADC_SQR2_SQ8_Pos)        /*!< 0x00000080 */
#define ADC_SQR2_SQ8_3                      (0x08UL << ADC_SQR2_SQ8_Pos)        /*!< 0x00000100 */
#define ADC_SQR2_SQ8_4                      (0x10UL << ADC_SQR2_SQ8_Pos)        /*!< 0x00000200 */

#define ADC_SQR2_SQ9_Pos                    (10U)                              
#define ADC_SQR2_SQ9_Msk                    (0x1FUL << ADC_SQR2_SQ9_Pos)        /*!< 0x00007C00 */
#define ADC_SQR2_SQ9                        ADC_SQR2_SQ9_Msk                   /*!< ADC group regular sequencer rank 9 */
#define ADC_SQR2_SQ9_0                      (0x01UL << ADC_SQR2_SQ9_Pos)        /*!< 0x00000400 */
#define ADC_SQR2_SQ9_1                      (0x02UL << ADC_SQR2_SQ9_Pos)        /*!< 0x00000800 */
#define ADC_SQR2_SQ9_2                      (0x04UL << ADC_SQR2_SQ9_Pos)        /*!< 0x00001000 */
#define ADC_SQR2_SQ9_3                      (0x08UL << ADC_SQR2_SQ9_Pos)        /*!< 0x00002000 */
#define ADC_SQR2_SQ9_4                      (0x10UL << ADC_SQR2_SQ9_Pos)        /*!< 0x00004000 */

#define ADC_SQR2_SQ10_Pos                   (15U)                              
#define ADC_SQR2_SQ10_Msk                   (0x1FUL << ADC_SQR2_SQ10_Pos)       /*!< 0x000F8000 */
#define ADC_SQR2_SQ10                       ADC_SQR2_SQ10_Msk                  /*!< ADC group regular sequencer rank 10 */
#define ADC_SQR2_SQ10_0                     (0x01UL << ADC_SQR2_SQ10_Pos)       /*!< 0x00008000 */
#define ADC_SQR2_SQ10_1                     (0x02UL << ADC_SQR2_SQ10_Pos)       /*!< 0x00010000 */
#define ADC_SQR2_SQ10_2                     (0x04UL << ADC_SQR2_SQ10_Pos)       /*!< 0x00020000 */
#define ADC_SQR2_SQ10_3                     (0x08UL << ADC_SQR2_SQ10_Pos)       /*!< 0x00040000 */
#define ADC_SQR2_SQ10_4                     (0x10UL << ADC_SQR2_SQ10_Pos)       /*!< 0x00080000 */

#define ADC_SQR2_SQ11_Pos                   (20U)                              
#define ADC_SQR2_SQ11_Msk                   (0x1FUL << ADC_SQR2_SQ11_Pos)       /*!< 0x01F00000 */
#define ADC_SQR2_SQ11                       ADC_SQR2_SQ11_Msk                  /*!< ADC group regular sequencer rank 1 */
#define ADC_SQR2_SQ11_0                     (0x01UL << ADC_SQR2_SQ11_Pos)       /*!< 0x00100000 */
#define ADC_SQR2_SQ11_1                     (0x02UL << ADC_SQR2_SQ11_Pos)       /*!< 0x00200000 */
#define ADC_SQR2_SQ11_2                     (0x04UL << ADC_SQR2_SQ11_Pos)       /*!< 0x00400000 */
#define ADC_SQR2_SQ11_3                     (0x08UL << ADC_SQR2_SQ11_Pos)       /*!< 0x00800000 */
#define ADC_SQR2_SQ11_4                     (0x10UL << ADC_SQR2_SQ11_Pos)       /*!< 0x01000000 */

#define ADC_SQR2_SQ12_Pos                   (25U)                              
#define ADC_SQR2_SQ12_Msk                   (0x1FUL << ADC_SQR2_SQ12_Pos)       /*!< 0x3E000000 */
#define ADC_SQR2_SQ12                       ADC_SQR2_SQ12_Msk                  /*!< ADC group regular sequencer rank 12 */
#define ADC_SQR2_SQ12_0                     (0x01UL << ADC_SQR2_SQ12_Pos)       /*!< 0x02000000 */
#define ADC_SQR2_SQ12_1                     (0x02UL << ADC_SQR2_SQ12_Pos)       /*!< 0x04000000 */
#define ADC_SQR2_SQ12_2                     (0x04UL << ADC_SQR2_SQ12_Pos)       /*!< 0x08000000 */
#define ADC_SQR2_SQ12_3                     (0x08UL << ADC_SQR2_SQ12_Pos)       /*!< 0x10000000 */
#define ADC_SQR2_SQ12_4                     (0x10UL << ADC_SQR2_SQ12_Pos)       /*!< 0x20000000 */

/*******************  Bit definition for ADC_SQR3 register  *******************/
#define ADC_SQR3_SQ1_Pos                    (0U)                               
#define ADC_SQR3_SQ1_Msk                    (0x1FUL << ADC_SQR3_SQ1_Pos)        /*!< 0x0000001F */
#define ADC_SQR3_SQ1                        ADC_SQR3_SQ1_Msk                   /*!< ADC group regular sequencer rank 1 */
#define ADC_SQR3_SQ1_0                      (0x01UL << ADC_SQR3_SQ1_Pos)        /*!< 0x00000001 */
#define ADC_SQR3_SQ1_1                      (0x02UL << ADC_SQR3_SQ1_Pos)        /*!< 0x00000002 */
#define ADC_SQR3_SQ1_2                      (0x04UL << ADC_SQR3_SQ1_Pos)        /*!< 0x00000004 */
#define ADC_SQR3_SQ1_3                      (0x08UL << ADC_SQR3_SQ1_Pos)        /*!< 0x00000008 */
#define ADC_SQR3_SQ1_4                      (0x10UL << ADC_SQR3_SQ1_Pos)        /*!< 0x00000010 */

#define ADC_SQR3_SQ2_Pos                    (5U)                               
#define ADC_SQR3_SQ2_Msk                    (0x1FUL << ADC_SQR3_SQ2_Pos)        /*!< 0x000003E0 */
#define ADC_SQR3_SQ2                        ADC_SQR3_SQ2_Msk                   /*!< ADC group regular sequencer rank 2 */
#define ADC_SQR3_SQ2_0                      (0x01UL << ADC_SQR3_SQ2_Pos)        /*!< 0x00000020 */
#define ADC_SQR3_SQ2_1                      (0x02UL << ADC_SQR3_SQ2_Pos)        /*!< 0x00000040 */
#define ADC_SQR3_SQ2_2                      (0x04UL << ADC_SQR3_SQ2_Pos)        /*!< 0x00000080 */
#define ADC_SQR3_SQ2_3                      (0x08UL << ADC_SQR3_SQ2_Pos)        /*!< 0x00000100 */
#define ADC_SQR3_SQ2_4                      (0x10UL << ADC_SQR3_SQ2_Pos)        /*!< 0x00000200 */

#define ADC_SQR3_SQ3_Pos                    (10U)                              
#define ADC_SQR3_SQ3_Msk                    (0x1FUL << ADC_SQR3_SQ3_Pos)        /*!< 0x00007C00 */
#define ADC_SQR3_SQ3                        ADC_SQR3_SQ3_Msk                   /*!< ADC group regular sequencer rank 3 */
#define ADC_SQR3_SQ3_0                      (0x01UL << ADC_SQR3_SQ3_Pos)        /*!< 0x00000400 */
#define ADC_SQR3_SQ3_1                      (0x02UL << ADC_SQR3_SQ3_Pos)        /*!< 0x00000800 */
#define ADC_SQR3_SQ3_2                      (0x04UL << ADC_SQR3_SQ3_Pos)        /*!< 0x00001000 */
#define ADC_SQR3_SQ3_3                      (0x08UL << ADC_SQR3_SQ3_Pos)        /*!< 0x00002000 */
#define ADC_SQR3_SQ3_4                      (0x10UL << ADC_SQR3_SQ3_Pos)        /*!< 0x00004000 */

#define ADC_SQR3_SQ4_Pos                    (15U)                              
#define ADC_SQR3_SQ4_Msk                    (0x1FUL << ADC_SQR3_SQ4_Pos)        /*!< 0x000F8000 */
#define ADC_SQR3_SQ4                        ADC_SQR3_SQ4_Msk                   /*!< ADC group regular sequencer rank 4 */
#define ADC_SQR3_SQ4_0                      (0x01UL << ADC_SQR3_SQ4_Pos)        /*!< 0x00008000 */
#define ADC_SQR3_SQ4_1                      (0x02UL << ADC_SQR3_SQ4_Pos)        /*!< 0x00010000 */
#define ADC_SQR3_SQ4_2                      (0x04UL << ADC_SQR3_SQ4_Pos)        /*!< 0x00020000 */
#define ADC_SQR3_SQ4_3                      (0x08UL << ADC_SQR3_SQ4_Pos)        /*!< 0x00040000 */
#define ADC_SQR3_SQ4_4                      (0x10UL << ADC_SQR3_SQ4_Pos)        /*!< 0x00080000 */

#define ADC_SQR3_SQ5_Pos                    (20U)                              
#define ADC_SQR3_SQ5_Msk                    (0x1FUL << ADC_SQR3_SQ5_Pos)        /*!< 0x01F00000 */
#define ADC_SQR3_SQ5                        ADC_SQR3_SQ5_Msk                   /*!< ADC group regular sequencer rank 5 */
#define ADC_SQR3_SQ5_0                      (0x01UL << ADC_SQR3_SQ5_Pos)        /*!< 0x00100000 */
#define ADC_SQR3_SQ5_1                      (0x02UL << ADC_SQR3_SQ5_Pos)        /*!< 0x00200000 */
#define ADC_SQR3_SQ5_2                      (0x04UL << ADC_SQR3_SQ5_Pos)        /*!< 0x00400000 */
#define ADC_SQR3_SQ5_3                      (0x08UL << ADC_SQR3_SQ5_Pos)        /*!< 0x00800000 */
#define ADC_SQR3_SQ5_4                      (0x10UL << ADC_SQR3_SQ5_Pos)        /*!< 0x01000000 */

#define ADC_SQR3_SQ6_Pos                    (25U)                              
#define ADC_SQR3_SQ6_Msk                    (0x1FUL << ADC_SQR3_SQ6_Pos)        /*!< 0x3E000000 */
#define ADC_SQR3_SQ6                        ADC_SQR3_SQ6_Msk                   /*!< ADC group regular sequencer rank 6 */
#define ADC_SQR3_SQ6_0                      (0x01UL << ADC_SQR3_SQ6_Pos)        /*!< 0x02000000 */
#define ADC_SQR3_SQ6_1                      (0x02UL << ADC_SQR3_SQ6_Pos)        /*!< 0x04000000 */
#define ADC_SQR3_SQ6_2                      (0x04UL << ADC_SQR3_SQ6_Pos)        /*!< 0x08000000 */
#define ADC_SQR3_SQ6_3                      (0x08UL << ADC_SQR3_SQ6_Pos)        /*!< 0x10000000 */
#define ADC_SQR3_SQ6_4                      (0x10UL << ADC_SQR3_SQ6_Pos)        /*!< 0x20000000 */

/*******************  Bit definition for ADC_JSQR register  *******************/
#define ADC_JSQR_JSQ1_Pos                   (0U)                               
#define ADC_JSQR_JSQ1_Msk                   (0x1FUL << ADC_JSQR_JSQ1_Pos)       /*!< 0x0000001F */
#define ADC_JSQR_JSQ1                       ADC_JSQR_JSQ1_Msk                  /*!< ADC group injected sequencer rank 1 */
#define ADC_JSQR_JSQ1_0                     (0x01UL << ADC_JSQR_JSQ1_Pos)       /*!< 0x00000001 */
#define ADC_JSQR_JSQ1_1                     (0x02UL << ADC_JSQR_JSQ1_Pos)       /*!< 0x00000002 */
#define ADC_JSQR_JSQ1_2                     (0x04UL << ADC_JSQR_JSQ1_Pos)       /*!< 0x00000004 */
#define ADC_JSQR_JSQ1_3                     (0x08UL << ADC_JSQR_JSQ1_Pos)       /*!< 0x00000008 */
#define ADC_JSQR_JSQ1_4                     (0x10UL << ADC_JSQR_JSQ1_Pos)       /*!< 0x00000010 */

#define ADC_JSQR_JSQ2_Pos                   (5U)                               
#define ADC_JSQR_JSQ2_Msk                   (0x1FUL << ADC_JSQR_JSQ2_Pos)       /*!< 0x000003E0 */
#define ADC_JSQR_JSQ2                       ADC_JSQR_JSQ2_Msk                  /*!< ADC group injected sequencer rank 2 */
#define ADC_JSQR_JSQ2_0                     (0x01UL << ADC_JSQR_JSQ2_Pos)       /*!< 0x00000020 */
#define ADC_JSQR_JSQ2_1                     (0x02UL << ADC_JSQR_JSQ2_Pos)       /*!< 0x00000040 */
#define ADC_JSQR_JSQ2_2                     (0x04UL << ADC_JSQR_JSQ2_Pos)       /*!< 0x00000080 */
#define ADC_JSQR_JSQ2_3                     (0x08UL << ADC_JSQR_JSQ2_Pos)       /*!< 0x00000100 */
#define ADC_JSQR_JSQ2_4                     (0x10UL << ADC_JSQR_JSQ2_Pos)       /*!< 0x00000200 */

#define ADC_JSQR_JSQ3_Pos                   (10U)                              
#define ADC_JSQR_JSQ3_Msk                   (0x1FUL << ADC_JSQR_JSQ3_Pos)       /*!< 0x00007C00 */
#define ADC_JSQR_JSQ3                       ADC_JSQR_JSQ3_Msk                  /*!< ADC group injected sequencer rank 3 */
#define ADC_JSQR_JSQ3_0                     (0x01UL << ADC_JSQR_JSQ3_Pos)       /*!< 0x00000400 */
#define ADC_JSQR_JSQ3_1                     (0x02UL << ADC_JSQR_JSQ3_Pos)       /*!< 0x00000800 */
#define ADC_JSQR_JSQ3_2                     (0x04UL << ADC_JSQR_JSQ3_Pos)       /*!< 0x00001000 */
#define ADC_JSQR_JSQ3_3                     (0x08UL << ADC_JSQR_JSQ3_Pos)       /*!< 0x00002000 */
#define ADC_JSQR_JSQ3_4                     (0x10UL << ADC_JSQR_JSQ3_Pos)       /*!< 0x00004000 */

#define ADC_JSQR_JSQ4_Pos                   (15U)                              
#define ADC_JSQR_JSQ4_Msk                   (0x1FUL << ADC_JSQR_JSQ4_Pos)       /*!< 0x000F8000 */
#define ADC_JSQR_JSQ4                       ADC_JSQR_JSQ4_Msk                  /*!< ADC group injected sequencer rank 4 */
#define ADC_JSQR_JSQ4_0                     (0x01UL << ADC_JSQR_JSQ4_Pos)       /*!< 0x00008000 */
#define ADC_JSQR_JSQ4_1                     (0x02UL << ADC_JSQR_JSQ4_Pos)       /*!< 0x00010000 */
#define ADC_JSQR_JSQ4_2                     (0x04UL << ADC_JSQR_JSQ4_Pos)       /*!< 0x00020000 */
#define ADC_JSQR_JSQ4_3                     (0x08UL << ADC_JSQR_JSQ4_Pos)       /*!< 0x00040000 */
#define ADC_JSQR_JSQ4_4                     (0x10UL << ADC_JSQR_JSQ4_Pos)       /*!< 0x00080000 */

#define ADC_JSQR_JL_Pos                     (20U)                              
#define ADC_JSQR_JL_Msk                     (0x3UL << ADC_JSQR_JL_Pos)          /*!< 0x00300000 */
#define ADC_JSQR_JL                         ADC_JSQR_JL_Msk                    /*!< ADC group injected sequencer scan length */
#define ADC_JSQR_JL_0                       (0x1UL << ADC_JSQR_JL_Pos)          /*!< 0x00100000 */
#define ADC_JSQR_JL_1                       (0x2UL << ADC_JSQR_JL_Pos)          /*!< 0x00200000 */

/*******************  Bit definition for ADC_JDR1 register  *******************/
#define ADC_JDR1_JDATA_Pos                  (0U)                               
#define ADC_JDR1_JDATA_Msk                  (0xFFFFUL << ADC_JDR1_JDATA_Pos)    /*!< 0x0000FFFF */
#define ADC_JDR1_JDATA                      ADC_JDR1_JDATA_Msk                 /*!< ADC group injected sequencer rank 1 conversion data */

/*******************  Bit definition for ADC_JDR2 register  *******************/
#define ADC_JDR2_JDATA_Pos                  (0U)                               
#define ADC_JDR2_JDATA_Msk                  (0xFFFFUL << ADC_JDR2_JDATA_Pos)    /*!< 0x0000FFFF */
#define ADC_JDR2_JDATA                      ADC_JDR2_JDATA_Msk                 /*!< ADC group injected sequencer rank 2 conversion data */

/*******************  Bit definition for ADC_JDR3 register  *******************/
#define ADC_JDR3_JDATA_Pos                  (0U)                               
#define ADC_JDR3_JDATA_Msk                  (0xFFFFUL << ADC_JDR3_JDATA_Pos)    /*!< 0x0000FFFF */
#define ADC_JDR3_JDATA                      ADC_JDR3_JDATA_Msk                 /*!< ADC group injected sequencer rank 3 conversion data */

/*******************  Bit definition for ADC_JDR4 register  *******************/
#define ADC_JDR4_JDATA_Pos                  (0U)                               
#define ADC_JDR4_JDATA_Msk                  (0xFFFFUL << ADC_JDR4_JDATA_Pos)    /*!< 0x0000FFFF */
#define ADC_JDR4_JDATA                      ADC_JDR4_JDATA_Msk                 /*!< ADC group injected sequencer rank 4 conversion data */

/********************  Bit definition for ADC_DR register  ********************/
#define ADC_DR_DATA_Pos                     (0U)                               
#define ADC_DR_DATA_Msk                     (0xFFFFUL << ADC_DR_DATA_Pos)       /*!< 0x0000FFFF */
#define ADC_DR_DATA                         ADC_DR_DATA_Msk                    /*!< ADC group regular conversion data */
#define ADC_DR_ADC2DATA_Pos                 (16U)                              
#define ADC_DR_ADC2DATA_Msk                 (0xFFFFUL << ADC_DR_ADC2DATA_Pos)   /*!< 0xFFFF0000 */
#define ADC_DR_ADC2DATA                     ADC_DR_ADC2DATA_Msk                /*!< ADC group regular conversion data for ADC slave, in multimode */


/* function declarations */
/* reset ADC */
void adc_deinit(uint32_t adc_periph);
/* enable ADC interface */
void adc_enable(uint32_t adc_periph);
/* disable ADC interface */
void adc_disable(uint32_t adc_periph);
/* ADC calibration and reset calibration */
void adc_calibration_enable(uint32_t adc_periph);
/* enable DMA request */
void adc_dma_mode_enable(uint32_t adc_periph);
/* disable DMA request */
void adc_dma_mode_disable(uint32_t adc_periph);
/* enable the temperature sensor and Vrefint channel */
void adc_tempsensor_vrefint_enable(void);
/* disable the temperature sensor and Vrefint channel */
void adc_tempsensor_vrefint_disable(void);

/* configure ADC resolution */
void adc_resolution_config(uint32_t adc_periph , uint32_t resolution);
/* configure ADC discontinuous mode */
void adc_discontinuous_mode_config(uint32_t adc_periph , uint8_t adc_channel_group , uint8_t length);

/* configure the ADC mode */
void adc_mode_config(uint32_t mode);
/* enable or disable ADC special function */
void adc_special_function_config(uint32_t adc_periph , uint32_t function , ControlStatus newvalue);
/* configure ADC data alignment */
void adc_data_alignment_config(uint32_t adc_periph , uint32_t data_alignment);
/* configure the length of regular channel group or inserted channel group */
void adc_channel_length_config(uint32_t adc_periph , uint8_t adc_channel_group , uint32_t length);
/* configure ADC regular channel */
void adc_regular_channel_config(uint32_t adc_periph , uint8_t rank , uint8_t adc_channel , uint32_t sample_time);
/* configure ADC inserted channel */
void adc_inserted_channel_config(uint32_t adc_periph , uint8_t rank , uint8_t adc_channel , uint32_t sample_time);
/* configure ADC inserted channel offset */
void adc_inserted_channel_offset_config(uint32_t adc_periph , uint8_t inserted_channel , uint16_t offset);
/* enable ADC external trigger */
void adc_external_trigger_config(uint32_t adc_periph, uint8_t adc_channel_group, ControlStatus newvalue);
/* configure ADC external trigger source */
void adc_external_trigger_source_config(uint32_t adc_periph, uint8_t adc_channel_group, uint32_t external_trigger_source);
/* enable ADC software trigger */
void adc_software_trigger_enable(uint32_t adc_periph , uint8_t adc_channel_group);

/* read ADC regular group data register */
uint16_t adc_regular_data_read(uint32_t adc_periph);
/* read ADC inserted group data register */
uint16_t adc_inserted_data_read(uint32_t adc_periph , uint8_t inserted_channel);
/* read the last ADC0 and ADC1 conversion result data in sync mode */
uint32_t adc_sync_mode_convert_value_read(void);

/* get the ADC flag bits */
FlagStatus adc_flag_get(uint32_t adc_periph , uint32_t adc_flag);
/* clear the ADC flag bits */
void adc_flag_clear(uint32_t adc_periph , uint32_t adc_flag);
/* get the ADC interrupt bits */
FlagStatus adc_interrupt_flag_get(uint32_t adc_periph , uint32_t adc_interrupt);
/* clear the ADC flag */
void adc_interrupt_flag_clear(uint32_t adc_periph , uint32_t adc_interrupt);
/* enable ADC interrupt */
void adc_interrupt_enable(uint32_t adc_periph , uint32_t adc_interrupt);
/* disable ADC interrupt */
void adc_interrupt_disable(uint32_t adc_periph , uint32_t adc_interrupt);

/* configure ADC analog watchdog single channel */
void adc_watchdog_single_channel_enable(uint32_t adc_periph, uint8_t adc_channel);
/* configure ADC analog watchdog group channel */
void adc_watchdog_group_channel_enable(uint32_t adc_periph, uint8_t adc_channel_group);
/* disable ADC analog watchdog */
void adc_watchdog_disable(uint32_t adc_periph);
/* configure ADC analog watchdog threshold */
void adc_watchdog_threshold_config(uint32_t adc_periph , uint16_t low_threshold , uint16_t high_threshold);

/* configure ADC oversample mode */
void adc_oversample_mode_config(uint32_t adc_periph , uint32_t mode , uint16_t shift , uint8_t ratio);
/* enable ADC oversample mode */
void adc_oversample_mode_enable(uint32_t adc_periph);
/* disable ADC oversample mode */
void adc_oversample_mode_disable(uint32_t adc_periph);
#endif /* GD32F30X_ADC_H */
