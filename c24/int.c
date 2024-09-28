#include <stdio.h>

#include "bootpack.h"
#include "graphic.h"
#include "int.h"
#include "io.h"
#include "keyboard.h"
#include "fifo.h"
#include "mouse.h"
#include "console.h"
#include "task.h"

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

void int_handler27(int *esp) {
	io_out8(PIC0_OCW2, 0x67);

	return;
}

int *int_handler0c(int *esp) {
    struct Console *cons = (struct Console *)*((int *) 0x0fec);
    struct Task *task = task_now();
    char s[30];

    cons_putstr(cons, "\nINT 0C:\n Stack Exception.\n");
    sprintf(s, "EIP = %08X\n", esp[11]);
    cons_putstr(cons, s);

    return &(task->tss.esp0);
}

int *int_handler0d(int *esp) {
    struct Console *cons = (struct Console *)*((int *) 0x0fec);
    struct Task *task = task_now();
    char s[30];

    cons_putstr(cons, "\nINT 0D:\n General Protected Exception.\n");
    sprintf(s, "EIP = %08X\n", esp[11]);
    cons_putstr(cons, s);

    return &(task->tss.esp0);
}