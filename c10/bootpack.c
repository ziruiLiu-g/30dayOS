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

int main(void)
{
    struct BootInfo *binfo = (struct BootInfo *)ADR_BOOTINFO;
    struct MemMan *memman = (struct MemMan *) MEMMAN_ADDR;
    struct MouseDec mdec;
    char s[40], mcursor[256];
    unsigned char data;
    unsigned int memtotal;
    struct Shtctl *shtctl;
    struct Sheet *sht_back, *sht_mouse;
    unsigned char *buf_back, buf_mouse[256];

    init_gdtidt();
    init_pic(); // GDT/IDT完成初始化，开放CPU中断

    io_sti(); // IF(interrupt flag) 变为1， cpu可以开始接受外部指令

    fifo8_init(&keyfifo, KEY_FIFO_BUF_SIZE, keybuf);
    fifo8_init(&mousefifo, MOUSE_FIFO_BUF_SIZE, mousebuf);

    io_out8(PIC0_IMR, 0xf9); // 开放PIC1以及键盘中断
    io_out8(PIC1_IMR, 0xef); // 开放鼠标中断

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
    buf_back =
        (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
    sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny,
                -1);                               // 没有透明色
    sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); // 透明色号99
    init_screen8(buf_back, binfo->scrnx, binfo->scrny);
    init_mouse_cursor8(buf_mouse, 99); // 背景色号99
    sheet_slide(shtctl, sht_back, 0, 0);

    int mx = (binfo->scrnx - 16) / 2;
    int my = (binfo->scrny - 28 - 16) / 2;

    sheet_slide(shtctl, sht_mouse, mx, my);
    sheet_updown(shtctl, sht_back, 0);
    sheet_updown(shtctl, sht_mouse, 1);

    // put_block8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);
    sprintf(s, "(%3d, %3d)", mx, my);
    put_fonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);
    sprintf(s, "memory %dMB, free: %dKB", memtotal / (1024 * 1024),
            memman_total(memman) / 1024);
    put_fonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s);
    sheet_refresh(shtctl);

    for (;;)
    {
        io_cli();
        if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) {
            io_stihlt();
        } else {
            if (fifo8_status(&keyfifo)) {
                data = (unsigned char)fifo8_get(&keyfifo);
                io_sti();
                
                sprintf(s, "%02X", data);
                box_fill8(buf_back, binfo->scrnx, COL8_008484, 0, 16, 15, 31);
                put_fonts8_asc(buf_back, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
                sheet_refresh(shtctl);
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
                    sheet_refresh(shtctl);
                    mx += mdec.x;
                    my += mdec.y;
                    if (mx < 0) {
                        mx = 0;
                    }
                    if (my < 0) {
                        my = 0;
                    }
                    if (mx > binfo->scrnx - 16) {
                        mx = binfo->scrnx - 16;
                    }
                    if (my > binfo->scrny - 16) {
                        my = binfo->scrny - 16;
                    }

                    sprintf(s, "[%3d %3d]", mx, my);
                    box_fill8(buf_back, binfo->scrnx, COL8_008484, 0, 0, 79, 15);
                    put_fonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);
                    // put_block8_8(buf_back, binfo->scrnx, 16, 16, mx, my, mcursor, 16);
                    sheet_slide(shtctl, sht_mouse, mx, my);
                }
            }
        }
    }

    return 0;
}
