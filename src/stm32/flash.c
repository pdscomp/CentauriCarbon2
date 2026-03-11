/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-03-22 11:23:58
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-22 11:23:58
 * @Description  : 实现FLASH编程以及BOOTLOADER跳转操作
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#include "flash.h"
#include "board/irq.h"      // irq_save
#include "board/misc.h"     // alloc_maxsize
#include "board/internal.h" // __CORTEX_M
#include "autoconf.h"
typedef void (*pFunction)(void);
pFunction Jump_To_Application;
// APP跳转函数
void jump_to_app(uint32_t address)
{
    uint32_t JumpAddress;
    if (((*(volatile uint32_t *)address) & 0x2FFE0000) == 0x20000000)
    {
        __disable_irq();
        JumpAddress = *(volatile uint32_t *)(address + 4);
        Jump_To_Application = (pFunction)JumpAddress;
        __set_MSP(*(volatile uint32_t *)address);
        Jump_To_Application();
    }
}

#if CONFIG_MACH_GD32F303

void flash_erase_pages(uint32_t start_address, uint16_t *page_size, uint16_t page_num)
{
    uint32_t next_address = start_address;
    fmc_unlock();
    fmc_flag_clear(FMC_FLAG_BANK0_END);
    fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
    fmc_flag_clear(FMC_FLAG_BANK0_PGERR);
    for (int i = 0; i < page_num; i++)
    {
        fmc_page_erase(next_address);
        next_address += page_size[i];
        fmc_flag_clear(FMC_FLAG_BANK0_END);
        fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
        fmc_flag_clear(FMC_FLAG_BANK0_PGERR);
    }
    fmc_lock();
}

void flash_word_program(uint32_t start_address, uint32_t *data, uint32_t count)
{
    fmc_unlock();
    uint32_t address = start_address;
    for (int i = 0; i < count; i++)
    {
        fmc_word_program(address, data[i]);
        address += 4;
        fmc_flag_clear(FMC_FLAG_BANK0_END);
        fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
        fmc_flag_clear(FMC_FLAG_BANK0_PGERR);
    }
    fmc_lock();
}

void flash_halfword_program(uint32_t start_address, uint16_t *data, uint32_t count)
{
    fmc_unlock();
    uint32_t address = start_address;
    for (int i = 0; i < count; i++)
    {
        fmc_halfword_program(address, data[i]);
        address += 2;
        fmc_flag_clear(FMC_FLAG_BANK0_END);
        fmc_flag_clear(FMC_FLAG_BANK0_WPERR);
        fmc_flag_clear(FMC_FLAG_BANK0_PGERR);
    }
    fmc_lock();
}

uint32_t flash_get_page_size(uint32_t start_address)
{
    // 0~255, 2KB
    // 256-895, 4KB
    if (start_address < 0x08080000)
        return 0x800;
    else if (start_address < 0x08300000)
        return 0x1000;
    return 0x00;
}

#elif CONFIG_MACH_STM32F103

// COPY FORM GD32
#define FMC_TIMEOUT_COUNT ((uint32_t)0x0FFF0000U) /*!< FMC timeout count value */
#define REG32(addr) (*(volatile uint32_t *)(uint32_t)(addr))
#define REG16(addr) (*(volatile uint16_t *)(uint32_t)(addr))

typedef enum
{
    FMC_READY, /*!< the operation has been completed */
    FMC_BUSY,  /*!< the operation is in progress */
    FMC_PGERR, /*!< program error */
    FMC_WPERR, /*!< erase/program protection error */
    FMC_TOERR, /*!< timeout error */
} fmc_state_enum;

