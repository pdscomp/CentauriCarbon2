/*!
    \file    gd32f30x_i2c.h
    \brief   definitions for the I2C

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

#ifndef GD32F30X_I2C_H
#define GD32F30X_I2C_H

#include "gd32f30x.h"

/* I2Cx(x=0,1) definitions */
#define I2C0                          I2C_BASE                         /*!< I2C0 base address */
#define I2C1                          (I2C_BASE + 0x00000400U)         /*!< I2C1 base address */

/* registers definitions */
#define I2C_CTL0(i2cx)                REG32((i2cx) + 0x00000000U)      /*!< I2C control register 0 */
#define I2C_CTL1(i2cx)                REG32((i2cx) + 0x00000004U)      /*!< I2C control register 1 */
#define I2C_SADDR0(i2cx)              REG32((i2cx) + 0x00000008U)      /*!< I2C slave address register 0*/
#define I2C_SADDR1(i2cx)              REG32((i2cx) + 0x0000000CU)      /*!< I2C slave address register */
#define I2C_DATA(i2cx)                REG32((i2cx) + 0x00000010U)      /*!< I2C transfer buffer register */
#define I2C_STAT0(i2cx)               REG32((i2cx) + 0x00000014U)      /*!< I2C transfer status register 0 */
#define I2C_STAT1(i2cx)               REG32((i2cx) + 0x00000018U)      /*!< I2C transfer status register */
#define I2C_CKCFG(i2cx)               REG32((i2cx) + 0x0000001CU)      /*!< I2C clock configure register */
#define I2C_RT(i2cx)                  REG32((i2cx) + 0x00000020U)      /*!< I2C rise time register */
#define I2C_FMPCFG(i2cx)              REG32((i2cx) + 0x00000090U)      /*!< I2C fast-mode-plus configure register */

/* bits definitions */
/* I2Cx_CTL0 */
#define I2C_CTL0_I2CEN                BIT(0)        /*!< peripheral enable */
#define I2C_CTL0_SMBEN                BIT(1)        /*!< SMBus mode */
#define I2C_CTL0_SMBSEL               BIT(3)        /*!< SMBus type */
#define I2C_CTL0_ARPEN                BIT(4)        /*!< ARP enable */
#define I2C_CTL0_PECEN                BIT(5)        /*!< PEC enable */
#define I2C_CTL0_GCEN                 BIT(6)        /*!< general call enable */
#define I2C_CTL0_SS                   BIT(7)        /*!< clock stretching disable (slave mode) */
#define I2C_CTL0_START                BIT(8)        /*!< start generation */
#define I2C_CTL0_STOP                 BIT(9)        /*!< stop generation */
#define I2C_CTL0_ACKEN                BIT(10)       /*!< acknowledge enable */
#define I2C_CTL0_POAP                 BIT(11)       /*!< acknowledge/PEC position (for data reception) */
#define I2C_CTL0_PECTRANS             BIT(12)       /*!< packet error checking */
#define I2C_CTL0_SALT                 BIT(13)       /*!< SMBus alert */
#define I2C_CTL0_SRESET               BIT(15)       /*!< software reset */

/* I2Cx_CTL1 */
#define I2C_CTL1_I2CCLK               BITS(0,6)     /*!< I2CCLK[6:0] bits (peripheral clock frequency) */
#define I2C_CTL1_ERRIE                BIT(8)        /*!< error interrupt inable */
#define I2C_CTL1_EVIE                 BIT(9)        /*!< event interrupt enable */
#define I2C_CTL1_BUFIE                BIT(10)       /*!< buffer interrupt enable */
#define I2C_CTL1_DMAON                BIT(11)       /*!< DMA requests enable */
#define I2C_CTL1_DMALST               BIT(12)       /*!< DMA last transfer */

/* I2Cx_SADDR0 */
#define I2C_SADDR0_ADDRESS0           BIT(0)        /*!< bit 0 of a 10-bit address */
#define I2C_SADDR0_ADDRESS            BITS(1,7)     /*!< 7-bit address or bits 7:1 of a 10-bit address */
#define I2C_SADDR0_ADDRESS_H          BITS(8,9)     /*!< highest two bits of a 10-bit address */
#define I2C_SADDR0_ADDFORMAT          BIT(15)       /*!< address mode for the I2C slave */

/* I2Cx_SADDR1 */
#define I2C_SADDR1_DUADEN             BIT(0)        /*!< aual-address mode switch */
#define I2C_SADDR1_ADDRESS2           BITS(1,7)     /*!< second I2C address for the slave in dual-address mode */

/* I2Cx_DATA */
#define I2C_DATA_TRB                  BITS(0,7)     /*!< 8-bit data register */

/* I2Cx_STAT0 */
#define I2C_STAT0_SBSEND              BIT(0)        /*!< start bit (master mode) */
#define I2C_STAT0_ADDSEND             BIT(1)        /*!< address sent (master mode)/matched (slave mode) */
#define I2C_STAT0_BTC                 BIT(2)        /*!< byte transfer finished */
#define I2C_STAT0_ADD10SEND           BIT(3)        /*!< 10-bit header sent (master mode) */
#define I2C_STAT0_STPDET              BIT(4)        /*!< stop detection (slave mode) */
#define I2C_STAT0_RBNE                BIT(6)        /*!< data register not empty (receivers) */
#define I2C_STAT0_TBE                 BIT(7)        /*!< data register empty (transmitters) */
#define I2C_STAT0_BERR                BIT(8)        /*!< bus error */
#define I2C_STAT0_LOSTARB             BIT(9)        /*!< arbitration lost (master mode) */
#define I2C_STAT0_AERR                BIT(10)       /*!< acknowledge failure */
#define I2C_STAT0_OUERR               BIT(11)       /*!< overrun/underrun */
#define I2C_STAT0_PECERR              BIT(12)       /*!< PEC error in reception */
#define I2C_STAT0_SMBTO               BIT(14)       /*!< timeout signal in SMBus mode */
#define I2C_STAT0_SMBALT              BIT(15)       /*!< SMBus alert status */

