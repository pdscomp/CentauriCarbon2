/*!
    \file    gd32f30x_spi.h
    \brief   definitions for the SPI

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

#ifndef GD32F30X_SPI_H
#define GD32F30X_SPI_H

#include "gd32f30x.h"

/* SPIx(x=0,1,2) definitions */
#define SPI0                            (SPI_BASE + 0x0000F800U)
#define SPI1                            SPI_BASE
#define SPI2                            (SPI_BASE + 0x00000400U)

/* SPI registers definitions */
#define SPI_CTL0(spix)                  REG32((spix) + 0x00U)                   /*!< SPI control register 0 */
#define SPI_CTL1(spix)                  REG32((spix) + 0x04U)                   /*!< SPI control register 1*/
#define SPI_STAT(spix)                  REG32((spix) + 0x08U)                   /*!< SPI status register */
#define SPI_DATA(spix)                  REG32((spix) + 0x0CU)                   /*!< SPI data register */
#define SPI_CRCPOLY(spix)               REG32((spix) + 0x10U)                   /*!< SPI CRC polynomial register */
#define SPI_RCRC(spix)                  REG32((spix) + 0x14U)                   /*!< SPI receive CRC register */
#define SPI_TCRC(spix)                  REG32((spix) + 0x18U)                   /*!< SPI transmit CRC register */
#define SPI_I2SCTL(spix)                REG32((spix) + 0x1CU)                   /*!< SPI I2S control register */
#define SPI_I2SPSC(spix)                REG32((spix) + 0x20U)                   /*!< SPI I2S clock prescaler register */
#define SPI_QCTL(spix)                  REG32((spix) + 0x80U)                   /*!< SPI quad mode control register(only SPI0) */

/* bits definitions */
/* SPI_CTL0 */
#define SPI_CTL0_CKPH                   BIT(0)                                  /*!< clock phase selection*/
#define SPI_CTL0_CKPL                   BIT(1)                                  /*!< clock polarity selection */
#define SPI_CTL0_MSTMOD                 BIT(2)                                  /*!< master mode enable */
#define SPI_CTL0_PSC                    BITS(3,5)                               /*!< master clock prescaler selection */
#define SPI_CTL0_SPIEN                  BIT(6)                                  /*!< SPI enable*/
#define SPI_CTL0_LF                     BIT(7)                                  /*!< LSB first mode */
#define SPI_CTL0_SWNSS                  BIT(8)                                  /*!< NSS pin selection in NSS software mode */
#define SPI_CTL0_SWNSSEN                BIT(9)                                  /*!< NSS software mode selection */
#define SPI_CTL0_RO                     BIT(10)                                 /*!< receive only */
#define SPI_CTL0_FF16                   BIT(11)                                 /*!< data frame size */
#define SPI_CTL0_CRCNT                  BIT(12)                                 /*!< CRC next transfer */
#define SPI_CTL0_CRCEN                  BIT(13)                                 /*!< CRC calculation enable */
#define SPI_CTL0_BDOEN                  BIT(14)                                 /*!< bidirectional transmit output enable*/
#define SPI_CTL0_BDEN                   BIT(15)                                 /*!< bidirectional enable */

/* SPI_CTL1 */
#define SPI_CTL1_DMAREN                 BIT(0)                                  /*!< receive buffer dma enable */
#define SPI_CTL1_DMATEN                 BIT(1)                                  /*!< transmit buffer dma enable */
#define SPI_CTL1_NSSDRV                 BIT(2)                                  /*!< drive NSS output */
#define SPI_CTL1_NSSP                   BIT(3)                                  /*!< SPI NSS pulse mode enable */
#define SPI_CTL1_TMOD                   BIT(4)                                  /*!< SPI TI mode enable */
#define SPI_CTL1_ERRIE                  BIT(5)                                  /*!< errors interrupt enable */
#define SPI_CTL1_RBNEIE                 BIT(6)                                  /*!< receive buffer not empty interrupt enable */
#define SPI_CTL1_TBEIE                  BIT(7)                                  /*!< transmit buffer empty interrupt enable */

/* SPI_STAT */
#define SPI_STAT_RBNE                   BIT(0)                                  /*!< receive buffer not empty */
#define SPI_STAT_TBE                    BIT(1)                                  /*!< transmit buffer empty */
#define SPI_STAT_I2SCH                  BIT(2)                                  /*!< I2S channel side */
#define SPI_STAT_TXURERR                BIT(3)                                  /*!< I2S transmission underrun error bit */
#define SPI_STAT_CRCERR                 BIT(4)                                  /*!< SPI CRC error bit */
#define SPI_STAT_CONFERR                BIT(5)                                  /*!< SPI configuration error bit */
#define SPI_STAT_RXORERR                BIT(6)                                  /*!< SPI reception overrun error bit */
#define SPI_STAT_TRANS                  BIT(7)                                  /*!< transmitting on-going bit */
#define SPI_STAT_FERR                   BIT(8)                                  /*!< format error bit */

/* SPI_DATA */
#define SPI_DATA_DATA                   BITS(0,15)                              /*!< data transfer register */

