/********************************************************************************************

    main.c

    Authors: Joe Grand (https://joegrand.com) and Lennert Wouters (https://x.com/lennertwo)
    Released as part of https://github.com/joegrand/stm32-fault-injection/
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses/.

********************************************************************************************/

#include <stdint.h>
#include <stddef.h>

#define CMD_READ    0x01u
#define CMD_TEST    0x02u
#define CMD_DUMPALL 0x03u
#define CMD_READCRC 0x04u

#define __IO volatile

/* --- Peripheral base addresses --- */
#define PERIPH_BASE      0x40000000UL
#define AHB1PERIPH_BASE  (PERIPH_BASE + 0x00020000UL) /* 0x40020000 */
#define APB2PERIPH_BASE  (PERIPH_BASE + 0x00010000UL) /* 0x40010000 */
#define RCC_BASE         0x40023800UL

/* --- RCC registers --- */
#define RCC_CR      (*(volatile uint32_t *)(RCC_BASE + 0x00U))
#define RCC_CFGR    (*(volatile uint32_t *)(RCC_BASE + 0x08U))
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x44U))

/* --- GPIOA (AHB1) --- */
typedef struct {
    __IO uint32_t MODER;
    __IO uint32_t OTYPER;
    __IO uint32_t OSPEEDR;
    __IO uint32_t PUPDR;
    __IO uint32_t IDR;
    __IO uint32_t ODR;
    __IO uint32_t BSRR;
    __IO uint32_t LCKR;
    __IO uint32_t AFRL;
    __IO uint32_t AFRH;
} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef *)(AHB1PERIPH_BASE + 0x0000U)) /* 0x40020000 */

/* --- USART1 (APB2) --- */
typedef struct {
    __IO uint32_t SR;
    __IO uint32_t DR;
    __IO uint32_t BRR;
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t CR3;
    __IO uint32_t GTPR;
} USART_TypeDef;

#define USART1 ((USART_TypeDef *)(APB2PERIPH_BASE + 0x1000U)) /* 0x40011000 */

/* --- IWDG (optional, left disabled by default) --- */
#define IWDG_KR (*(volatile uint32_t *)0x40003000UL)

#ifdef AUTOBAUD
/* --- SysTick (Cortex-M core) --- */
#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)
#endif

/* --- Clocks/baud --- */
#define HSI_FREQ_HZ 16000000UL
#define BAUD        115200UL

/* --- Globals --- */
static USART_TypeDef *usart = USART1;

/* --- tiny delay --- */
static void short_delay(volatile uint32_t n) { while (n--) __asm__ volatile("nop"); }

/* --- Force SYSCLK to HSI with no prescalers --- */
static void force_hsi(void)
{
    RCC_CR |= (1u << 0);                   /* HSION */
    while (!(RCC_CR & (1u << 1))) {}       /* wait for HSIRDY */

    /* SW=00 -> select HSI as SYSCLK; clear AHB/APB1/APB2 prescalers */
    RCC_CFGR = (RCC_CFGR & ~0x0000FCF3u)  /* clear SW, HPRE, PPRE1, PPRE2 */
             | (0u << 0);                  /* SW=00 = HSI */
    while (((RCC_CFGR >> 2) & 0x3u) != 0) {} /* wait for SWS=HSI */
}

#ifdef AUTOBAUD
/* --- Auto-baud: measure host's 0x55 sync byte via GPIO + SysTick ---
   Host must send one 0x55 byte at desired baud rate before communication.
   Measures 8 bit periods from the alternating edges of 0x55 (LSB-first: 1,0,1,0,...).
   Returns the BRR value (SysTick ticks per bit = fPCLK2/baud when prescalers are /1). */