void fmc_unlock(void)
{
    if ((RESET != (FLASH->CR & FLASH_CR_LOCK)))
    {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

void fmc_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

fmc_state_enum fmc_bank0_state_get(void)
{
    fmc_state_enum fmc_state = FMC_READY;

    if ((uint32_t)0x00U != (FLASH->SR & FLASH_SR_BSY))
    {
        fmc_state = FMC_BUSY;
    }
    else
    {
        if ((uint32_t)0x00U != (FLASH->SR & FLASH_SR_WRPRTERR))
        {
            fmc_state = FMC_WPERR;
        }
        else
        {
            if ((uint32_t)0x00U != (FLASH->SR & (FLASH_SR_PGERR)))
            {
                fmc_state = FMC_PGERR;
            }
        }
    }
    /* return the FMC state */
    return fmc_state;
}

fmc_state_enum fmc_bank0_ready_wait(uint32_t timeout)
{
    fmc_state_enum fmc_state = FMC_BUSY;

    /* wait for FMC ready */
    do
    {
        /* get FMC state */
        fmc_state = fmc_bank0_state_get();
        timeout--;
    } while ((FMC_BUSY == fmc_state) && (0x00U != timeout));

    if (FMC_BUSY == fmc_state)
    {
        fmc_state = FMC_TOERR;
    }
    /* return the FMC state */
    return fmc_state;
}

fmc_state_enum fmc_page_erase(uint32_t page_address)
{
    fmc_state_enum fmc_state;

    fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
    /* if the last operation is completed, start page erase */
    if (FMC_READY == fmc_state)
    {
        FLASH->CR |= FLASH_CR_PER;
        FLASH->AR = page_address;
        FLASH->CR |= FLASH_CR_STRT;
        __NOP();
        __NOP();
        /* wait for the FMC ready */
        fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
        /* reset the PER bit */
        FLASH->CR &= ~FLASH_CR_PER;
    }
    /* return the FMC state */
    return fmc_state;
}

fmc_state_enum fmc_halfword_program(uint32_t address, uint16_t data)
{
    fmc_state_enum fmc_state = FMC_READY;
    fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
    if (FMC_READY == fmc_state)
    {
        /* set the PG bit to start program */
        FLASH->CR |= FLASH_CR_PG;
        REG16(address) = data;
        /* wait for the FMC ready */
        fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
        /* reset the PG bit */
        FLASH->CR &= ~FLASH_CR_PG;
    }
    /* return the FMC state */
    return fmc_state;
}

fmc_state_enum fmc_word_program(uint32_t address, uint32_t data)
{
    fmc_state_enum fmc_state = FMC_READY;
    volatile uint32_t lhWord = data & 0x0000FFFF;
    volatile uint32_t hhWord = (data & 0xFFFF0000) >> 16;
    fmc_state = fmc_halfword_program(address + 0x02, (volatile uint16_t)hhWord);
    fmc_state = fmc_halfword_program(address, (volatile uint16_t)lhWord);
    return fmc_state;
}

void flash_erase_pages(uint32_t start_address, uint16_t *page_size, uint16_t page_num)
{
    uint32_t next_address = start_address;
    fmc_unlock();
    FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR);
    for (int i = 0; i < page_num; i++)
    {
        fmc_page_erase(next_address);

        next_address += page_size[i];
        FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR);
    }
    fmc_lock();
}

void flash_word_program(uint32_t start_address, uint32_t *data, uint32_t count)
{
    fmc_unlock();
    uint32_t address = start_address;
    for (int i = 0; i < count; i++)
    {
        fmc_word_program(address, data[i]);
        address += 4;
        FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR);
    }
    fmc_lock();
}

void flash_halfword_program(uint32_t start_address, uint16_t *data, uint32_t count)
{
    fmc_unlock();
    uint32_t address = start_address;
    for (int i = 0; i < count; i++)
    {
        fmc_halfword_program(address, data[i]);
        address += 2;
        FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_WRPRTERR | FLASH_SR_PGERR);
    }
    fmc_lock();
}

uint32_t flash_get_page_size(uint32_t start_address)
{
    // 0~127, 1KB
    if (start_address < 0x08020000)
        return 0x400;
    return 0x00;
}

#elif CONFIG_MACH_STM32F401

#define RDP_KEY_Pos (0U)
#define RDP_KEY_Msk (0xA5UL << RDP_KEY_Pos) /*!< 0x000000A5 */
#define RDP_KEY RDP_KEY_Msk                 /*!< RDP Key */
#define FLASH_KEY1_Pos (0U)
#define FLASH_KEY1_Msk (0x45670123UL << FLASH_KEY1_Pos) /*!< 0x45670123 */
#define FLASH_KEY1 FLASH_KEY1_Msk                       /*!< FPEC Key1 */
#define FLASH_KEY2_Pos (0U)
#define FLASH_KEY2_Msk (0xCDEF89ABUL << FLASH_KEY2_Pos) /*!< 0xCDEF89AB */
#define FLASH_KEY2 FLASH_KEY2_Msk

// COPY FORM GD32
#define FMC_TIMEOUT_COUNT ((uint32_t)0x0FFFFFFFU) /*!< FMC timeout count value */
#define REG32(addr) (*(volatile uint32_t *)(uint32_t)(addr))
#define REG16(addr) (*(volatile uint16_t *)(uint32_t)(addr))

typedef enum
{
    FMC_READY, /*!< the operation has been completed */
    FMC_BUSY,  /*!< the operation is in progress */
    FMC_PGERR, /*!< program error */
    FMC_WPERR, /*!< erase/program protection error */
    FMC_TOERR, /*!< timeout error */
} fmc_state_enum;