/* SPI_CRCPOLY */
#define SPI_CRCPOLY_CPR                 BITS(0,15)                              /*!< CRC polynomial register */

/* SPI_RCRC */
#define SPI_RCRC_RCR                    BITS(0,15)                              /*!< RX CRC register */

/* SPI_TCRC */
#define SPI_TCRC_TCR                    BITS(0,15)                              /*!< TX CRC register */

/* SPI_I2SCTL */
#define SPI_I2SCTL_CHLEN                BIT(0)                                  /*!< channel length */
#define SPI_I2SCTL_DTLEN                BITS(1,2)                               /*!< data length */
#define SPI_I2SCTL_CKPL                 BIT(3)                                  /*!< idle state clock polarity */
#define SPI_I2SCTL_I2SSTD               BITS(4,5)                               /*!< I2S standard selection */
#define SPI_I2SCTL_PCMSMOD              BIT(7)                                  /*!< PCM frame synchronization mode */
#define SPI_I2SCTL_I2SOPMOD             BITS(8,9)                               /*!< I2S operation mode */
#define SPI_I2SCTL_I2SEN                BIT(10)                                 /*!< I2S enable */
#define SPI_I2SCTL_I2SSEL               BIT(11)                                 /*!< I2S mode selection */

/* SPI_I2SPSC */
#define SPI_I2SPSC_DIV                  BITS(0,7)                               /*!< dividing factor for the prescaler */
#define SPI_I2SPSC_OF                   BIT(8)                                  /*!< odd factor for the prescaler */
#define SPI_I2SPSC_MCKOEN               BIT(9)                                  /*!< I2S MCK output enable */

/* SPI_QCTL(only for SPI0) */
#define SPI_QCTL_QMOD                   BIT(0)                                  /*!< quad-SPI mode enable */
#define SPI_QCTL_QRD                    BIT(1)                                  /*!< quad-SPI mode read select */
#define SPI_QCTL_IO23_DRV               BIT(2)                                  /*!< drive SPI_IO2 and SPI_IO3 enable */

/* constants definitions */
/* SPI and I2S parameter struct definitions */
typedef struct
{   
    uint32_t device_mode;                                                       /*!< SPI master or slave */
    uint32_t trans_mode;                                                        /*!< SPI transtype */
    uint32_t frame_size;                                                        /*!< SPI frame size */
    uint32_t nss;                                                               /*!< SPI NSS control by handware or software */
    uint32_t endian;                                                            /*!< SPI big endian or little endian */
    uint32_t clock_polarity_phase;                                              /*!< SPI clock phase and polarity */
    uint32_t prescale;                                                          /*!< SPI prescale factor */
}spi_parameter_struct;

/* SPI mode definitions */
#define SPI_MASTER                      (SPI_CTL0_MSTMOD | SPI_CTL0_SWNSS)      /*!< SPI as master */
#define SPI_SLAVE                       ((uint32_t)0x00000000U)                 /*!< SPI as slave */

/* SPI bidirectional transfer direction */
#define SPI_BIDIRECTIONAL_TRANSMIT      SPI_CTL0_BDOEN                          /*!< SPI work in transmit-only mode */
#define SPI_BIDIRECTIONAL_RECEIVE       (~SPI_CTL0_BDOEN)                       /*!< SPI work in receive-only mode */

/* SPI transmit type */
#define SPI_TRANSMODE_FULLDUPLEX        ((uint32_t)0x00000000U)                 /*!< SPI receive and send data at fullduplex communication */
#define SPI_TRANSMODE_RECEIVEONLY       SPI_CTL0_RO                             /*!< SPI only receive data */
#define SPI_TRANSMODE_BDRECEIVE         SPI_CTL0_BDEN                           /*!< bidirectional receive data */
#define SPI_TRANSMODE_BDTRANSMIT        (SPI_CTL0_BDEN | SPI_CTL0_BDOEN)        /*!< bidirectional transmit data*/

/* SPI frame size */
#define SPI_FRAMESIZE_16BIT             SPI_CTL0_FF16                           /*!< SPI frame size is 16 bits */
#define SPI_FRAMESIZE_8BIT              ((uint32_t)0x00000000U)                 /*!< SPI frame size is 8 bits */

/* SPI NSS control mode */
#define SPI_NSS_SOFT                    SPI_CTL0_SWNSSEN                        /*!< SPI NSS control by sofrware */
#define SPI_NSS_HARD                    ((uint32_t)0x00000000U)                 /*!< SPI NSS control by hardware */

/* SPI transmit way */
#define SPI_ENDIAN_MSB                  ((uint32_t)0x00000000U)                 /*!< SPI transmit way is big endian: transmit MSB first */
#define SPI_ENDIAN_LSB                  SPI_CTL0_LF                             /*!< SPI transmit way is little endian: transmit LSB first */

