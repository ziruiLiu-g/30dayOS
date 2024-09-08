#include <stdio.h>

#include "bootpack.h"
#include "desctbl.h"
#include "graphic.h"
#include "int.h"
#include "io.h"
#include "keyboard.h"
#include "fifo.h"
#include "memory.h"
#include "mouse.h"
#include "sheet.h"
#include "window.h"
#include "timer.h"

int main(void)
{
    struct BootInfo *binfo = (struct BootInfo *)ADR_BOOTINFO;
    struct MemMan *memman = (struct MemMan *) MEMMAN_ADDR;
    unsigned int memtotal, count = 0;
    struct MouseDec mdec;
    char s[40], mcursor[256];
    unsigned char data;
    struct Shtctl *shtctl;
    struct Sheet *sht_back, *sht_mouse, *sht_win;
    unsigned char *buf_back, buf_mouse[256], *buf_win;

    struct FIFO8 timerfifo, timerfifo2, timerfifo3;
    char timerbuf[8], timerbuf2[8], timerbuf3[8];
    struct Timer *timer, *timer2, *timer3;

    init_gdtidt();
    init_pic(); // GDT/IDT完成初始化，开放CPU中断

    io_sti(); // IF(interrupt flag) 变为1， cpu可以开始接受外部指令

    fifo8_init(&keyfifo, KEY_FIFO_BUF_SIZE, keybuf);
    fifo8_init(&mousefifo, MOUSE_FIFO_BUF_SIZE, mousebuf);

    init_pit();

    io_out8(PIC0_IMR, 0xf8); // 开放PIC1以及键盘中断
    io_out8(PIC1_IMR, 0xef); // 开放鼠标中断

    fifo8_init(&timerfifo, 8, timerbuf);
    timer = timer_alloc();
    timer_init(timer, &timerfifo, 1);
    timer_set_timer(timer, 100);

    fifo8_init(&timerfifo2, 8, timerbuf2);
    timer2 = timer_alloc();
    timer_init(timer2, &timerfifo2, 1);
    timer_set_timer(timer2, 300);

    fifo8_init(&timerfifo3, 8, timerbuf3);
    timer3 = timer_alloc();
    timer_init(timer3, &timerfifo3, 1);
    timer_set_timer(timer3, 50);

    init_keyboard();
    enable_mouse(&mdec);

    memtotal = memtest(0x00400000, 0xbfffffff);
    memman_init(memman);
    // 测试时发现会造成错误（原因未知），所以改为由0x00010000开始
    memman_free(memman, 0x00010000, 0x009e000); /* 0x00010000 - 0x0009efff */
    memman_free(memman, 0x00400000, memtotal - 0x00400000);

    init_palette();

    shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
    sht_back = sheet_alloc(shtctl);
    sht_mouse = sheet_alloc(shtctl);
    sht_win = sheet_alloc(shtctl);
    buf_back =
        (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
    buf_win = (unsigned char *)memman_alloc_4k(memman, 160 * 52);
    sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny,
                -1);                               // 没有透明色
    sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); // 透明色号99
    sheet_setbuf(sht_win, buf_win, 160, 52, -1);
    init_screen8(buf_back, binfo->scrnx, binfo->scrny);
    init_mouse_cursor8(buf_mouse, 99); // 背景色号99
    make_window8(buf_win, 160, 52, "Counter");

    sheet_slide(sht_back, 0, 0);

    int mx = (binfo->scrnx - 16) / 2;
    int my = (binfo->scrny - 28 - 16) / 2;

    sheet_slide(sht_mouse, mx, my);
    sheet_slide(sht_win, 80, 72);
    sheet_updown(sht_back, 0);
    sheet_updown(sht_win, 1);
    sheet_updown(sht_mouse, 2);

    // put_block8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);
    sprintf(s, "(%3d, %3d)", mx, my);
    put_fonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);
    sprintf(s, "memory %dMB, free: %dKB", memtotal / (1024 * 1024),
            memman_total(memman) / 1024);
    put_fonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s);
    sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48);

    for (;;)
    {
        // count++;
        sprintf(s, "%010d", timerctl.count);
        box_fill8(buf_win, 160, COL8_C6C6C6, 40, 28,119, 43);
        put_fonts8_asc(buf_win, 160, 40, 28, COL8_000000, s);
        sheet_refresh(sht_win, 40, 28, 120, 44);

        io_cli();
        if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) + fifo8_status(&timerfifo) + fifo8_status(&timerfifo2) + fifo8_status(&timerfifo3) == 0) {
            io_stihlt();
        } else {
            if (fifo8_status(&keyfifo)) {
                data = (unsigned char)fifo8_get(&keyfifo);
                io_sti();
                
                sprintf(s, "%02X", data);
                box_fill8(buf_back, binfo->scrnx, COL8_008484, 0, 16, 15, 31);
                put_fonts8_asc(buf_back, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
                sheet_refresh(sht_back, 0, 16, 16, 32);
            } else if (fifo8_status(&mousefifo)) {
                data = (unsigned char)fifo8_get(&mousefifo);
                io_sti();

                if (mouse_decode(&mdec, data)) {
                    sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
                    if ((mdec.btn & 0x01) != 0) {
                        s[1] = 'L';
                    }
                    if ((mdec.btn & 0x02) != 0) {
                        s[3] = 'R';
                    }
                    if ((mdec.btn & 0x04) != 0) {
                        s[2] = 'C';
                    }
                    box_fill8(buf_back, binfo->scrnx, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
                    put_fonts8_asc(buf_back, binfo->scrnx, 32, 16, COL8_FFFFFF, s);
                    // box_fill8(buf_back, binfo->scrnx, COL8_008484, mx, my, mx + 15, my + 15); // hide mouse
                    sheet_refresh(sht_back, 32, 16, 32 + 15*8, 32);
                    mx += mdec.x;
                    my += mdec.y;
                    if (mx < 0) {
                        mx = 0;
                    }
                    if (my < 0) {
                        my = 0;
                    }
                    if (mx > binfo->scrnx - 1) {
                        mx = binfo->scrnx - 1;
                    }
                    if (my > binfo->scrny - 1) {
                        my = binfo->scrny - 1;
                    }

                    sprintf(s, "[%3d %3d]", mx, my);
                    box_fill8(buf_back, binfo->scrnx, COL8_008484, 0, 0, 79, 15);
                    put_fonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);
                    sheet_refresh(sht_back, 0, 0, 80, 16);
                    sheet_slide(sht_mouse, mx, my);
                }
            } else if (fifo8_status(&timerfifo)) {
                data = fifo8_get(&timerfifo);
                io_sti();
                put_fonts8_asc(buf_back, binfo->scrnx, 0, 64, COL8_FFFFFF, "1[SEC]");
                sheet_refresh(sht_back, 0, 64, 56, 80);
            } else if (fifo8_status(&timerfifo2)) {
                data = fifo8_get(&timerfifo2);
                io_sti();
                put_fonts8_asc(buf_back, binfo->scrnx, 0, 80, COL8_FFFFFF, "3[SEC]");
                sheet_refresh(sht_back, 0, 80, 48, 96);
            } else if (fifo8_status(&timerfifo3)) {
                data = fifo8_get(&timerfifo3);
                io_sti();
                if (data != 0) {
                    timer_init(timer3, &timerfifo3, 0);
                    box_fill8(buf_back, binfo->scrnx, COL8_FFFFFF, 8, 96, 15, 111);
                } else {
                    timer_init(timer3, &timerfifo3, 1);
                    box_fill8(buf_back, binfo->scrnx, COL8_008484, 8, 96, 15, 111);
                }

                timer_set_timer(timer3, 50);
                sheet_refresh(sht_back, 8, 96, 16, 112);
            }
        }
    }

    return 0;
}
