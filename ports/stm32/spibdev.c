/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/obj.h"
#include "py/mperrno.h"
#include "systick.h"
#include "led.h"
#include "storage.h"

int32_t spi_bdev_ioctl(spi_bdev_t *bdev, uint32_t op, uint32_t arg) {
    switch (op) {
        case BDEV_IOCTL_INIT:
            bdev->spiflash.config = (const mp_spiflash_config_t*)arg;
            mp_spiflash_init(&bdev->spiflash);
            bdev->flash_tick_counter_last_write = 0;
            return 0;

        case BDEV_IOCTL_IRQ_HANDLER:
            if ((bdev->spiflash.flags & 1) && sys_tick_has_passed(bdev->flash_tick_counter_last_write, 1000)) {
                mp_spiflash_flush(&bdev->spiflash);
                led_state(PYB_LED_RED, 0); // indicate a clean cache with LED off
            }
            return 0;

        case BDEV_IOCTL_SYNC:
            if (bdev->spiflash.flags & 1) {
                // we must disable USB irqs to prevent MSC contention with SPI flash
                uint32_t basepri = raise_irq_pri(IRQ_PRI_OTG_FS);
                mp_spiflash_flush(&bdev->spiflash);
                led_state(PYB_LED_RED, 0); // indicate a clean cache with LED off
                restore_irq_pri(basepri);
            }
            return 0;
    }
    return -MP_EINVAL;
}

int spi_bdev_readblocks(spi_bdev_t *bdev, uint8_t *dest, uint32_t block_num, uint32_t num_blocks) {
    // we must disable USB irqs to prevent MSC contention with SPI flash
    uint32_t basepri = raise_irq_pri(IRQ_PRI_OTG_FS);
    mp_spiflash_read(&bdev->spiflash, block_num * FLASH_BLOCK_SIZE, num_blocks * FLASH_BLOCK_SIZE, dest);
    restore_irq_pri(basepri);

    return 0;
}

int spi_bdev_writeblocks(spi_bdev_t *bdev, const uint8_t *src, uint32_t block_num, uint32_t num_blocks) {
    // we must disable USB irqs to prevent MSC contention with SPI flash
    uint32_t basepri = raise_irq_pri(IRQ_PRI_OTG_FS);
    int ret = mp_spiflash_write(&bdev->spiflash, block_num * FLASH_BLOCK_SIZE, num_blocks * FLASH_BLOCK_SIZE, src);
    if (bdev->spiflash.flags & 1) {
        led_state(PYB_LED_RED, 1); // indicate a dirty cache with LED on
        bdev->flash_tick_counter_last_write = HAL_GetTick();
    }
    restore_irq_pri(basepri);

    return ret;
}