static uint32_t autobaud(void)
{
    /* PA10 → GPIO input with pull-up (idle-high) */
    GPIOA->MODER &= ~(3u << (10*2));
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3u << (10*2))) | (1u << (10*2));

    /* SysTick: free-running 24-bit downcounter at HCLK */
    SYST_CSR = 0;
    SYST_RVR = 0x00FFFFFFu;
    SYST_CVR = 0;
    SYST_CSR = 5;  /* enable, processor clock, no interrupt */

    /* Wait for line idle (high) */
    while (!(GPIOA->IDR & (1u << 10))) { IWDG_KR = 0xAAAAU; }

    /* Wait for start-bit falling edge */
    while (GPIOA->IDR & (1u << 10)) { IWDG_KR = 0xAAAAU; }
    uint32_t t0 = SYST_CVR;

    /* 0x55 LSB-first gives 4 rise+fall pairs after the start bit = 8 bit periods */
    for (int i = 0; i < 4; i++) {
        while (!(GPIOA->IDR & (1u << 10))) {}  /* wait for rising edge */
        while (GPIOA->IDR & (1u << 10)) {}     /* wait for falling edge */
    }
    uint32_t t1 = SYST_CVR;

    SYST_CSR = 0;

    uint32_t elapsed = (t0 - t1) & 0x00FFFFFFu;
    uint32_t brr = (elapsed + 4u) / 8u;

    /* Wait for frame to finish (stop bit → idle) */
    while (!(GPIOA->IDR & (1u << 10))) { IWDG_KR = 0xAAAAU; }

    /* Restore PA10: no pull, will be set to AF by init_usart1 */
    GPIOA->PUPDR &= ~(3u << (10*2));

    return brr;
}
#endif

/* --- USART1 init: PA9=TX, PA10=RX, 115200, 8E1 ---
   Caller must enable GPIOA + USART1 clocks and call force_hsi() first. */
static void init_usart1(void)
{
    /* ---- GPIOA config ----
       PA9  -> AF7 (USART1_TX)
       PA10 -> AF7 (USART1_RX) */
    /* MODER: alternate function for PA9, PA10 */
    GPIOA->MODER &= ~((3u << (9*2)) | (3u << (10*2)));
    GPIOA->MODER |=  ((2u << (9*2)) | (2u << (10*2)));

    /* OTYPER: push-pull on both */
    GPIOA->OTYPER &= ~((1u << 9) | (1u << 10));

    /* OSPEEDR: high speed (optional) */
    GPIOA->OSPEEDR &= ~((3u << (9*2)) | (3u << (10*2)));
    GPIOA->OSPEEDR |=  ((3u << (9*2)) | (3u << (10*2)));

    /* PUPDR: none (or set pull-up on RX if desired) */
    GPIOA->PUPDR &= ~((3u << (9*2)) | (3u << (10*2)));
    /* Uncomment for RX pull-up:
       GPIOA->PUPDR |= (1u << (10*2)); // 01 = pull-up on PA10
    */

    /* AFRH: AF7 for PA9, PA10 (pins 9 & 10 live in AFRH) */
    GPIOA->AFRH &= ~((0xFu << ((9-8)*4)) | (0xFu << ((10-8)*4)));
    GPIOA->AFRH |=  ((7u   << ((9-8)*4)) | (7u    << ((10-8)*4)));

    /* ---- USART1 config: 115200, 8E1, oversampling by 16 ---- */
    usart->CR1 = 0;     /* ensure disabled while configuring */
    usart->CR2 = 0;
    usart->CR3 = 0;

    usart->BRR = (HSI_FREQ_HZ + BAUD / 2u) / BAUD;

    /* M (bit 12)=1, PCE (bit 10)=1, PS (bit 9)=0 (even), TE (3)=1, RE (2)=1, UE(13)=1 */
    usart->CR1 =
        (1u << 12) |  /* M=1 -> 9-bit frame; with parity enabled, data bits = 8 */
        (1u << 10) |  /* PCE = enable parity */
        (0u <<  9) |  /* PS = 0 -> even parity */
        (1u <<  3) |  /* TE = transmit enable */
        (1u <<  2) |  /* RE = receive enable */
        (1u << 13);   /* UE = USART enable */

    short_delay(1000);
}

static void refresh_iwdg(void)
{
    IWDG_KR = 0xAAAAU;
}

static void write_char(uint8_t ch)
{
    /* Wait for TXE (bit 7) */
    while (!(usart->SR & (1u << 7))) { refresh_iwdg(); }
    usart->DR = (uint32_t)ch;
}

static void write_string(const char *s)
{
    if (!s) return;
    while (*s) write_char((uint8_t)*s++);
}

static uint8_t read_char(void)
{
    /* wait for RXNE (bit 5 of SR) */
    while (!(usart->SR & (1u << 5))) {
        refresh_iwdg();
    }
    return (uint8_t)(usart->DR & 0xFFu);
}

static void read_bytes(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        buf[i] = read_char();
    }
}

static uint32_t le32_from_bytes(const uint8_t b[4])
{
    return  (uint32_t)b[0]
          | ((uint32_t)b[1] << 8)
          | ((uint32_t)b[2] << 16)
          | ((uint32_t)b[3] << 24);
}

