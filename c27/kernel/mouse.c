#include "fifo.h"
#include "io.h"
#include "keyboard.h"
#include "mouse.h"
#include "int.h"

struct FIFO32 *mousefifo;
int mousedata0;

void enable_mouse(struct FIFO32 *fifo, int data0, struct MouseDec *mdec) {
    mousefifo = fifo;
    mousedata0 = data0;

    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
    
    mdec->phase = 0;
}

int mouse_decode(struct MouseDec *mdec, unsigned char dat) {
    if (mdec->phase == 0) {
        if (dat == 0xfa) {
            mdec->phase = 1;
        }
        return 0;
    } else if (mdec->phase == 1) {
        if ((dat & 0xc8) == 0x08) {
            mdec->buf[0] = dat;
            mdec->phase = 2;
        }
        return 0;
    } else if (mdec->phase == 2) {
        mdec->buf[1] = dat;
        mdec->phase = 3;
        return 0;
    } else if (mdec->phase == 3) {
        mdec->buf[2] = dat;
        mdec->phase = 1;
        mdec->btn = mdec->buf[0] & 0x07;
        mdec->x = mdec->buf[1];
        mdec->y = mdec->buf[2];

        if ((mdec->buf[0] & 0x10) != 0) {
            mdec->x |= 0xffffff00;
        }
        if ((mdec->buf[0] & 0x20) != 0) {
            mdec->y |= 0xffffff00;
        }

        mdec->y = -mdec->y; // 鼠标y方向和画面符号相反
        return 1;
    }
    return -1;
}

void int_handler2c(int *esp) {
    // ps/2 mouse interrupt
    int data;

    // 先通知pic1 再通知pic0 转到cpu
    io_out8(PIC1_OCW2, 0x64); // tell pic1  IRQ-12 has been processed
    io_out8(PIC0_OCW2, 0x62); // tell pic0  IRQ-02 has been processed

    data = io_in8(PORT_KEYDAT);
    fifo32_put(mousefifo, data + mousedata0);
    return;
}