/* SPI clock phase and polarity */
#define SPI_CK_PL_LOW_PH_1EDGE          ((uint32_t)0x00000000U)                 /*!< SPI clock polarity is low level and phase is first edge */
#define SPI_CK_PL_HIGH_PH_1EDGE         SPI_CTL0_CKPL                           /*!< SPI clock polarity is high level and phase is first edge */
#define SPI_CK_PL_LOW_PH_2EDGE          SPI_CTL0_CKPH                           /*!< SPI clock polarity is low level and phase is second edge */
#define SPI_CK_PL_HIGH_PH_2EDGE         (SPI_CTL0_CKPL | SPI_CTL0_CKPH)         /*!< SPI clock polarity is high level and phase is second edge */

/* SPI clock prescale factor */
#define CTL0_PSC(regval)                (BITS(3,5) & ((uint32_t)(regval) << 3))
#define SPI_PSC_2                       CTL0_PSC(0)                             /*!< SPI clock prescale factor is 2 */
#define SPI_PSC_4                       CTL0_PSC(1)                             /*!< SPI clock prescale factor is 4 */
#define SPI_PSC_8                       CTL0_PSC(2)                             /*!< SPI clock prescale factor is 8 */
#define SPI_PSC_16                      CTL0_PSC(3)                             /*!< SPI clock prescale factor is 16 */
#define SPI_PSC_32                      CTL0_PSC(4)                             /*!< SPI clock prescale factor is 32 */
#define SPI_PSC_64                      CTL0_PSC(5)                             /*!< SPI clock prescale factor is 64 */
#define SPI_PSC_128                     CTL0_PSC(6)                             /*!< SPI clock prescale factor is 128 */
#define SPI_PSC_256                     CTL0_PSC(7)                             /*!< SPI clock prescale factor is 256 */

/* I2S audio sample rate */
#define I2S_AUDIOSAMPLE_8K              ((uint32_t)8000U)                       /*!< I2S audio sample rate is 8KHz */
#define I2S_AUDIOSAMPLE_11K             ((uint32_t)11025U)                      /*!< I2S audio sample rate is 11KHz */
#define I2S_AUDIOSAMPLE_16K             ((uint32_t)16000U)                      /*!< I2S audio sample rate is 16KHz */
#define I2S_AUDIOSAMPLE_22K             ((uint32_t)22050U)                      /*!< I2S audio sample rate is 22KHz */
#define I2S_AUDIOSAMPLE_32K             ((uint32_t)32000U)                      /*!< I2S audio sample rate is 32KHz */
#define I2S_AUDIOSAMPLE_44K             ((uint32_t)44100U)                      /*!< I2S audio sample rate is 44KHz */
#define I2S_AUDIOSAMPLE_48K             ((uint32_t)48000U)                      /*!< I2S audio sample rate is 48KHz */
#define I2S_AUDIOSAMPLE_96K             ((uint32_t)96000U)                      /*!< I2S audio sample rate is 96KHz */
#define I2S_AUDIOSAMPLE_192K            ((uint32_t)192000U)                     /*!< I2S audio sample rate is 192KHz */

/* I2S frame format */
#define I2SCTL_DTLEN(regval)            (BITS(1,2) & ((uint32_t)(regval) << 1))
#define I2S_FRAMEFORMAT_DT16B_CH16B     I2SCTL_DTLEN(0)                         /*!< I2S data length is 16 bit and channel length is 16 bit */
#define I2S_FRAMEFORMAT_DT16B_CH32B     (I2SCTL_DTLEN(0) | SPI_I2SCTL_CHLEN)    /*!< I2S data length is 16 bit and channel length is 32 bit */
#define I2S_FRAMEFORMAT_DT24B_CH32B     (I2SCTL_DTLEN(1) | SPI_I2SCTL_CHLEN)    /*!< I2S data length is 24 bit and channel length is 32 bit */
#define I2S_FRAMEFORMAT_DT32B_CH32B     (I2SCTL_DTLEN(2) | SPI_I2SCTL_CHLEN)    /*!< I2S data length is 32 bit and channel length is 32 bit */

/* I2S master clock output */
#define I2S_MCKOUT_DISABLE              ((uint32_t)0x00000000U)                 /*!< I2S master clock output disable */
#define I2S_MCKOUT_ENABLE               SPI_I2SPSC_MCKOEN                       /*!< I2S master clock output enable */

/* I2S operation mode */
#define I2SCTL_I2SOPMOD(regval)         (BITS(8,9) & ((uint32_t)(regval) << 8))
#define I2S_MODE_SLAVETX                I2SCTL_I2SOPMOD(0)                      /*!< I2S slave transmit mode */
#define I2S_MODE_SLAVERX                I2SCTL_I2SOPMOD(1)                      /*!< I2S slave receive mode */
#define I2S_MODE_MASTERTX               I2SCTL_I2SOPMOD(2)                      /*!< I2S master transmit mode */
#define I2S_MODE_MASTERRX               I2SCTL_I2SOPMOD(3)                      /*!< I2S master receive mode */