static uint32_t CRCTable[256];

static void CRC32_init(void)
{
    uint32_t crc32 = 1;
    CRCTable[0] = 0;

    for (unsigned int i = 128; i; i >>= 1) {
        crc32 = (crc32 >> 1) ^ (crc32 & 1 ? 0xedb88320 : 0);
        for (unsigned int j = 0; j < 256; j += 2*i)
            CRCTable[i + j] = crc32 ^ CRCTable[j];
    }
}

uint32_t CRC32(const uint8_t data[], size_t data_length)
{
    uint32_t crc32 = 0xFFFFFFFFu;

    if (CRCTable[255] == 0)
        CRC32_init();
    
    for (size_t i = 0; i < data_length; i++) {
        crc32 ^= data[i];
        crc32 = (crc32 >> 8) ^ CRCTable[crc32 & 0xFF];
    }
    
    // Finalize the CRC-32 value by inverting all the bits
    crc32 ^= 0xFFFFFFFFu;
    return crc32;
}

void alert_crash() {
  write_string("Crashed, restart required\n");
  while (1){refresh_iwdg();}
}

int main_stage1(void)
{
    refresh_iwdg();
    force_hsi();
    RCC_AHB1ENR |= (1u << 0);
    RCC_APB2ENR |= (1u << 4);
    (void)RCC_AHB1ENR;
    short_delay(2000);

#ifdef AUTOBAUD
    uint32_t brr = autobaud();
#endif

    init_usart1();

#ifdef AUTOBAUD
    usart->CR1 &= ~(1u << 13);
    usart->BRR = brr;
    usart->CR1 |= (1u << 13);
#endif

    write_string("Executing payload: stage 1!\r\n");
    while (1){refresh_iwdg();}
}

int main_stage2(void)
{
    refresh_iwdg();

    /* Enable clocks and do GPIO-level auto-baud before full USART init */
    force_hsi();
    RCC_AHB1ENR |= (1u << 0);   /* GPIOAEN */
    RCC_APB2ENR |= (1u << 4);   /* USART1EN */
    (void)RCC_AHB1ENR;
    short_delay(2000);

#ifdef AUTOBAUD
    uint32_t brr = autobaud();   /* host sends 0x55 sync byte */
#endif

    init_usart1();

#ifdef AUTOBAUD
    usart->CR1 &= ~(1u << 13);  /* UE off to change BRR */
    usart->BRR = brr;
    usart->CR1 |= (1u << 13);   /* UE on */
#endif

    write_string("Executing payload: stage 2!\r\n");

    refresh_iwdg();
    CRC32_init();
    refresh_iwdg();

    uint8_t cmd;
    uint8_t data[8];
    uint32_t addr;
    uint32_t len;
    uint8_t *p;

    while (1) {

        cmd = read_char();

        switch (cmd) {
        case CMD_READ:
            read_bytes(data, 8);
            addr = le32_from_bytes(&data[0]);
            len  = le32_from_bytes(&data[4]);

            p = (uint8_t *)addr;
            for (uint32_t i = 0; i < len; ++i) {
                write_char(p[i]);
                if (i%16 == 0){
                    refresh_iwdg();
                }
            }
            break;

        case CMD_TEST:
            write_string("Interface test response\r\n");
            break;

        case CMD_DUMPALL:
            const uint8_t *start = (const uint8_t *)0x08000000UL;
            const uintptr_t end  = 0x08000000UL + ((uint32_t)FLASH_SIZE_KB * 1024UL);
            while ((uintptr_t)start < end) {
                write_char(*start++);
                refresh_iwdg();
            }
            break;

        case CMD_READCRC:
            read_bytes(data, 8);
            addr = le32_from_bytes(&data[0]);
            len  = le32_from_bytes(&data[4]);

            for (uint32_t i = 0; i < 8; ++i) {
                write_char(data[i]);
            }
            refresh_iwdg();

            p = (uint8_t *)addr;
            for (uint32_t i = 0; i < len; ++i) {
                write_char(p[i]);
                if (i%16 == 0){
                    refresh_iwdg();
                }
            }

            refresh_iwdg();
            uint32_t crc32 = CRC32(p, len);
            p = (uint8_t *)&crc32;
            for (uint32_t i = 0; i < 4; ++i) {
                write_char(p[i]);
                refresh_iwdg();
            }
            break;

        default:
            break;
        }

        refresh_iwdg();
    }
}
