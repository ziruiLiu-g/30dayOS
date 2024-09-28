#include <stdio.h>
#include <string.h>

#include "bootpack.h"
#include "desctbl.h"
#include "fifo.h"
#include "graphic.h"
#include "int.h"
#include "io.h"
#include "keyboard.h"
#include "memory.h"
#include "mouse.h"
#include "sheet.h"
#include "task.h"
#include "timer.h"
#include "window.h"
#include "console.h"
#include "fs.h"
#include "app.h"

int main(void)
{
    struct BootInfo *binfo = (struct BootInfo *)ADR_BOOTINFO;
    struct MemMan *memman = (struct MemMan *) MEMMAN_ADDR;
    unsigned int memtotal;
    struct MouseDec mdec;
    char s[40], mcursor[256];
    struct Shtctl *shtctl;
    struct Sheet *sht_back, *sht_mouse, *sht_win, *sht_cons;
    unsigned char *buf_back, buf_mouse[256], *buf_win, *buf_cons;

    struct Console *cons;

    struct FIFO32 fifo, keycmd;
    int fifobuf[128], keycmd_buf[32];
    // caplock at 6th bit of binfo->leds, right shift 4 bit and '&7' we get the 4th 5th 6th bit status
    // bin 7 = 111
    int data, key_to = 0, key_shift = 0, key_ctl = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;

    struct Timer *timer;

    init_gdtidt();
    init_pic(); // GDT/IDT完成初始化，开放CPU中断
    io_sti(); // IF(interrupt flag) 变为1， cpu可以开始接受外部指令
    

    fifo32_init(&fifo, 128, fifobuf, 0);
    fifo32_init(&keycmd, 32, keycmd_buf, 0);
    init_keyboard(&fifo, 256);
    enable_mouse(&fifo, 512, &mdec);

    init_pit();

    io_out8(PIC0_IMR, 0xf8); // 开放PIC1以及键盘中断
    io_out8(PIC1_IMR, 0xef); // 开放鼠标中断

    memtotal = memtest(0x00400000, 0xbfffffff);
    memman_init(memman);
    // 书中为0x00001000 ~ 0x0009e000
    // 注1: 测试时发现会造成错误（原因未知），所以改为由0x00010000开始
    // 注2: 在640*480模式下，free该段内存后会导致屏幕画面错误
    // memman_free(memman, 0x00010000, 0x0009e000); // 0x00010000 ~ 0x0009efff
    // memman_free(memman, 0x00010000, 0x009e000); /* 0x00010000 - 0x0009efff */
    memman_free(memman, 0x00400000, memtotal - 0x00400000);

    init_palette();

    shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
    *((int *) 0x0fe4) = (int) shtctl;
    struct Task *task_a = task_init(memman);
    fifo.task = task_a;
    task_run(task_a, 1, 0);

    /* For back */
    sht_back = sheet_alloc(shtctl);
    buf_back =
        (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
    sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny,
                -1);                               // 没有透明色
    init_screen8(buf_back, binfo->scrnx, binfo->scrny);

    /* For Console */
    sht_cons = sheet_alloc(shtctl);
    buf_cons = (unsigned char *) memman_alloc_4k(memman, 256 * 165);
    sheet_setbuf(sht_cons, buf_cons, 256, 165, -1);
    make_window8(buf_cons, 256, 165, "console",  0);
    make_textbox8(sht_cons, 8, 28, 240, 128, COL8_000000);
    struct Task *task_cons = task_alloc();
    task_cons->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 12; // address of tail of stack. 
    task_cons->tss.eip = (int) &console_task;
    task_cons->tss.es = 1 * 8;
    task_cons->tss.cs = 2 * 8;
    task_cons->tss.ss = 1 * 8;
    task_cons->tss.ds = 1 * 8;
    task_cons->tss.fs = 1 * 8;
    task_cons->tss.gs = 1 * 8;
    *((int *) (task_cons->tss.esp + 4)) = (int) sht_cons;
    *((int *) (task_cons->tss.esp + 8)) = (int) memtotal;
    task_run(task_cons, 2, 2); // level = 2; priority = 2;

    /* For Win */
    sht_win = sheet_alloc(shtctl);
    buf_win = (unsigned char *)memman_alloc_4k(memman, 160 * 52);
    sheet_setbuf(sht_win, buf_win, 160, 52, -1); // 没有透明色
    make_window8(buf_win, 160, 52, "task_a", 1);
    make_textbox8(sht_win, 8, 28, 128, 16, COL8_FFFFFF);
    int cursor_x = 8;
    int cursor_c = COL8_FFFFFF;
    timer = timer_alloc();
    timer_init(timer, &fifo, 1);
    timer_set_timer(timer, 50);

    /* For Mouse */
    sht_mouse = sheet_alloc(shtctl);
    sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); // 透明色号99
    init_mouse_cursor8(buf_mouse, 99); // 背景色号99
    int mx = (binfo->scrnx - 16) / 2;
    int my = (binfo->scrny - 28 - 16) / 2;

    sheet_slide(sht_back, 0, 0);
    sheet_slide(sht_cons, 32, 4);
    sheet_slide(sht_win, 80, 72);
    sheet_slide(sht_mouse, mx, my);
    sheet_updown(sht_back, 0);
    sheet_updown(sht_cons, 1);
    sheet_updown(sht_win, 2);
    sheet_updown(sht_mouse, 3);
    // sprintf(s, "(%3d, %3d)", mx, my);
    // put_fonts8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
    // sprintf(s, "memory %dMB, free: %dKB", memtotal / (1024 * 1024),
    //         memman_total(memman) / 1024);
    // put_fonts8_asc_sht(sht_back, 0, 32, COL8_FFFFFF, COL8_008484, s, 40);

    // avoid keyboard conflict
    fifo32_put(&keycmd, KEYCMD_LED);
    fifo32_put(&keycmd, key_leds);

    for (;;)
    {
        if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) {
            keycmd_wait = fifo32_get(&keycmd);
            wait_KBC_sendready();
            io_out8(PORT_KEYDAT, keycmd_wait);
        }

        io_cli();
        if (fifo32_status(&fifo) == 0) {
            task_sleep(task_a);
            io_sti();
        } else {
            data = fifo32_get(&fifo);
            io_sti();
            
            if (256 <= data && data <= 511) { // keyboard

                if (data < 0x80 + 256) {
                    if (key_shift == 0) {
                        s[0] = keytable0[data - 256];
                    } else {
                        s[0] = keytable1[data - 256];
                    }
                } else {
                    s[0] = 0;
                }
                if ('A' <= s[0] && s[0] <= 'Z') {
                    // 4 = 100
                    // key_leds & 4 gets 3rd bit of key_leds, which is the 6th bit of binfo->leds
                    if ((!(key_leds & 4) && !key_shift) || ((key_leds & 4) && key_shift)) {
                        s[0] += 0x20;
                    }
                }

                if (!key_ctl && s[0]) {
                    if (!key_to) {
                        if (cursor_x < 128) {
                        // 显示一个字符之后将光标后移一位
                            s[1] = '\0';
                            put_fonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000,
                                                COL8_FFFFFF, s, 1);
                            cursor_x += 8;
                        }
                    } else {
                        fifo32_put(&task_cons->fifo, s[0] + 256);
                    }
                }
                if (data == 256 + 0x2e && key_ctl != 0 && task_cons->tss.ss0 != 0) {
                    cons = (struct Console *) *((int *) 0x0fec);
                    cons_putstr(cons, "\nBreak(key):\n");
                    io_cli();
                    task_cons->tss.eax = (int) &(task_cons->tss.esp0);
                    task_cons->tss.eip = (int) asm_end_app;
                    io_sti();
                }
                if (data == 256 + 0x0e) {   // backspace
                    if (key_to == 0) {
                        if (cursor_x > 8) {
                            put_fonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
                            cursor_x -= 8;
                        }
                    } else {
                        fifo32_put(&task_cons->fifo, 8 + 256);
                    }
                }
                if (data == 256 + 0x0f) {   // tab
                    if (key_to == 0) {
                        key_to = 1;
                        make_window_title8(buf_win, sht_win->bxsize, "task_a", 0);
                        make_window_title8(buf_cons, sht_cons->bxsize, "console", 1);
                        cursor_c = -1;
                        box_fill8(sht_win->buf, sht_win->bxsize, COL8_FFFFFF, cursor_x, 28, cursor_x + 7, 43);
                        fifo32_put(&task_cons->fifo, 2); // cursor on
                    } else {
                        key_to = 0;
                        make_window_title8(buf_win, sht_win->bxsize, "task_a", 1);
                        make_window_title8(buf_cons, sht_cons->bxsize, "console", 0);
                        cursor_c = COL8_000000;
                        fifo32_put(&task_cons->fifo, 3); // cursor off
                    }

                    sheet_refresh(sht_win, 0, 0, sht_win->bxsize, 21);
                    sheet_refresh(sht_cons, 0, 0, sht_cons->bxsize, 21);
                }
                if (data == 256 + 0x2a) { // left shift on
                    key_shift |= 1;
                }
                if (data == 256 + 0x36) { // right shift on
                    key_shift |= 2;
                }
                if (data == 256 + 0xaa) { // left shift off
                    key_shift &= ~1;
                }
                if (data == 256 + 0xb6) { // left shift off
                    key_shift &= ~2;
                }
                if (data == 256 + 0x1d) { // ctl on
                    key_ctl |= 1;
                }
                if (data == 256 + 0x9d) { // ctl off
                    key_ctl &= ~1;
                }
                if (data == 256 + 0x3a) {  // capslock
                    key_leds ^= 4;
                    fifo32_put(&keycmd, KEYCMD_LED);
                    fifo32_put(&keycmd, key_leds);
                }
                if (data == 256 + 0x45) {  // numlock
                    key_leds ^= 2;
                    fifo32_put(&keycmd, KEYCMD_LED);
                    fifo32_put(&keycmd, key_leds);
                }
                if (data == 256 + 0x46) {  // scrollLock
                    key_leds ^= 1;
                    fifo32_put(&keycmd, KEYCMD_LED);
                    fifo32_put(&keycmd, key_leds);
                }
                if (data == 256 + 0x1c) { // enter
                    if (key_to != 0) {
                        fifo32_put(&task_cons->fifo, 10 + 256);
                    }
                }
                if (data == 256 + 0xfa) {   // keyboard get data
                    keycmd_wait = -1;
                }
                if (data == 256 + 0xfe) {   // keyboard fail to get data
                    wait_KBC_sendready();
                    io_out8(PORT_KEYDAT, keycmd_wait);
                }

                if (cursor_c >= 0) {
                    box_fill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
                }
                sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
            } else if (512 <= data && data <= 767) { // mouse
                if (mouse_decode(&mdec, data - 512)) {
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

                    sheet_slide(sht_mouse, mx, my);

                    if (mdec.btn & 0x01) {
                        sheet_slide(sht_win, mx - 80, my - 8);
                    }
                }
            } else if (data <= 1) {
                if (data) {
                    timer_init(timer, &fifo, 0);
                    if (cursor_c >= 0) {
                        cursor_c = COL8_000000;
                    }
                } else {
                    timer_init(timer, &fifo, 1);
                    if (cursor_c >= 0) {
                        cursor_c = COL8_FFFFFF;
                    }
                }
                
                timer_set_timer(timer, 50);
                if (cursor_c >= 0) {
                    box_fill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
                    sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);  
                }   
            }
        }
    }

    return 0;
}