/* I2S standard */
#define I2SCTL_I2SSTD(regval)           (BITS(4,5) & ((uint32_t)(regval) << 4))
#define I2S_STD_PHILLIPS                I2SCTL_I2SSTD(0)                        /*!< I2S phillips standard */
#define I2S_STD_MSB                     I2SCTL_I2SSTD(1)                        /*!< I2S MSB standard */
#define I2S_STD_LSB                     I2SCTL_I2SSTD(2)                        /*!< I2S LSB standard */
#define I2S_STD_PCMSHORT                I2SCTL_I2SSTD(3)                        /*!< I2S PCM short standard */
#define I2S_STD_PCMLONG                 (I2SCTL_I2SSTD(3) | SPI_I2SCTL_PCMSMOD) /*!< I2S PCM long standard */

/* I2S clock polarity */
#define I2S_CKPL_LOW                    ((uint32_t)0x00000000U)                 /*!< I2S clock polarity low level */
#define I2S_CKPL_HIGH                   SPI_I2SCTL_CKPL                         /*!< I2S clock polarity high level */

/* SPI DMA constants definitions */                                    
#define SPI_DMA_TRANSMIT                ((uint8_t)0x00U)                        /*!< SPI transmit data use DMA */
#define SPI_DMA_RECEIVE                 ((uint8_t)0x01U)                        /*!< SPI receive data use DMA */

/* SPI CRC constants definitions */
#define SPI_CRC_TX                      ((uint8_t)0x00U)                        /*!< SPI transmit CRC value */
#define SPI_CRC_RX                      ((uint8_t)0x01U)                        /*!< SPI receive CRC value */

/* SPI/I2S interrupt enable/disable constants definitions */
#define SPI_I2S_INT_TBE                 ((uint8_t)0x00U)                        /*!< transmit buffer empty interrupt */
#define SPI_I2S_INT_RBNE                ((uint8_t)0x01U)                        /*!< receive buffer not empty interrupt */
#define SPI_I2S_INT_ERR                 ((uint8_t)0x02U)                        /*!< error interrupt */

/* SPI/I2S interrupt flag constants definitions */
#define SPI_I2S_INT_FLAG_TBE            ((uint8_t)0x00U)                        /*!< transmit buffer empty interrupt flag */
#define SPI_I2S_INT_FLAG_RBNE           ((uint8_t)0x01U)                        /*!< receive buffer not empty interrupt flag */
#define SPI_I2S_INT_FLAG_RXORERR        ((uint8_t)0x02U)                        /*!< overrun interrupt flag */
#define SPI_INT_FLAG_CONFERR            ((uint8_t)0x03U)                        /*!< config error interrupt flag */
#define SPI_INT_FLAG_CRCERR             ((uint8_t)0x04U)                        /*!< CRC error interrupt flag */
#define I2S_INT_FLAG_TXURERR            ((uint8_t)0x05U)                        /*!< underrun error interrupt flag */
#define SPI_I2S_INT_FLAG_FERR           ((uint8_t)0x06U)                        /*!< format error interrupt flag */

/* SPI/I2S flag definitions */                                                  
#define SPI_FLAG_RBNE                   SPI_STAT_RBNE                           /*!< receive buffer not empty flag */
#define SPI_FLAG_TBE                    SPI_STAT_TBE                            /*!< transmit buffer empty flag */
#define SPI_FLAG_CRCERR                 SPI_STAT_CRCERR                         /*!< CRC error flag */
#define SPI_FLAG_CONFERR                SPI_STAT_CONFERR                        /*!< mode config error flag */
#define SPI_FLAG_RXORERR                SPI_STAT_RXORERR                        /*!< receive overrun error flag */
#define SPI_FLAG_TRANS                  SPI_STAT_TRANS                          /*!< transmit on-going flag */
#define SPI_FLAG_FERR                   SPI_STAT_FERR                           /*!< format error flag */
#define I2S_FLAG_RBNE                   SPI_STAT_RBNE                           /*!< receive buffer not empty flag */
#define I2S_FLAG_TBE                    SPI_STAT_TBE                            /*!< transmit buffer empty flag */
#define I2S_FLAG_CH                     SPI_STAT_I2SCH                          /*!< channel side flag */
#define I2S_FLAG_TXURERR                SPI_STAT_TXURERR                        /*!< underrun error flag */
#define I2S_FLAG_RXORERR                SPI_STAT_RXORERR                        /*!< overrun error flag */
#define I2S_FLAG_TRANS                  SPI_STAT_TRANS                          /*!< transmit on-going flag */
#define I2S_FLAG_FERR                   SPI_STAT_FERR                           /*!< format error flag */


/*****************************************************************************/
/*                                                                           */
/*                        Serial Peripheral Interface (SPI)                  */
/*                                                                           */
/*****************************************************************************/

/*
 * @brief Specific device feature definitions (not present on all devices in the STM32F0 serie)
 */
/* Note: No specific macro feature on this device */