void fmc_unlock(void)
{
    if ((RESET != (FLASH->CR & FLASH_CR_LOCK)))
    {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

void fmc_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

fmc_state_enum fmc_bank0_state_get(void)
{
    fmc_state_enum fmc_state = FMC_READY;

    if ((uint32_t)0x00U != (FLASH->SR & FLASH_SR_BSY))
    {
        fmc_state = FMC_BUSY;
    }
    else
    {
        if ((uint32_t)0x00U != (FLASH->SR & FLASH_SR_WRPERR))
        {
            fmc_state = FMC_WPERR;
        }
        else
        {
            if ((uint32_t)0x00U != (FLASH->SR & (FLASH_SR_PGPERR)) ||
                (uint32_t)0x00U != (FLASH->SR & (FLASH_SR_PGSERR)))
            {
                fmc_state = FMC_PGERR;
            }
        }
    }
    /* return the FMC state */
    return fmc_state;
}

fmc_state_enum fmc_bank0_ready_wait(uint32_t timeout)
{
    fmc_state_enum fmc_state = FMC_BUSY;
    /* wait for FMC ready */
    do
    {
        /* get FMC state */
        fmc_state = fmc_bank0_state_get();
        timeout--;
    } while ((FMC_BUSY == fmc_state) && (0x00U != timeout));

    if (FMC_BUSY == fmc_state)
    {
        fmc_state = FMC_TOERR;
    }
    /* return the FMC state */
    return fmc_state;
}

fmc_state_enum fmc_page_erase(uint32_t sector_num)
{
    fmc_state_enum fmc_state;
    fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
    /* if the last operation is completed, start page erase */
    if (FMC_READY == fmc_state)
    {
        FLASH->CR &= ~FLASH_CR_PSIZE;
        FLASH->CR |= 0x02 << FLASH_CR_PSIZE_Pos;
        FLASH->CR &= ~FLASH_CR_SNB;
        FLASH->CR |= sector_num << FLASH_CR_SNB_Pos;
        FLASH->CR |= FLASH_CR_SER;
        // 这里要喂狗,否则容易复位
        IWDG->KR = 0XAAAA;
        FLASH->CR |= FLASH_CR_STRT;
        /* wait for the FMC ready */
        fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);

        /* reset the PER bit */
        FLASH->CR &= ~FLASH_CR_SNB;
        FLASH->CR &= ~FLASH_CR_SER;
    }
    /* return the FMC state */
    return fmc_state;
}

fmc_state_enum fmc_halfword_program(uint32_t address, uint16_t data)
{
    fmc_state_enum fmc_state = FMC_READY;
    fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
    if (FMC_READY == fmc_state)
    {
        /* set the PG bit to start program */
        FLASH->CR |= FLASH_CR_PG;
        FLASH->CR &= ~FLASH_CR_PSIZE;
        FLASH->CR |= 0x01 << FLASH_CR_PSIZE_Pos;
        IWDG->KR = 0XAAAA;
        REG16(address) = data;
        /* wait for the FMC ready */
        fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
        /* reset the PG bit */
        FLASH->CR &= ~FLASH_CR_PG;
        FLASH->CR &= ~FLASH_CR_PSIZE;
    }
    /* return the FMC state */
    return fmc_state;
}

fmc_state_enum fmc_word_program(uint32_t address, uint32_t data)
{
    fmc_state_enum fmc_state = FMC_READY;
    fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
    if (FMC_READY == fmc_state)
    {
        /* set the PG bit to start program */
        FLASH->CR |= FLASH_CR_PG;
        FLASH->CR &= ~FLASH_CR_PSIZE;
        FLASH->CR |= 0x02 << FLASH_CR_PSIZE_Pos;
        IWDG->KR = 0XAAAA;
        REG32(address) = data;
        /* wait for the FMC ready */
        fmc_state = fmc_bank0_ready_wait(FMC_TIMEOUT_COUNT);
        /* reset the PG bit */
        FLASH->CR &= ~FLASH_CR_PG;
        FLASH->CR &= ~FLASH_CR_PSIZE;
    }
    /* return the FMC state */
    return fmc_state;
}

void flash_erase_sector(uint16_t sector_num)
{
    FLASH->ACR &= ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN);
    fmc_unlock();
    FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGSERR);
    fmc_page_erase(sector_num);
    FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGSERR);
    fmc_lock();
    FLASH->ACR |= (FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN);
}

void flash_word_program(uint32_t start_address, uint32_t *data, uint32_t count)
{
    FLASH->ACR &= ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN);

    fmc_unlock();
    uint32_t address = start_address;
    for (int i = 0; i < count; i++)
    {
        fmc_word_program(address, data[i]);
        address += 4;
        FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGSERR);
    }
    fmc_lock();

    FLASH->ACR |= (FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN);
}

void flash_halfword_program(uint32_t start_address, uint16_t *data, uint32_t count)
{
    FLASH->ACR &= ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN);

    fmc_unlock();
    uint32_t address = start_address;
    for (int i = 0; i < count; i++)
    {
        fmc_halfword_program(address, data[i]);
        address += 2;
        FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGSERR);
    }
    fmc_lock();
    FLASH->ACR |= (FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN);
}

uint32_t flash_get_sector_size(uint32_t sector)
{
    // 0~3 16KB
    // 4 64KB
    // 5~7 128KB
    if (sector <= 3)
        return 0x4000;
    else if (sector <= 4)
        return 0x10000;
    else if (sector <= 7)
        return 0x20000;
    return 0x00;
}

#endif
