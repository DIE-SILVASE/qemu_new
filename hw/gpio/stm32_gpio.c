/*
 * STM32 System-on-Chip general purpose input/output register definition
 *
 * Copyright 2024 Román Cárdenas <rcardenas.rod@gmail.com>
 *
 * Based on sifive_gpio.c:
 *
 * Copyright 2019 AdaCore
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 */

#include <stdio.h>

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/gpio/stm32_gpio.h"
#include "migration/vmstate.h"
#include "trace.h"

static void update_state(STM32GPIOState *s)
{
    bool prev_id, new_id, od, in, in_mask;
    uint8_t mode, pupd;

    for (size_t i = 0; i < s->ngpio; i++) {
        prev_id = extract32(s->idr, i, 1);
        od      = extract32(s->odr, i, 1);
        in      = extract32(s->in, i, 1);
        in_mask = extract32(s->in_mask, i, 1);

        mode    = extract32(s->moder, i * 2, 2);
        pupd    = extract32(s->pupdr, i * 2, 2);

        /* Pin both driven externally and internally */
        if (mode == STM32_GPIO_MODE_OUTPUT && in_mask) {
            qemu_log_mask(LOG_GUEST_ERROR, "GPIO pin %zu short circuited\n", i);
        }

        if (in_mask) {
            /* The pin is driven by external device */
            new_id = in;
        } else if (mode == STM32_GPIO_MODE_OUTPUT) {
            /* The pin is driven by internal circuit */
            new_id = od;
        } else {
            /* Floating? Apply pull-up resistor */
            new_id = pupd == STM32_GPIO_PULL_UP;
        }

        /* Update IDR */
        s->idr = deposit32(s->idr, i, 1, new_id);

        // Trigger output interrupts ()
        if (mode == STM32_GPIO_MODE_OUTPUT && new_id != prev_id) {
            qemu_set_irq(s->output[i], new_id);
        }

        // TODO trigger input interrupt (needs access to EXTI)
    }
}

static uint64_t stm32_gpio_read(void *opaque, hwaddr offset, unsigned int size)
{
    STM32GPIOState *s = STM32_GPIO(opaque);
    uint64_t r = 0;

    switch (offset) {
    case STM32_GPIO_REG_MODER:
        r = s->moder;
        break;

    case STM32_GPIO_REG_OTYPER:
        r = s->otyper;
        break;

    case STM32_GPIO_REG_OSPEEDR:
        r = s->ospeedr;
        break;

    case STM32_GPIO_REG_PUPDR:
        r = s->pupdr;
        break;

    case STM32_GPIO_REG_IDR:
        r = s->idr;
        break;

    case STM32_GPIO_REG_ODR:
        r = s->odr;
        break;

    case STM32_GPIO_REG_BSRR:
        r = 0; // BSRR is write-only
        break;

    case STM32_GPIO_REG_LCKR:
        r = s->lckr;
        break;

    case STM32_GPIO_REG_AFRL:
        r = s->aflr;
        break;

    case STM32_GPIO_REG_AFRH:
        r = s->afhr;
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad read offset 0x%" HWADDR_PRIx "\n",  __func__, offset);
    }

    trace_stm32_gpio_read(offset, r);

    return r;
}

static void stm32_gpio_write(void *opaque, hwaddr offset, uint64_t value, unsigned int size)
{
    STM32GPIOState *s = STM32_GPIO(opaque);

    trace_stm32_gpio_write(offset, value);

    switch (offset) {

    case STM32_GPIO_REG_MODER:
        s->moder = value;
        break;

    case STM32_GPIO_REG_OTYPER:
        s->otyper = value;
        break;

    case STM32_GPIO_REG_OSPEEDR:
        s->ospeedr = value;
        break;

    case STM32_GPIO_REG_PUPDR:
        s->pupdr = value;
        break;
    
    case STM32_GPIO_REG_IDR:
        break; // IDR is read-only

    case STM32_GPIO_REG_ODR:
        s->odr = value; // IDR is updated in update_state
        break;

    case STM32_GPIO_REG_BSRR:
        s->odr &= ~((value >> 16) & 0xFFFF);
        s->odr |= value & 0xFFFF; // set bits have higher priority than reset bits
        break;

    case STM32_GPIO_REG_LCKR:
        s->lckr = value;
        break;

    case STM32_GPIO_REG_AFRL:
        s->aflr = value;
        break;

    case STM32_GPIO_REG_AFRH:
        s->afhr = value;
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write offset 0x%" HWADDR_PRIx "\n", __func__, offset);
    }

    update_state(s);
}