/*******************  Bit definition for SPI_CR1 register  *******************/
#define SPI_CR1_CPHA_Pos            (0U)                                       
#define SPI_CR1_CPHA_Msk            (0x1U << SPI_CR1_CPHA_Pos)                 /*!< 0x00000001 */
#define SPI_CR1_CPHA                SPI_CR1_CPHA_Msk                           /*!< Clock Phase */
#define SPI_CR1_CPOL_Pos            (1U)                                       
#define SPI_CR1_CPOL_Msk            (0x1U << SPI_CR1_CPOL_Pos)                 /*!< 0x00000002 */
#define SPI_CR1_CPOL                SPI_CR1_CPOL_Msk                           /*!< Clock Polarity */
#define SPI_CR1_MSTR_Pos            (2U)                                       
#define SPI_CR1_MSTR_Msk            (0x1U << SPI_CR1_MSTR_Pos)                 /*!< 0x00000004 */
#define SPI_CR1_MSTR                SPI_CR1_MSTR_Msk                           /*!< Master Selection */
#define SPI_CR1_BR_Pos              (3U)                                       
#define SPI_CR1_BR_Msk              (0x7U << SPI_CR1_BR_Pos)                   /*!< 0x00000038 */
#define SPI_CR1_BR                  SPI_CR1_BR_Msk                             /*!< BR[2:0] bits (Baud Rate Control) */
#define SPI_CR1_BR_0                (0x1U << SPI_CR1_BR_Pos)                   /*!< 0x00000008 */
#define SPI_CR1_BR_1                (0x2U << SPI_CR1_BR_Pos)                   /*!< 0x00000010 */
#define SPI_CR1_BR_2                (0x4U << SPI_CR1_BR_Pos)                   /*!< 0x00000020 */
#define SPI_CR1_SPE_Pos             (6U)                                       
#define SPI_CR1_SPE_Msk             (0x1U << SPI_CR1_SPE_Pos)                  /*!< 0x00000040 */
#define SPI_CR1_SPE                 SPI_CR1_SPE_Msk                            /*!< SPI Enable */
#define SPI_CR1_LSBFIRST_Pos        (7U)                                       
#define SPI_CR1_LSBFIRST_Msk        (0x1U << SPI_CR1_LSBFIRST_Pos)             /*!< 0x00000080 */
#define SPI_CR1_LSBFIRST            SPI_CR1_LSBFIRST_Msk                       /*!< Frame Format */
#define SPI_CR1_SSI_Pos             (8U)                                       
#define SPI_CR1_SSI_Msk             (0x1U << SPI_CR1_SSI_Pos)                  /*!< 0x00000100 */
#define SPI_CR1_SSI                 SPI_CR1_SSI_Msk                            /*!< Internal slave select */
#define SPI_CR1_SSM_Pos             (9U)                                       
#define SPI_CR1_SSM_Msk             (0x1U << SPI_CR1_SSM_Pos)                  /*!< 0x00000200 */
#define SPI_CR1_SSM                 SPI_CR1_SSM_Msk                            /*!< Software slave management */
#define SPI_CR1_RXONLY_Pos          (10U)                                      
#define SPI_CR1_RXONLY_Msk          (0x1U << SPI_CR1_RXONLY_Pos)               /*!< 0x00000400 */
#define SPI_CR1_RXONLY              SPI_CR1_RXONLY_Msk                         /*!< Receive only */
#define SPI_CR1_CRCL_Pos            (11U)                                      
#define SPI_CR1_CRCL_Msk            (0x1U << SPI_CR1_CRCL_Pos)                 /*!< 0x00000800 */
#define SPI_CR1_CRCL                SPI_CR1_CRCL_Msk                           /*!< CRC Length */
#define SPI_CR1_CRCNEXT_Pos         (12U)                                      
#define SPI_CR1_CRCNEXT_Msk         (0x1U << SPI_CR1_CRCNEXT_Pos)              /*!< 0x00001000 */
#define SPI_CR1_CRCNEXT             SPI_CR1_CRCNEXT_Msk                        /*!< Transmit CRC next */
#define SPI_CR1_CRCEN_Pos           (13U)                                      
#define SPI_CR1_CRCEN_Msk           (0x1U << SPI_CR1_CRCEN_Pos)                /*!< 0x00002000 */
#define SPI_CR1_CRCEN               SPI_CR1_CRCEN_Msk                          /*!< Hardware CRC calculation enable */
#define SPI_CR1_BIDIOE_Pos          (14U)                                      
#define SPI_CR1_BIDIOE_Msk          (0x1U << SPI_CR1_BIDIOE_Pos)               /*!< 0x00004000 */
#define SPI_CR1_BIDIOE              SPI_CR1_BIDIOE_Msk                         /*!< Output enable in bidirectional mode */
#define SPI_CR1_BIDIMODE_Pos        (15U)                                      
#define SPI_CR1_BIDIMODE_Msk        (0x1U << SPI_CR1_BIDIMODE_Pos)             /*!< 0x00008000 */
#define SPI_CR1_BIDIMODE            SPI_CR1_BIDIMODE_Msk                       /*!< Bidirectional data mode enable */

