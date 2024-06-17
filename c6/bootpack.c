#include <stdio.h>

#include "bootpack.h"
#include "desctbl.h"
#include "graphic.h"
#include "int.h"
#include "io.h"


int main(void)
{
    char s[40], mcursor[256];
    struct BootInfo *binfo = (struct BootInfo *)ADR_BOOTINFO;

    init_gdtidt();
    init_pic(); // GDT/IDT完成初始化，开放CPU中断
    io_sti(); // IF(interrupt flag) 变为1， cpu可以开始接受外部指令

    init_palette();
    init_screen(binfo->vram, binfo->scrnx, binfo->scrny);

    put_fonts8_asc(binfo->vram, binfo->scrnx, 8, 8, COL8_FFFFFF, "ABC 123");
    put_fonts8_asc(binfo->vram, binfo->scrnx, 31, 31, COL8_000000,
                    "Haribote OS.");
    put_fonts8_asc(binfo->vram, binfo->scrnx, 30, 30, COL8_FFFFFF,
                 "Haribote OS.");

    sprintf(s, "scrnx = %d", binfo->scrnx);
    put_fonts8_asc(binfo->vram, binfo->scrnx, 16, 64, COL8_FFFFFF, s);

    int mx = (binfo->scrnx - 16) / 2;
    int my = (binfo->scrny - 28 - 16) / 2;
    init_mouse_cursor8(mcursor, COL8_008484);
    putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);

    io_out8(PIC0_IMR, 0xf9); // 开放PIC1以及键盘中断
    io_out8(PIC1_IMR, 0xef); // 开放鼠标中断

    for (;;)
    {
        io_hlt();
    }

    return 0;
}