/* I2Cx_STAT1 */
#define I2C_STAT1_MASTER              BIT(0)        /*!< master/slave */
#define I2C_STAT1_I2CBSY              BIT(1)        /*!< bus busy */
#define I2C_STAT1_TR                  BIT(2)        /*!< transmitter/receiver */
#define I2C_STAT1_RXGC                BIT(4)        /*!< general call address (slave mode) */
#define I2C_STAT1_DEFSMB              BIT(5)        /*!< SMBus device default address (slave mode) */
#define I2C_STAT1_HSTSMB              BIT(6)        /*!< SMBus host header (slave mode) */
#define I2C_STAT1_DUMODF              BIT(7)        /*!< dual flag (slave mode) */
#define I2C_STAT1_PECV                BITS(8,15)    /*!< packet error checking value */

/* I2Cx_CKCFG */
#define I2C_CKCFG_CLKC                BITS(0,11)    /*!< clock control register in fast/standard mode or fast mode plus(master mode) */
#define I2C_CKCFG_DTCY                BIT(14)       /*!< duty cycle of fast mode or fast mode plus */
#define I2C_CKCFG_FAST                BIT(15)       /*!< I2C speed selection in master mode */

/* I2Cx_RT */
#define I2C_RT_RISETIME               BITS(0,6)     /*!< maximum rise time in fast/standard mode or fast mode plus(master mode) */

/* I2Cx_FMPCFG */
#define I2C_FMPCFG_FMPEN              BIT(0)        /*!< fast mode plus enable bit */

/* constants definitions */
/* define the I2C bit position and its register index offset */
#define I2C_REGIDX_BIT(regidx, bitpos)  (((uint32_t)(regidx) << 6) | (uint32_t)(bitpos))
#define I2C_REG_VAL(i2cx, offset)       (REG32((i2cx) + (((uint32_t)(offset) & 0xFFFFU) >> 6)))
#define I2C_BIT_POS(val)                ((uint32_t)(val) & 0x1FU)
#define I2C_REGIDX_BIT2(regidx, bitpos, regidx2, bitpos2)   (((uint32_t)(regidx2) << 22) | (uint32_t)((bitpos2) << 16)\
                                                              | (((uint32_t)(regidx) << 6) | (uint32_t)(bitpos)))
#define I2C_REG_VAL2(i2cx, offset)      (REG32((i2cx) + ((uint32_t)(offset) >> 22)))
#define I2C_BIT_POS2(val)               (((uint32_t)(val) & 0x1F0000U) >> 16)

/* register offset */
#define I2C_CTL1_REG_OFFSET           0x04U         /*!< CTL1 register offset */
#define I2C_STAT0_REG_OFFSET          0x14U         /*!< STAT0 register offset */
#define I2C_STAT1_REG_OFFSET          0x18U         /*!< STAT1 register offset */

/* I2C flags */
typedef enum
{
    /* flags in STAT0 register */
    I2C_FLAG_SBSEND = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 0U),                /*!< start condition sent out in master mode */
    I2C_FLAG_ADDSEND = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 1U),               /*!< address is sent in master mode or received and matches in slave mode */
    I2C_FLAG_BTC = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 2U),                   /*!< byte transmission finishes */
    I2C_FLAG_ADD10SEND = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 3U),             /*!< header of 10-bit address is sent in master mode */
    I2C_FLAG_STPDET = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 4U),                /*!< stop condition detected in slave mode */
    I2C_FLAG_RBNE = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 6U),                  /*!< I2C_DATA is not Empty during receiving */
    I2C_FLAG_TBE = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 7U),                   /*!< I2C_DATA is empty during transmitting */
    I2C_FLAG_BERR = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 8U),                  /*!< a bus error occurs indication a unexpected start or stop condition on I2C bus */
    I2C_FLAG_LOSTARB = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 9U),               /*!< arbitration lost in master mode */
    I2C_FLAG_AERR = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 10U),                 /*!< acknowledge error */
    I2C_FLAG_OUERR = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 11U),                /*!< over-run or under-run situation occurs in slave mode */
    I2C_FLAG_PECERR = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 12U),               /*!< PEC error when receiving data */
    I2C_FLAG_SMBTO = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 14U),                /*!< timeout signal in SMBus mode */
    I2C_FLAG_SMBALT = I2C_REGIDX_BIT(I2C_STAT0_REG_OFFSET, 15U),               /*!< SMBus alert status */
    /* flags in STAT1 register */
    I2C_FLAG_MASTER = I2C_REGIDX_BIT(I2C_STAT1_REG_OFFSET, 0U),                /*!< a flag indicating whether I2C block is in master or slave mode */
    I2C_FLAG_I2CBSY = I2C_REGIDX_BIT(I2C_STAT1_REG_OFFSET, 1U),                /*!< busy flag */
    I2C_FLAG_TRS = I2C_REGIDX_BIT(I2C_STAT1_REG_OFFSET, 2U),                   /*!< whether the I2C is a transmitter or a receiver */
    I2C_FLAG_RXGC = I2C_REGIDX_BIT(I2C_STAT1_REG_OFFSET, 4U),                  /*!< general call address (00h) received */
    I2C_FLAG_DEFSMB = I2C_REGIDX_BIT(I2C_STAT1_REG_OFFSET, 5U),                /*!< default address of SMBus device */
    I2C_FLAG_HSTSMB = I2C_REGIDX_BIT(I2C_STAT1_REG_OFFSET, 6U),                /*!< SMBus host header detected in slave mode */
    I2C_FLAG_DUMOD = I2C_REGIDX_BIT(I2C_STAT1_REG_OFFSET, 7U)                  /*!< dual flag in slave mode indicating which address is matched in dual-address mode */
}i2c_flag_enum;