/*******************  Bit definition for SPI_CR2 register  *******************/
#define SPI_CR2_RXDMAEN_Pos         (0U)                                       
#define SPI_CR2_RXDMAEN_Msk         (0x1U << SPI_CR2_RXDMAEN_Pos)              /*!< 0x00000001 */
#define SPI_CR2_RXDMAEN             SPI_CR2_RXDMAEN_Msk                        /*!< Rx Buffer DMA Enable */
#define SPI_CR2_TXDMAEN_Pos         (1U)                                       
#define SPI_CR2_TXDMAEN_Msk         (0x1U << SPI_CR2_TXDMAEN_Pos)              /*!< 0x00000002 */
#define SPI_CR2_TXDMAEN             SPI_CR2_TXDMAEN_Msk                        /*!< Tx Buffer DMA Enable */
#define SPI_CR2_SSOE_Pos            (2U)                                       
#define SPI_CR2_SSOE_Msk            (0x1U << SPI_CR2_SSOE_Pos)                 /*!< 0x00000004 */
#define SPI_CR2_SSOE                SPI_CR2_SSOE_Msk                           /*!< SS Output Enable */
#define SPI_CR2_NSSP_Pos            (3U)                                       
#define SPI_CR2_NSSP_Msk            (0x1U << SPI_CR2_NSSP_Pos)                 /*!< 0x00000008 */
#define SPI_CR2_NSSP                SPI_CR2_NSSP_Msk                           /*!< NSS pulse management Enable */
#define SPI_CR2_FRF_Pos             (4U)                                       
#define SPI_CR2_FRF_Msk             (0x1U << SPI_CR2_FRF_Pos)                  /*!< 0x00000010 */
#define SPI_CR2_FRF                 SPI_CR2_FRF_Msk                            /*!< Frame Format Enable */
#define SPI_CR2_ERRIE_Pos           (5U)                                       
#define SPI_CR2_ERRIE_Msk           (0x1U << SPI_CR2_ERRIE_Pos)                /*!< 0x00000020 */
#define SPI_CR2_ERRIE               SPI_CR2_ERRIE_Msk                          /*!< Error Interrupt Enable */
#define SPI_CR2_RXNEIE_Pos          (6U)                                       
#define SPI_CR2_RXNEIE_Msk          (0x1U << SPI_CR2_RXNEIE_Pos)               /*!< 0x00000040 */
#define SPI_CR2_RXNEIE              SPI_CR2_RXNEIE_Msk                         /*!< RX buffer Not Empty Interrupt Enable */
#define SPI_CR2_TXEIE_Pos           (7U)                                       
#define SPI_CR2_TXEIE_Msk           (0x1U << SPI_CR2_TXEIE_Pos)                /*!< 0x00000080 */
#define SPI_CR2_TXEIE               SPI_CR2_TXEIE_Msk                          /*!< Tx buffer Empty Interrupt Enable */
#define SPI_CR2_DS_Pos              (8U)                                       
#define SPI_CR2_DS_Msk              (0xFU << SPI_CR2_DS_Pos)                   /*!< 0x00000F00 */
#define SPI_CR2_DS                  SPI_CR2_DS_Msk                             /*!< DS[3:0] Data Size */
#define SPI_CR2_DS_0                (0x1U << SPI_CR2_DS_Pos)                   /*!< 0x00000100 */
#define SPI_CR2_DS_1                (0x2U << SPI_CR2_DS_Pos)                   /*!< 0x00000200 */
#define SPI_CR2_DS_2                (0x4U << SPI_CR2_DS_Pos)                   /*!< 0x00000400 */
#define SPI_CR2_DS_3                (0x8U << SPI_CR2_DS_Pos)                   /*!< 0x00000800 */
#define SPI_CR2_FRXTH_Pos           (12U)                                      
#define SPI_CR2_FRXTH_Msk           (0x1U << SPI_CR2_FRXTH_Pos)                /*!< 0x00001000 */
#define SPI_CR2_FRXTH               SPI_CR2_FRXTH_Msk                          /*!< FIFO reception Threshold */
#define SPI_CR2_LDMARX_Pos          (13U)                                      
#define SPI_CR2_LDMARX_Msk          (0x1U << SPI_CR2_LDMARX_Pos)               /*!< 0x00002000 */
#define SPI_CR2_LDMARX              SPI_CR2_LDMARX_Msk                         /*!< Last DMA transfer for reception */
#define SPI_CR2_LDMATX_Pos          (14U)                                      
#define SPI_CR2_LDMATX_Msk          (0x1U << SPI_CR2_LDMATX_Pos)               /*!< 0x00004000 */
#define SPI_CR2_LDMATX              SPI_CR2_LDMATX_Msk                         /*!< Last DMA transfer for transmission */

