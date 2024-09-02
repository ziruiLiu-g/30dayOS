#include <stdio.h>

#include "bootpack.h"
#include "graphic.h"
#include "int.h"
#include "io.h"
#include "keyboard.h"
#include "fifo.h"
#include "mouse.h"

void init_pic(void) {
    io_out8(PIC0_IMR, 0xff);
    io_out8(PIC1_IMR, 0xff);

    io_out8(PIC0_ICW1, 0x11); // edge trigger mode
    io_out8(PIC0_ICW2, 0x20); // irq0-7 -> int20-27
    io_out8(PIC0_ICW3, 1 << 2);
    io_out8(PIC0_ICW4, 0x01); // no buffer

    io_out8(PIC1_ICW1, 0x11);
    io_out8(PIC1_ICW2, 0x28); // irq8-15 -> int28-2f
    io_out8(PIC1_ICW3, 2);
    io_out8(PIC1_ICW4, 0x01);

    io_out8(PIC0_IMR, 0xfb); // 11111011 ban except pic1
    io_out8(PIC1_IMR, 0xff); // 11111111

    return;
}

void int_handler21(int *esp) {
    // ps/2 keyboard interrupt
    struct BootInfo *binfo = (struct BootInfo *) ADR_BOOTINFO;

    unsigned char data;
    io_out8(PIC0_OCW2, 0x61); // tell pic  IRQ-01 has been processed. to Tell pic what IRQ is triggered, just use (0x60 + IRQ number).
    data = io_in8(PORT_KEYDAT);

    fifo8_put(&keyfifo, data);

    return;
}

void int_handler2c(int *esp) {
    // ps/2 mouse interrupt
    unsigned char data;

    // 先通知pic1 再通知pic0 转到cpu
    io_out8(PIC1_OCW2, 0x64); // tell pic1  IRQ-12 has been processed
    io_out8(PIC0_OCW2, 0x62); // tell pic0  IRQ-02 has been processed

    data = io_in8(PORT_KEYDAT);
    fifo8_put(&mousefifo, data);
    return;
}

void int_handler27(int *esp) {
	io_out8(PIC0_OCW2, 0x67);

	return;
}