/* I2C interrupt flags */
typedef enum
{
    /* interrupt flags in CTL1 register */
    I2C_INT_FLAG_SBSEND = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 9U, I2C_STAT0_REG_OFFSET, 0U),        /*!< start condition sent out in master mode interrupt flag */
    I2C_INT_FLAG_ADDSEND = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 9U, I2C_STAT0_REG_OFFSET, 1U),       /*!< address is sent in master mode or received and matches in slave mode interrupt flag */
    I2C_INT_FLAG_BTC =  I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 9U, I2C_STAT0_REG_OFFSET, 2U),          /*!< byte transmission finishes */
    I2C_INT_FLAG_ADD10SEND =  I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 9U, I2C_STAT0_REG_OFFSET, 3U),    /*!< header of 10-bit address is sent in master mode interrupt flag */
    I2C_INT_FLAG_STPDET = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 9U, I2C_STAT0_REG_OFFSET, 4U),        /*!< stop condition detected in slave mode interrupt flag */
    I2C_INT_FLAG_RBNE = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 9U, I2C_STAT0_REG_OFFSET, 6U),          /*!< I2C_DATA is not Empty during receiving interrupt flag */
    I2C_INT_FLAG_TBE = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 9U, I2C_STAT0_REG_OFFSET, 7U),           /*!< I2C_DATA is empty during transmitting interrupt flag */    
    I2C_INT_FLAG_BERR = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 8U, I2C_STAT0_REG_OFFSET, 8U),          /*!< a bus error occurs indication a unexpected start or stop condition on I2C bus interrupt flag */
    I2C_INT_FLAG_LOSTARB = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 8U, I2C_STAT0_REG_OFFSET, 9U),       /*!< arbitration lost in master mode interrupt flag */
    I2C_INT_FLAG_AERR = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 8U, I2C_STAT0_REG_OFFSET, 10U),         /*!< acknowledge error interrupt flag */
    I2C_INT_FLAG_OUERR = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 8U, I2C_STAT0_REG_OFFSET, 11U),        /*!< over-run or under-run situation occurs in slave mode interrupt flag */
    I2C_INT_FLAG_PECERR = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 8U, I2C_STAT0_REG_OFFSET, 12U),       /*!< PEC error when receiving data interrupt flag */
    I2C_INT_FLAG_SMBTO = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 8U, I2C_STAT0_REG_OFFSET, 14U),        /*!< timeout signal in SMBus mode interrupt flag */
    I2C_INT_FLAG_SMBALT = I2C_REGIDX_BIT2(I2C_CTL1_REG_OFFSET, 8U, I2C_STAT0_REG_OFFSET, 15U),       /*!< SMBus Alert status interrupt flag */
}i2c_interrupt_flag_enum;

/* I2C interrupt enable or disable */
typedef enum
{
    /* interrupt in CTL1 register */
    I2C_INT_ERR = I2C_REGIDX_BIT(I2C_CTL1_REG_OFFSET, 8U),                     /*!< error interrupt enable */
    I2C_INT_EV = I2C_REGIDX_BIT(I2C_CTL1_REG_OFFSET, 9U),                      /*!< event interrupt enable */
    I2C_INT_BUF = I2C_REGIDX_BIT(I2C_CTL1_REG_OFFSET, 10U),                    /*!< buffer interrupt enable */
}i2c_interrupt_enum;

/* SMBus/I2C mode switch and SMBus type selection */
#define I2C_I2CMODE_ENABLE            ((uint32_t)0x00000000U)                  /*!< I2C mode */
#define I2C_SMBUSMODE_ENABLE          I2C_CTL0_SMBEN                           /*!< SMBus mode */

/* SMBus/I2C mode switch and SMBus type selection */
#define I2C_SMBUS_DEVICE              ((uint32_t)0x00000000U)                  /*!< SMBus mode device type */
#define I2C_SMBUS_HOST                I2C_CTL0_SMBSEL                          /*!< SMBus mode host type */

/* I2C transfer direction */
#define I2C_RECEIVER                  ((uint32_t)0x00000001U)                  /*!< receiver */
#define I2C_TRANSMITTER               ((uint32_t)0xFFFFFFFEU)                  /*!< transmitter */

/* whether or not to send an ACK */
#define I2C_ACK_DISABLE               ((uint32_t)0x00000000U)                  /*!< ACK will be not sent */
#define I2C_ACK_ENABLE                I2C_CTL0_ACKEN                           /*!< ACK will be sent */

/* I2C POAP position*/
#define I2C_ACKPOS_CURRENT            ((uint32_t)0x00000000U)                  /*!< ACKEN bit decides whether or not to send ACK or not for the current byte */
#define I2C_ACKPOS_NEXT               I2C_CTL0_POAP                            /*!< ACKEN bit decides whether or not to send ACK for the next byte */

/* I2C dual-address mode switch */
#define I2C_DUADEN_DISABLE            ((uint32_t)0x00000000U)                  /*!< dual-address mode disabled */
#define I2C_DUADEN_ENABLE             ((uint32_t)0x00000001U)                  /*!< dual-address mode enabled */

/* whether or not to stretch SCL low */
#define I2C_SCLSTRETCH_ENABLE         ((uint32_t)0x00000000U)                  /*!< SCL stretching is enabled */
#define I2C_SCLSTRETCH_DISABLE        I2C_CTL0_SS                              /*!< SCL stretching is disabled */

/* whether or not to response to a general call */
#define I2C_GCEN_ENABLE               I2C_CTL0_GCEN                            /*!< slave will response to a general call */
#define I2C_GCEN_DISABLE              ((uint32_t)0x00000000U)                  /*!< slave will not response to a general call */

/* software reset I2C */
#define I2C_SRESET_SET                I2C_CTL0_SRESET                          /*!< I2C is under reset */
#define I2C_SRESET_RESET              ((uint32_t)0x00000000U)                  /*!< I2C is not under reset */