/********************  Bit definition for SPI_SR register  *******************/
#define SPI_SR_RXNE_Pos             (0U)                                       
#define SPI_SR_RXNE_Msk             (0x1U << SPI_SR_RXNE_Pos)                  /*!< 0x00000001 */
#define SPI_SR_RXNE                 SPI_SR_RXNE_Msk                            /*!< Receive buffer Not Empty */
#define SPI_SR_TXE_Pos              (1U)                                       
#define SPI_SR_TXE_Msk              (0x1U << SPI_SR_TXE_Pos)                   /*!< 0x00000002 */
#define SPI_SR_TXE                  SPI_SR_TXE_Msk                             /*!< Transmit buffer Empty */
#define SPI_SR_CRCERR_Pos           (4U)                                       
#define SPI_SR_CRCERR_Msk           (0x1U << SPI_SR_CRCERR_Pos)                /*!< 0x00000010 */
#define SPI_SR_CRCERR               SPI_SR_CRCERR_Msk                          /*!< CRC Error flag */
#define SPI_SR_MODF_Pos             (5U)                                       
#define SPI_SR_MODF_Msk             (0x1U << SPI_SR_MODF_Pos)                  /*!< 0x00000020 */
#define SPI_SR_MODF                 SPI_SR_MODF_Msk                            /*!< Mode fault */
#define SPI_SR_OVR_Pos              (6U)                                       
#define SPI_SR_OVR_Msk              (0x1U << SPI_SR_OVR_Pos)                   /*!< 0x00000040 */
#define SPI_SR_OVR                  SPI_SR_OVR_Msk                             /*!< Overrun flag */
#define SPI_SR_BSY_Pos              (7U)                                       
#define SPI_SR_BSY_Msk              (0x1U << SPI_SR_BSY_Pos)                   /*!< 0x00000080 */
#define SPI_SR_BSY                  SPI_SR_BSY_Msk                             /*!< Busy flag */
#define SPI_SR_FRE_Pos              (8U)                                       
#define SPI_SR_FRE_Msk              (0x1U << SPI_SR_FRE_Pos)                   /*!< 0x00000100 */
#define SPI_SR_FRE                  SPI_SR_FRE_Msk                             /*!< TI frame format error */
#define SPI_SR_FRLVL_Pos            (9U)                                       
#define SPI_SR_FRLVL_Msk            (0x3U << SPI_SR_FRLVL_Pos)                 /*!< 0x00000600 */
#define SPI_SR_FRLVL                SPI_SR_FRLVL_Msk                           /*!< FIFO Reception Level */
#define SPI_SR_FRLVL_0              (0x1U << SPI_SR_FRLVL_Pos)                 /*!< 0x00000200 */
#define SPI_SR_FRLVL_1              (0x2U << SPI_SR_FRLVL_Pos)                 /*!< 0x00000400 */
#define SPI_SR_FTLVL_Pos            (11U)                                      
#define SPI_SR_FTLVL_Msk            (0x3U << SPI_SR_FTLVL_Pos)                 /*!< 0x00001800 */
#define SPI_SR_FTLVL                SPI_SR_FTLVL_Msk                           /*!< FIFO Transmission Level */
#define SPI_SR_FTLVL_0              (0x1U << SPI_SR_FTLVL_Pos)                 /*!< 0x00000800 */
#define SPI_SR_FTLVL_1              (0x2U << SPI_SR_FTLVL_Pos)                 /*!< 0x00001000 */

/********************  Bit definition for SPI_DR register  *******************/
#define SPI_DR_DR_Pos               (0U)                                       
#define SPI_DR_DR_Msk               (0xFFFFFFFFU << SPI_DR_DR_Pos)             /*!< 0xFFFFFFFF */
#define SPI_DR_DR                   SPI_DR_DR_Msk                              /*!< Data Register */

/*******************  Bit definition for SPI_CRCPR register  *****************/
#define SPI_CRCPR_CRCPOLY_Pos       (0U)                                       
#define SPI_CRCPR_CRCPOLY_Msk       (0xFFFFFFFFU << SPI_CRCPR_CRCPOLY_Pos)     /*!< 0xFFFFFFFF */
#define SPI_CRCPR_CRCPOLY           SPI_CRCPR_CRCPOLY_Msk                      /*!< CRC polynomial register */

/******************  Bit definition for SPI_RXCRCR register  *****************/
#define SPI_RXCRCR_RXCRC_Pos        (0U)                                       
#define SPI_RXCRCR_RXCRC_Msk        (0xFFFFFFFFU << SPI_RXCRCR_RXCRC_Pos)      /*!< 0xFFFFFFFF */
#define SPI_RXCRCR_RXCRC            SPI_RXCRCR_RXCRC_Msk                       /*!< Rx CRC Register */

/******************  Bit definition for SPI_TXCRCR register  *****************/
#define SPI_TXCRCR_TXCRC_Pos        (0U)                                       
#define SPI_TXCRCR_TXCRC_Msk        (0xFFFFFFFFU << SPI_TXCRCR_TXCRC_Pos)      /*!< 0xFFFFFFFF */
#define SPI_TXCRCR_TXCRC            SPI_TXCRCR_TXCRC_Msk                       /*!< Tx CRC Register */

/******************  Bit definition for SPI_I2SCFGR register  ****************/
#define SPI_I2SCFGR_I2SMOD_Pos      (11U)                                      
#define SPI_I2SCFGR_I2SMOD_Msk      (0x1U << SPI_I2SCFGR_I2SMOD_Pos)           /*!< 0x00000800 */
#define SPI_I2SCFGR_I2SMOD          SPI_I2SCFGR_I2SMOD_Msk                     /*!< Keep for compatibility */

