/*
 * STM32 System-on-Chip general purpose input/output register definition
 *
 * Copyright 2019 AdaCore
 *
 * Base on nrf51_gpio.c:
 *
 * Copyright 2018 Steffen Görtz <contrib@steffen-goertz.de>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#ifndef STM32_GPIO_H
#define STM32_GPIO_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_STM32_GPIO "stm32.gpio"

typedef struct STM32GPIOState STM32GPIOState;

DECLARE_INSTANCE_CHECKER(STM32GPIOState, STM32_GPIO, TYPE_STM32_GPIO)


#define STM32_GPIO_REG_MODER       0x000
#define STM32_GPIO_REG_OTYPER      0x004
#define STM32_GPIO_REG_OSPEEDR     0x008
#define STM32_GPIO_REG_PUPDR       0x00C
#define STM32_GPIO_REG_IDR         0x010
#define STM32_GPIO_REG_ODR         0x014
#define STM32_GPIO_REG_BSRR        0x018
#define STM32_GPIO_REG_LCKR        0x01C
#define STM32_GPIO_REG_AFRL        0x020
#define STM32_GPIO_REG_AFRH        0x024

#define STM32_GPIO_NPINS           16
#define STM32_GPIO_NREGS           10
#define STM32_GPIO_PERIPHERAL_SIZE 0x400

struct STM32GPIOState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    uint32_t moder;
    uint32_t otyper;
    uint32_t ospeedr;
    uint32_t pupdr;
    uint32_t idr;      /* Actual value of the pin */
    uint32_t odr;      /* Pin value requested by the user */
    uint32_t lckr;
    uint32_t aflr;
    uint32_t afhr;

    uint32_t in;
    uint32_t in_mask;

    qemu_irq input[STM32_GPIO_NPINS];
    qemu_irq output[STM32_GPIO_NPINS];

    /* config */
    uint32_t port;
    uint32_t ngpio;
};

enum STM32GPIOPort {
    STM32_GPIO_PORT_A = 0,
    STM32_GPIO_PORT_B = 1,
    STM32_GPIO_PORT_C = 2,
    STM32_GPIO_PORT_D = 3,
    STM32_GPIO_PORT_E = 4,
    STM32_GPIO_PORT_F = 5,
    STM32_GPIO_PORT_G = 6,
    STM32_GPIO_PORT_H = 7,
    STM32_GPIO_PORT_I = 8,
    STM32_GPIO_PORT_J = 9,
    STM32_GPIO_PORT_K = 10,
};

enum STM32GPIOMode {
    STM32_GPIO_MODE_INPUT = 0,
    STM32_GPIO_MODE_OUTPUT = 1,
    STM32_GPIO_MODE_AF = 2,
    STM32_GPIO_MODE_ANALOG = 3,
};

enum STM32GPIOPull {
    STM32_GPIO_PULL_NONE = 0,
    STM32_GPIO_PULL_UP = 1,
    STM32_GPIO_PULL_DOWN = 2,
};

#endif /* STM32_GPIO_H */