/* I2C DMA mode configure */
/* DMA mode switch */
#define I2C_DMA_ON                    I2C_CTL1_DMAON                           /*!< DMA mode enabled */
#define I2C_DMA_OFF                   ((uint32_t)0x00000000U)                  /*!< DMA mode disabled */

/* flag indicating DMA last transfer */
#define I2C_DMALST_ON                 I2C_CTL1_DMALST                          /*!< next DMA EOT is the last transfer */
#define I2C_DMALST_OFF                ((uint32_t)0x00000000U)                  /*!< next DMA EOT is not the last transfer */

/* I2C PEC configure */
/* PEC enable */
#define I2C_PEC_ENABLE                I2C_CTL0_PECEN                           /*!< PEC calculation on */
#define I2C_PEC_DISABLE               ((uint32_t)0x00000000U)                   /*!< PEC calculation off */

/* PEC transfer */
#define I2C_PECTRANS_ENABLE           I2C_CTL0_PECTRANS                        /*!< transfer PEC */
#define I2C_PECTRANS_DISABLE          ((uint32_t)0x00000000U)                  /*!< not transfer PEC value */

/* I2C SMBus configure */
/* issue or not alert through SMBA pin */
#define I2C_SALTSEND_ENABLE           I2C_CTL0_SALT                            /*!< issue alert through SMBA pin */
#define I2C_SALTSEND_DISABLE          ((uint32_t)0x00000000U)                  /*!< not issue alert through SMBA */

/* ARP protocol in SMBus switch */
#define I2C_ARP_ENABLE                I2C_CTL0_ARPEN                           /*!< ARP enable */
#define I2C_ARP_DISABLE               ((uint32_t)0x00000000U)                  /*!< ARP disable */

/* fast mode plus enable */
#define I2C_FAST_MODE_PLUS_ENABLE     I2C_FMPCFG_FMPEN                         /*!< fast mode plus enable */
#define I2C_FAST_MODE_PLUS_DISABLE    ((uint32_t)0x00000000U)                  /*!< fast mode plus disable */

/* transmit I2C data */
#define DATA_TRANS(regval)            (BITS(0,7) & ((uint32_t)(regval) << 0))

/* receive I2C data */
#define DATA_RECV(regval)             GET_BITS((uint32_t)(regval), 0, 7)

/* I2C duty cycle in fast mode or fast mode plus */
#define I2C_DTCY_2                    ((uint32_t)0x00000000U)                  /*!< in I2C fast mode or fast mode plus Tlow/Thigh = 2 */
#define I2C_DTCY_16_9                 I2C_CKCFG_DTCY                           /*!< in I2C fast mode or fast mode plus Tlow/Thigh = 16/9 */

/* address mode for the I2C slave */
#define I2C_ADDFORMAT_7BITS           ((uint32_t)0x00000000U)                  /*!< address:7 bits */
#define I2C_ADDFORMAT_10BITS          I2C_SADDR0_ADDFORMAT                     /*!< address:10 bits */


/******************************************************************************/
/*                                                                            */
/*                      Inter-integrated Circuit Interface                    */
/*                                                                            */
/******************************************************************************/

/*******************  Bit definition for I2C_CR1 register  ********************/
#define I2C_CR1_PE_Pos                      (0U)                               
#define I2C_CR1_PE_Msk                      (0x1UL << I2C_CR1_PE_Pos)           /*!< 0x00000001 */
#define I2C_CR1_PE                          I2C_CR1_PE_Msk                     /*!< Peripheral Enable */
#define I2C_CR1_SMBUS_Pos                   (1U)                               
#define I2C_CR1_SMBUS_Msk                   (0x1UL << I2C_CR1_SMBUS_Pos)        /*!< 0x00000002 */
#define I2C_CR1_SMBUS                       I2C_CR1_SMBUS_Msk                  /*!< SMBus Mode */
#define I2C_CR1_SMBTYPE_Pos                 (3U)                               
#define I2C_CR1_SMBTYPE_Msk                 (0x1UL << I2C_CR1_SMBTYPE_Pos)      /*!< 0x00000008 */
#define I2C_CR1_SMBTYPE                     I2C_CR1_SMBTYPE_Msk                /*!< SMBus Type */
#define I2C_CR1_ENARP_Pos                   (4U)                               
#define I2C_CR1_ENARP_Msk                   (0x1UL << I2C_CR1_ENARP_Pos)        /*!< 0x00000010 */
#define I2C_CR1_ENARP                       I2C_CR1_ENARP_Msk                  /*!< ARP Enable */
#define I2C_CR1_ENPEC_Pos                   (5U)                               
#define I2C_CR1_ENPEC_Msk                   (0x1UL << I2C_CR1_ENPEC_Pos)        /*!< 0x00000020 */
#define I2C_CR1_ENPEC                       I2C_CR1_ENPEC_Msk                  /*!< PEC Enable */
#define I2C_CR1_ENGC_Pos                    (6U)                               
#define I2C_CR1_ENGC_Msk                    (0x1UL << I2C_CR1_ENGC_Pos)         /*!< 0x00000040 */
#define I2C_CR1_ENGC                        I2C_CR1_ENGC_Msk                   /*!< General Call Enable */
#define I2C_CR1_NOSTRETCH_Pos               (7U)                               
#define I2C_CR1_NOSTRETCH_Msk               (0x1UL << I2C_CR1_NOSTRETCH_Pos)    /*!< 0x00000080 */
#define I2C_CR1_NOSTRETCH                   I2C_CR1_NOSTRETCH_Msk              /*!< Clock Stretching Disable (Slave mode) */
#define I2C_CR1_START_Pos                   (8U)                               
#define I2C_CR1_START_Msk                   (0x1UL << I2C_CR1_START_Pos)        /*!< 0x00000100 */
#define I2C_CR1_START                       I2C_CR1_START_Msk                  /*!< Start Generation */
#define I2C_CR1_STOP_Pos                    (9U)                               
#define I2C_CR1_STOP_Msk                    (0x1UL << I2C_CR1_STOP_Pos)         /*!< 0x00000200 */
#define I2C_CR1_STOP                        I2C_CR1_STOP_Msk                   /*!< Stop Generation */
#define I2C_CR1_ACK_Pos                     (10U)                              
#define I2C_CR1_ACK_Msk                     (0x1UL << I2C_CR1_ACK_Pos)          /*!< 0x00000400 */
#define I2C_CR1_ACK                         I2C_CR1_ACK_Msk                    /*!< Acknowledge Enable */
#define I2C_CR1_POS_Pos                     (11U)                              
#define I2C_CR1_POS_Msk                     (0x1UL << I2C_CR1_POS_Pos)          /*!< 0x00000800 */
#define I2C_CR1_POS                         I2C_CR1_POS_Msk                    /*!< Acknowledge/PEC Position (for data reception) */
#define I2C_CR1_PEC_Pos                     (12U)                              
#define I2C_CR1_PEC_Msk                     (0x1UL << I2C_CR1_PEC_Pos)          /*!< 0x00001000 */
#define I2C_CR1_PEC                         I2C_CR1_PEC_Msk                    /*!< Packet Error Checking */
#define I2C_CR1_ALERT_Pos                   (13U)                              
#define I2C_CR1_ALERT_Msk                   (0x1UL << I2C_CR1_ALERT_Pos)        /*!< 0x00002000 */
#define I2C_CR1_ALERT                       I2C_CR1_ALERT_Msk                  /*!< SMBus Alert */
#define I2C_CR1_SWRST_Pos                   (15U)                              
#define I2C_CR1_SWRST_Msk                   (0x1UL << I2C_CR1_SWRST_Pos)        /*!< 0x00008000 */
#define I2C_CR1_SWRST                       I2C_CR1_SWRST_Msk                  /*!< Software Reset */