/* function declarations */
/* SPI/I2S deinitialization and initialization functions */
/* reset SPI and I2S */
void spi_i2s_deinit(uint32_t spi_periph);
/* initialize the parameters of SPI struct with the default values */
void spi_struct_para_init(spi_parameter_struct* spi_struct);
/* initialize SPI parameter */
void spi_init(uint32_t spi_periph, spi_parameter_struct* spi_struct);
/* enable SPI */
void spi_enable(uint32_t spi_periph);
/* disable SPI */
void spi_disable(uint32_t spi_periph);

/* initialize I2S parameter */
void i2s_init(uint32_t spi_periph, uint32_t i2s_mode, uint32_t i2s_standard, uint32_t i2s_ckpl);
/* configure I2S prescaler */
void i2s_psc_config(uint32_t spi_periph, uint32_t i2s_audiosample, uint32_t i2s_frameformat, uint32_t i2s_mckout);
/* enable I2S */
void i2s_enable(uint32_t spi_periph);
/* disable I2S */
void i2s_disable(uint32_t spi_periph);

/* NSS functions */
/* enable SPI NSS output */
void spi_nss_output_enable(uint32_t spi_periph);
/* disable SPI NSS output */
void spi_nss_output_disable(uint32_t spi_periph);
/* SPI NSS pin high level in software mode */
void spi_nss_internal_high(uint32_t spi_periph);
/* SPI NSS pin low level in software mode */
void spi_nss_internal_low(uint32_t spi_periph);

/* SPI DMA functions */
/* enable SPI DMA */
void spi_dma_enable(uint32_t spi_periph, uint8_t dma);
/* disable SPI DMA */
void spi_dma_disable(uint32_t spi_periph, uint8_t dma);

/* SPI/I2S transfer functions */
/* configure SPI/I2S data frame format */
void spi_i2s_data_frame_format_config(uint32_t spi_periph, uint16_t frame_format);
/* SPI transmit data */
void spi_i2s_data_transmit(uint32_t spi_periph, uint16_t data);
/* SPI receive data */
uint16_t spi_i2s_data_receive(uint32_t spi_periph);
/* configure SPI bidirectional transfer direction */
void spi_bidirectional_transfer_config(uint32_t spi_periph, uint32_t transfer_direction);
/* clear TI Mode Format Error flag status */
void spi_i2s_format_error_clear(uint32_t spi_periph, uint32_t flag);


/* SPI CRC functions */
/* set SPI CRC polynomial */
void spi_crc_polynomial_set(uint32_t spi_periph, uint16_t crc_poly);
/* get SPI CRC polynomial */
uint16_t spi_crc_polynomial_get(uint32_t spi_periph);
/* turn on SPI CRC function */
void spi_crc_on(uint32_t spi_periph);
/* turn off SPI CRC function */
void spi_crc_off(uint32_t spi_periph);
/* SPI next data is CRC value */
void spi_crc_next(uint32_t spi_periph);
/* get SPI CRC send value or receive value */
uint16_t spi_crc_get(uint32_t spi_periph, uint8_t crc);
/* clear SPI CRC error flag status */
void spi_crc_error_clear(uint32_t spi_periph);

/* SPI TI mode functions */
/* enable SPI TI mode */
void spi_ti_mode_enable(uint32_t spi_periph);
/* disable SPI TI mode */
void spi_ti_mode_disable(uint32_t spi_periph);

/* SPI NSS pulse mode functions */
/* enable SPI NSS pulse mode */
void spi_nssp_mode_enable(uint32_t spi_periph);
/* disable SPI NSS pulse mode */
void spi_nssp_mode_disable(uint32_t spi_periph);

/* quad wire SPI functions */
/* enable quad wire SPI */
void spi_quad_enable(uint32_t spi_periph);
/* disable quad wire SPI */
void spi_quad_disable(uint32_t spi_periph);
/* enable quad wire SPI write */
void spi_quad_write_enable(uint32_t spi_periph);
/* enable quad wire SPI read */
void spi_quad_read_enable(uint32_t spi_periph);
/* enable quad wire SPI_IO2 and SPI_IO3 pin output */
void spi_quad_io23_output_enable(uint32_t spi_periph);
/* disable quad wire SPI_IO2 and SPI_IO3 pin output */
void spi_quad_io23_output_disable(uint32_t spi_periph);

/* flag and interrupt functions */
/* enable SPI and I2S interrupt */
void spi_i2s_interrupt_enable(uint32_t spi_periph, uint8_t interrupt);
/* disable SPI and I2S interrupt */
void spi_i2s_interrupt_disable(uint32_t spi_periph, uint8_t interrupt);
/* get SPI and I2S interrupt status */
FlagStatus spi_i2s_interrupt_flag_get(uint32_t spi_periph, uint8_t interrupt);
/* get SPI and I2S flag status */
FlagStatus spi_i2s_flag_get(uint32_t spi_periph, uint32_t flag);

#endif /* GD32F30X_SPI_H */