static const MemoryRegionOps gpio_ops = {
    .read =  stm32_gpio_read,
    .write = stm32_gpio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

static void stm32_gpio_set(void *opaque, int line, int value)
{
    STM32GPIOState *s = STM32_GPIO(opaque);

    trace_stm32_gpio_set(line, value);

    assert(line >= 0 && line < STM32_GPIO_NPINS);

    s->in_mask = deposit32(s->in_mask, line, 1, value >= 0);
    if (value >= 0) {
        s->in = deposit32(s->in, line, 1, value != 0);
    }

    update_state(s);
}

static void stm32_gpio_reset(DeviceState *dev)
{
    STM32GPIOState *s = STM32_GPIO(dev);

    /* Reset values of ODR, OSPEEDR, and PUPDR depends on GPIO port */
    if (s->port == STM32_GPIO_PORT_A) {
        s->odr = 0xA8000000;
        s->ospeedr = 0;
        s->pupdr = 0x64000000;
    } else if (s->port == STM32_GPIO_PORT_B) {
        s->odr = 0x00000280;
        s->ospeedr = 0x000000C0;
        s->pupdr = 0x00000100;
    } else {
        s->odr = 0;
        s->ospeedr = 0;
        s->pupdr = 0;
    }

    s->otyper = 0;
    s->idr = 0;
    s->odr = 0;
    s->lckr = 0;
    s->aflr = 0;
    s->afhr = 0;

    s->in = 0;
    s->in_mask = 0;
}

static const VMStateDescription vmstate_STM32_gpio = {
    .name = TYPE_STM32_GPIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(moder,    STM32GPIOState),
        VMSTATE_UINT32(otyper,   STM32GPIOState),
        VMSTATE_UINT32(ospeedr,  STM32GPIOState),
        VMSTATE_UINT32(pupdr,    STM32GPIOState),
        VMSTATE_UINT32(idr,      STM32GPIOState),
        VMSTATE_UINT32(odr,      STM32GPIOState),
        VMSTATE_UINT32(lckr,     STM32GPIOState),
        VMSTATE_UINT32(aflr,     STM32GPIOState),
        VMSTATE_UINT32(afhr,     STM32GPIOState),
        VMSTATE_UINT32(in,      STM32GPIOState),
        VMSTATE_UINT32(in_mask, STM32GPIOState),
        VMSTATE_END_OF_LIST()
    }
};

static Property STM32_gpio_properties[] = {
    DEFINE_PROP_UINT32("ngpio", STM32GPIOState, ngpio, STM32_GPIO_NPINS),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32_gpio_realize(DeviceState *dev, Error **errp)
{
    STM32GPIOState *s = STM32_GPIO(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &gpio_ops, s, TYPE_STM32_GPIO, STM32_GPIO_PERIPHERAL_SIZE);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);

    for (int i = 0; i < STM32_GPIO_NPINS; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->input[i]);
    }

    qdev_init_gpio_in(DEVICE(s), stm32_gpio_set, STM32_GPIO_NPINS);
    qdev_init_gpio_out(DEVICE(s), s->output, STM32_GPIO_NPINS);
}

static void stm32_gpio_class_init(ObjectClass *klass, void *data)
{
    printf("stm32_gpio_class_init\n");
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, STM32_gpio_properties);
    dc->vmsd = &vmstate_STM32_gpio;
    dc->realize = stm32_gpio_realize;
    dc->reset = stm32_gpio_reset;
    dc->desc = "STM32 GPIO";
}

static const TypeInfo STM32_gpio_info = {
    .name = TYPE_STM32_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32GPIOState),
    .class_init = stm32_gpio_class_init
};

static void STM32_gpio_register_types(void)
{
    type_register_static(&STM32_gpio_info);
}

type_init(STM32_gpio_register_types)