/*******************  Bit definition for I2C_CR2 register  ********************/
#define I2C_CR2_FREQ_Pos                    (0U)                               
#define I2C_CR2_FREQ_Msk                    (0x3FUL << I2C_CR2_FREQ_Pos)        /*!< 0x0000003F */
#define I2C_CR2_FREQ                        I2C_CR2_FREQ_Msk                   /*!< FREQ[5:0] bits (Peripheral Clock Frequency) */
#define I2C_CR2_FREQ_0                      (0x01UL << I2C_CR2_FREQ_Pos)        /*!< 0x00000001 */
#define I2C_CR2_FREQ_1                      (0x02UL << I2C_CR2_FREQ_Pos)        /*!< 0x00000002 */
#define I2C_CR2_FREQ_2                      (0x04UL << I2C_CR2_FREQ_Pos)        /*!< 0x00000004 */
#define I2C_CR2_FREQ_3                      (0x08UL << I2C_CR2_FREQ_Pos)        /*!< 0x00000008 */
#define I2C_CR2_FREQ_4                      (0x10UL << I2C_CR2_FREQ_Pos)        /*!< 0x00000010 */
#define I2C_CR2_FREQ_5                      (0x20UL << I2C_CR2_FREQ_Pos)        /*!< 0x00000020 */

#define I2C_CR2_ITERREN_Pos                 (8U)                               
#define I2C_CR2_ITERREN_Msk                 (0x1UL << I2C_CR2_ITERREN_Pos)      /*!< 0x00000100 */
#define I2C_CR2_ITERREN                     I2C_CR2_ITERREN_Msk                /*!< Error Interrupt Enable */
#define I2C_CR2_ITEVTEN_Pos                 (9U)                               
#define I2C_CR2_ITEVTEN_Msk                 (0x1UL << I2C_CR2_ITEVTEN_Pos)      /*!< 0x00000200 */
#define I2C_CR2_ITEVTEN                     I2C_CR2_ITEVTEN_Msk                /*!< Event Interrupt Enable */
#define I2C_CR2_ITBUFEN_Pos                 (10U)                              
#define I2C_CR2_ITBUFEN_Msk                 (0x1UL << I2C_CR2_ITBUFEN_Pos)      /*!< 0x00000400 */
#define I2C_CR2_ITBUFEN                     I2C_CR2_ITBUFEN_Msk                /*!< Buffer Interrupt Enable */
#define I2C_CR2_DMAEN_Pos                   (11U)                              
#define I2C_CR2_DMAEN_Msk                   (0x1UL << I2C_CR2_DMAEN_Pos)        /*!< 0x00000800 */
#define I2C_CR2_DMAEN                       I2C_CR2_DMAEN_Msk                  /*!< DMA Requests Enable */
#define I2C_CR2_LAST_Pos                    (12U)                              
#define I2C_CR2_LAST_Msk                    (0x1UL << I2C_CR2_LAST_Pos)         /*!< 0x00001000 */
#define I2C_CR2_LAST                        I2C_CR2_LAST_Msk                   /*!< DMA Last Transfer */

/*******************  Bit definition for I2C_OAR1 register  *******************/
#define I2C_OAR1_ADD1_7                     0x000000FEU             /*!< Interface Address */
#define I2C_OAR1_ADD8_9                     0x00000300U             /*!< Interface Address */

#define I2C_OAR1_ADD0_Pos                   (0U)                               
#define I2C_OAR1_ADD0_Msk                   (0x1UL << I2C_OAR1_ADD0_Pos)        /*!< 0x00000001 */
#define I2C_OAR1_ADD0                       I2C_OAR1_ADD0_Msk                  /*!< Bit 0 */
#define I2C_OAR1_ADD1_Pos                   (1U)                               
#define I2C_OAR1_ADD1_Msk                   (0x1UL << I2C_OAR1_ADD1_Pos)        /*!< 0x00000002 */
#define I2C_OAR1_ADD1                       I2C_OAR1_ADD1_Msk                  /*!< Bit 1 */
#define I2C_OAR1_ADD2_Pos                   (2U)                               
#define I2C_OAR1_ADD2_Msk                   (0x1UL << I2C_OAR1_ADD2_Pos)        /*!< 0x00000004 */
#define I2C_OAR1_ADD2                       I2C_OAR1_ADD2_Msk                  /*!< Bit 2 */
#define I2C_OAR1_ADD3_Pos                   (3U)                               
#define I2C_OAR1_ADD3_Msk                   (0x1UL << I2C_OAR1_ADD3_Pos)        /*!< 0x00000008 */
#define I2C_OAR1_ADD3                       I2C_OAR1_ADD3_Msk                  /*!< Bit 3 */
#define I2C_OAR1_ADD4_Pos                   (4U)                               
#define I2C_OAR1_ADD4_Msk                   (0x1UL << I2C_OAR1_ADD4_Pos)        /*!< 0x00000010 */
#define I2C_OAR1_ADD4                       I2C_OAR1_ADD4_Msk                  /*!< Bit 4 */
#define I2C_OAR1_ADD5_Pos                   (5U)                               
#define I2C_OAR1_ADD5_Msk                   (0x1UL << I2C_OAR1_ADD5_Pos)        /*!< 0x00000020 */
#define I2C_OAR1_ADD5                       I2C_OAR1_ADD5_Msk                  /*!< Bit 5 */
#define I2C_OAR1_ADD6_Pos                   (6U)                               
#define I2C_OAR1_ADD6_Msk                   (0x1UL << I2C_OAR1_ADD6_Pos)        /*!< 0x00000040 */
#define I2C_OAR1_ADD6                       I2C_OAR1_ADD6_Msk                  /*!< Bit 6 */
#define I2C_OAR1_ADD7_Pos                   (7U)                               
#define I2C_OAR1_ADD7_Msk                   (0x1UL << I2C_OAR1_ADD7_Pos)        /*!< 0x00000080 */
#define I2C_OAR1_ADD7                       I2C_OAR1_ADD7_Msk                  /*!< Bit 7 */
#define I2C_OAR1_ADD8_Pos                   (8U)                               
#define I2C_OAR1_ADD8_Msk                   (0x1UL << I2C_OAR1_ADD8_Pos)        /*!< 0x00000100 */
#define I2C_OAR1_ADD8                       I2C_OAR1_ADD8_Msk                  /*!< Bit 8 */
#define I2C_OAR1_ADD9_Pos                   (9U)                               
#define I2C_OAR1_ADD9_Msk                   (0x1UL << I2C_OAR1_ADD9_Pos)        /*!< 0x00000200 */
#define I2C_OAR1_ADD9                       I2C_OAR1_ADD9_Msk                  /*!< Bit 9 */

#define I2C_OAR1_ADDMODE_Pos                (15U)                              
#define I2C_OAR1_ADDMODE_Msk                (0x1UL << I2C_OAR1_ADDMODE_Pos)     /*!< 0x00008000 */
#define I2C_OAR1_ADDMODE                    I2C_OAR1_ADDMODE_Msk               /*!< Addressing Mode (Slave mode) */

/*******************  Bit definition for I2C_OAR2 register  *******************/
#define I2C_OAR2_ENDUAL_Pos                 (0U)                               
#define I2C_OAR2_ENDUAL_Msk                 (0x1UL << I2C_OAR2_ENDUAL_Pos)      /*!< 0x00000001 */
#define I2C_OAR2_ENDUAL                     I2C_OAR2_ENDUAL_Msk                /*!< Dual addressing mode enable */
#define I2C_OAR2_ADD2_Pos                   (1U)                               
#define I2C_OAR2_ADD2_Msk                   (0x7FUL << I2C_OAR2_ADD2_Pos)       /*!< 0x000000FE */
#define I2C_OAR2_ADD2                       I2C_OAR2_ADD2_Msk                  /*!< Interface address */

/********************  Bit definition for I2C_DR register  ********************/
#define I2C_DR_DR_Pos             (0U)                                         
#define I2C_DR_DR_Msk             (0xFFUL << I2C_DR_DR_Pos)                     /*!< 0x000000FF */
#define I2C_DR_DR                 I2C_DR_DR_Msk                                /*!< 8-bit Data Register         */

/*******************  Bit definition for I2C_SR1 register  ********************/
#define I2C_SR1_SB_Pos                      (0U)                               
#define I2C_SR1_SB_Msk                      (0x1UL << I2C_SR1_SB_Pos)           /*!< 0x00000001 */
#define I2C_SR1_SB                          I2C_SR1_SB_Msk                     /*!< Start Bit (Master mode) */
#define I2C_SR1_ADDR_Pos                    (1U)                               
#define I2C_SR1_ADDR_Msk                    (0x1UL << I2C_SR1_ADDR_Pos)         /*!< 0x00000002 */
#define I2C_SR1_ADDR                        I2C_SR1_ADDR_Msk                   /*!< Address sent (master mode)/matched (slave mode) */
#define I2C_SR1_BTF_Pos                     (2U)                               
#define I2C_SR1_BTF_Msk                     (0x1UL << I2C_SR1_BTF_Pos)          /*!< 0x00000004 */
#define I2C_SR1_BTF                         I2C_SR1_BTF_Msk                    /*!< Byte Transfer Finished */
#define I2C_SR1_ADD10_Pos                   (3U)                               
#define I2C_SR1_ADD10_Msk                   (0x1UL << I2C_SR1_ADD10_Pos)        /*!< 0x00000008 */
#define I2C_SR1_ADD10                       I2C_SR1_ADD10_Msk                  /*!< 10-bit header sent (Master mode) */
#define I2C_SR1_STOPF_Pos                   (4U)                               
#define I2C_SR1_STOPF_Msk                   (0x1UL << I2C_SR1_STOPF_Pos)        /*!< 0x00000010 */
#define I2C_SR1_STOPF                       I2C_SR1_STOPF_Msk                  /*!< Stop detection (Slave mode) */
#define I2C_SR1_RXNE_Pos                    (6U)                               
#define I2C_SR1_RXNE_Msk                    (0x1UL << I2C_SR1_RXNE_Pos)         /*!< 0x00000040 */
#define I2C_SR1_RXNE                        I2C_SR1_RXNE_Msk                   /*!< Data Register not Empty (receivers) */
#define I2C_SR1_TXE_Pos                     (7U)                               
#define I2C_SR1_TXE_Msk                     (0x1UL << I2C_SR1_TXE_Pos)          /*!< 0x00000080 */
#define I2C_SR1_TXE                         I2C_SR1_TXE_Msk                    /*!< Data Register Empty (transmitters) */
#define I2C_SR1_BERR_Pos                    (8U)                               
#define I2C_SR1_BERR_Msk                    (0x1UL << I2C_SR1_BERR_Pos)         /*!< 0x00000100 */
#define I2C_SR1_BERR                        I2C_SR1_BERR_Msk                   /*!< Bus Error */
#define I2C_SR1_ARLO_Pos                    (9U)                               
#define I2C_SR1_ARLO_Msk                    (0x1UL << I2C_SR1_ARLO_Pos)         /*!< 0x00000200 */
#define I2C_SR1_ARLO                        I2C_SR1_ARLO_Msk                   /*!< Arbitration Lost (master mode) */
#define I2C_SR1_AF_Pos                      (10U)                              
#define I2C_SR1_AF_Msk                      (0x1UL << I2C_SR1_AF_Pos)           /*!< 0x00000400 */
#define I2C_SR1_AF                          I2C_SR1_AF_Msk                     /*!< Acknowledge Failure */
#define I2C_SR1_OVR_Pos                     (11U)                              
#define I2C_SR1_OVR_Msk                     (0x1UL << I2C_SR1_OVR_Pos)          /*!< 0x00000800 */
#define I2C_SR1_OVR                         I2C_SR1_OVR_Msk                    /*!< Overrun/Underrun */
#define I2C_SR1_PECERR_Pos                  (12U)                              
#define I2C_SR1_PECERR_Msk                  (0x1UL << I2C_SR1_PECERR_Pos)       /*!< 0x00001000 */
#define I2C_SR1_PECERR                      I2C_SR1_PECERR_Msk                 /*!< PEC Error in reception */
#define I2C_SR1_TIMEOUT_Pos                 (14U)                              
#define I2C_SR1_TIMEOUT_Msk                 (0x1UL << I2C_SR1_TIMEOUT_Pos)      /*!< 0x00004000 */
#define I2C_SR1_TIMEOUT                     I2C_SR1_TIMEOUT_Msk                /*!< Timeout or Tlow Error */
#define I2C_SR1_SMBALERT_Pos                (15U)                              
#define I2C_SR1_SMBALERT_Msk                (0x1UL << I2C_SR1_SMBALERT_Pos)     /*!< 0x00008000 */
#define I2C_SR1_SMBALERT                    I2C_SR1_SMBALERT_Msk               /*!< SMBus Alert */

/*******************  Bit definition for I2C_SR2 register  ********************/
#define I2C_SR2_MSL_Pos                     (0U)                               
#define I2C_SR2_MSL_Msk                     (0x1UL << I2C_SR2_MSL_Pos)          /*!< 0x00000001 */
#define I2C_SR2_MSL                         I2C_SR2_MSL_Msk                    /*!< Master/Slave */
#define I2C_SR2_BUSY_Pos                    (1U)                               
#define I2C_SR2_BUSY_Msk                    (0x1UL << I2C_SR2_BUSY_Pos)         /*!< 0x00000002 */
#define I2C_SR2_BUSY                        I2C_SR2_BUSY_Msk                   /*!< Bus Busy */
#define I2C_SR2_TRA_Pos                     (2U)                               
#define I2C_SR2_TRA_Msk                     (0x1UL << I2C_SR2_TRA_Pos)          /*!< 0x00000004 */
#define I2C_SR2_TRA                         I2C_SR2_TRA_Msk                    /*!< Transmitter/Receiver */
#define I2C_SR2_GENCALL_Pos                 (4U)                               
#define I2C_SR2_GENCALL_Msk                 (0x1UL << I2C_SR2_GENCALL_Pos)      /*!< 0x00000010 */
#define I2C_SR2_GENCALL                     I2C_SR2_GENCALL_Msk                /*!< General Call Address (Slave mode) */
#define I2C_SR2_SMBDEFAULT_Pos              (5U)                               
#define I2C_SR2_SMBDEFAULT_Msk              (0x1UL << I2C_SR2_SMBDEFAULT_Pos)   /*!< 0x00000020 */
#define I2C_SR2_SMBDEFAULT                  I2C_SR2_SMBDEFAULT_Msk             /*!< SMBus Device Default Address (Slave mode) */
#define I2C_SR2_SMBHOST_Pos                 (6U)                               
#define I2C_SR2_SMBHOST_Msk                 (0x1UL << I2C_SR2_SMBHOST_Pos)      /*!< 0x00000040 */
#define I2C_SR2_SMBHOST                     I2C_SR2_SMBHOST_Msk                /*!< SMBus Host Header (Slave mode) */
#define I2C_SR2_DUALF_Pos                   (7U)                               
#define I2C_SR2_DUALF_Msk                   (0x1UL << I2C_SR2_DUALF_Pos)        /*!< 0x00000080 */
#define I2C_SR2_DUALF                       I2C_SR2_DUALF_Msk                  /*!< Dual Flag (Slave mode) */
#define I2C_SR2_PEC_Pos                     (8U)                               
#define I2C_SR2_PEC_Msk                     (0xFFUL << I2C_SR2_PEC_Pos)         /*!< 0x0000FF00 */
#define I2C_SR2_PEC                         I2C_SR2_PEC_Msk                    /*!< Packet Error Checking Register */

/*******************  Bit definition for I2C_CCR register  ********************/
#define I2C_CCR_CCR_Pos                     (0U)                               
#define I2C_CCR_CCR_Msk                     (0xFFFUL << I2C_CCR_CCR_Pos)        /*!< 0x00000FFF */
#define I2C_CCR_CCR                         I2C_CCR_CCR_Msk                    /*!< Clock Control Register in Fast/Standard mode (Master mode) */
#define I2C_CCR_DUTY_Pos                    (14U)                              
#define I2C_CCR_DUTY_Msk                    (0x1UL << I2C_CCR_DUTY_Pos)         /*!< 0x00004000 */
#define I2C_CCR_DUTY                        I2C_CCR_DUTY_Msk                   /*!< Fast Mode Duty Cycle */
#define I2C_CCR_FS_Pos                      (15U)                              
#define I2C_CCR_FS_Msk                      (0x1UL << I2C_CCR_FS_Pos)           /*!< 0x00008000 */
#define I2C_CCR_FS                          I2C_CCR_FS_Msk                     /*!< I2C Master Mode Selection */

/******************  Bit definition for I2C_TRISE register  *******************/
#define I2C_TRISE_TRISE_Pos                 (0U)                               
#define I2C_TRISE_TRISE_Msk                 (0x3FUL << I2C_TRISE_TRISE_Pos)     /*!< 0x0000003F */
#define I2C_TRISE_TRISE                     I2C_TRISE_TRISE_Msk                /*!< Maximum Rise Time in Fast/Standard mode (Master mode) */


/* function declarations */
/* initialization functions */
/* reset I2C */
void i2c_deinit(uint32_t i2c_periph);
/* configure I2C clock */
void i2c_clock_config(uint32_t i2c_periph, uint32_t clkspeed, uint32_t dutycyc);
/* configure I2C address */
void i2c_mode_addr_config(uint32_t i2c_periph, uint32_t mode, uint32_t addformat, uint32_t addr);

/* application function declarations */
/* select SMBus type */
void i2c_smbus_type_config(uint32_t i2c_periph, uint32_t type);
/* whether or not to send an ACK */
void i2c_ack_config(uint32_t i2c_periph, uint32_t ack);
/* configure I2C POAP position */
void i2c_ackpos_config(uint32_t i2c_periph, uint32_t pos);
/* master sends slave address */
void i2c_master_addressing(uint32_t i2c_periph, uint32_t addr, uint32_t trandirection);
/* enable dual-address mode */
void i2c_dualaddr_enable(uint32_t i2c_periph, uint32_t dualaddr);
/* disable dual-address mode */
void i2c_dualaddr_disable(uint32_t i2c_periph);
/* enable I2C */
void i2c_enable(uint32_t i2c_periph);
/* disable I2C */
void i2c_disable(uint32_t i2c_periph);
/* generate a START condition on I2C bus */
void i2c_start_on_bus(uint32_t i2c_periph);
/* generate a STOP condition on I2C bus */
void i2c_stop_on_bus(uint32_t i2c_periph);
/* I2C transmit data function */
void i2c_data_transmit(uint32_t i2c_periph, uint8_t data);
/* I2C receive data function */
uint8_t i2c_data_receive(uint32_t i2c_periph);
/* configure I2C DMA mode */
void i2c_dma_config(uint32_t i2c_periph, uint32_t dmastate);
/* configure whether next DMA EOT is DMA last transfer or not */
void i2c_dma_last_transfer_config(uint32_t i2c_periph, uint32_t dmalast);
/* whether to stretch SCL low when data is not ready in slave mode */
void i2c_stretch_scl_low_config(uint32_t i2c_periph, uint32_t stretchpara);
/* whether or not to response to a general call */
void i2c_slave_response_to_gcall_config(uint32_t i2c_periph, uint32_t gcallpara);
/* configure software reset of I2C */
void i2c_software_reset_config(uint32_t i2c_periph, uint32_t sreset);
/* configure I2C PEC calculation */
void i2c_pec_config(uint32_t i2c_periph, uint32_t pecstate);
/* configure whether to transfer PEC value */
void i2c_pec_transfer_config(uint32_t i2c_periph, uint32_t pecpara);
/* get packet error checking value */
uint8_t i2c_pec_value_get(uint32_t i2c_periph);
/* configure I2C alert through SMBA pin */
void i2c_smbus_alert_config(uint32_t i2c_periph, uint32_t smbuspara);
/* configure I2C ARP protocol in SMBus */
void i2c_smbus_arp_config(uint32_t i2c_periph, uint32_t arpstate);

/* interrupt & flag functions */
/* get I2C flag status */
FlagStatus i2c_flag_get(uint32_t i2c_periph, i2c_flag_enum flag);
/* clear I2C flag status */
void i2c_flag_clear(uint32_t i2c_periph, i2c_flag_enum flag);
/* enable I2C interrupt */
void i2c_interrupt_enable(uint32_t i2c_periph, i2c_interrupt_enum interrupt);
/* disable I2C interrupt */
void i2c_interrupt_disable(uint32_t i2c_periph, i2c_interrupt_enum interrupt);
/* get I2C interrupt flag status */
FlagStatus i2c_interrupt_flag_get(uint32_t i2c_periph, i2c_interrupt_flag_enum int_flag);
/* clear I2C interrupt flag status */
void i2c_interrupt_flag_clear(uint32_t i2c_periph, i2c_interrupt_flag_enum int_flag);

#endif /* GD32F30X_I2C_H */
