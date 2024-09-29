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
    struct Sheet *sht_back, *sht_mouse;
    unsigned char *buf_back, buf_mouse[256];
    struct Task *task_cons[2], *task;

    struct Console *cons;

    struct FIFO32 fifo, keycmd;
    int fifobuf[128], keycmd_buf[32];
    // caplock at 6th bit of binfo->leds, right shift 4 bit and '&7' we get the 4th 5th 6th bit status
    // bin 7 = 111
    int data, key_to = 0, key_shift = 0, key_ctl = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;

    struct Timer *timer;

    int j, x, y;
    int mmx, mmy, mmx2; // position before mouse move window
    int mx, my, new_mx = -1, new_my = 0, new_wx = 0x7fffffff, new_wy = 0;
    struct Sheet *sht = 0, *key_win;


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
    key_win = open_console(shtctl, memtotal);

    timer = timer_alloc();
    timer_init(timer, &fifo, 1);
    timer_set_timer(timer, 50);

    /* For Mouse */
    sht_mouse = sheet_alloc(shtctl);
    sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); // 透明色号99
    init_mouse_cursor8(buf_mouse, 99); // 背景色号99
    mx = (binfo->scrnx - 16) / 2;
    my = (binfo->scrny - 28 - 16) / 2;

    sheet_slide(sht_back, 0, 0);
    sheet_slide(key_win, 32, 4);
    sheet_slide(sht_mouse, mx, my);
    sheet_updown(sht_back, 0);
    sheet_updown(key_win, 1);
    sheet_updown(sht_mouse, 2);
    fifo32_put(&keycmd, KEYCMD_LED);
    fifo32_put(&keycmd, key_leds);
    keywin_on(key_win);

    *((int *) 0x0fec) = (int) &fifo;

    for (;;)
    {
        if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) {
            keycmd_wait = fifo32_get(&keycmd);
            wait_KBC_sendready();
            io_out8(PORT_KEYDAT, keycmd_wait);
        }

        io_cli();
        if (fifo32_status(&fifo) == 0) {
            if (new_mx >= 0) {
                io_sti();
                sheet_slide(sht_mouse, new_mx, new_my);
                new_mx = -1;
            } else if (new_wx != 0x7fffffff) {
                io_sti();
                sheet_slide(sht, new_wx, new_wy);
                new_wx = 0x7fffffff;
            } else {
                task_sleep(task_a);
                io_sti();
            }
        } else {
            data = fifo32_get(&fifo);
            io_sti();
            if (key_win != NULL && !key_win->flags) {
                if (shtctl->top == 1) {
                    key_win = NULL;
                } else {
                    key_win = shtctl->sheets[shtctl->top - 1];
                    keywin_on(key_win);
                }
            }
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

                if (!key_ctl && s[0] && key_win != NULL) {
                    fifo32_put(&key_win->task->fifo, s[0] + 256);
                }
                if (data == 256 + 0x2e && key_ctl != 0 && key_win != NULL) {
                    task = key_win->task;
                    if (task != 0 && task->tss.ss0 != 0) {
                        cons_putstr(task->cons, "\nBreak(key):\n");
                        io_cli();
                        task->tss.eax = (int) &(task->tss.esp0);
                        task->tss.eip = (int) asm_end_app;
                        io_sti();
                    }
                }
                if (data == 256 + 0x3c && key_shift != 0) {
                    if (key_win != 0) {
                        keywin_off(key_win);
                    }
                    key_win = open_console(shtctl, memtotal);
                    sheet_slide(key_win, 32, 4);
                    sheet_updown(key_win, shtctl->top);
                    keywin_on(key_win);
                }
                if (data == 256 + 0x0e) {   // backspace
                    fifo32_put(&key_win->task->fifo, 8 + 256);
                }
                if (data == 256 + 0x0f && key_win != 0) {   // tab
                    keywin_off(key_win);
                    j = key_win->height - 1; // press tab, switch to next windown
                    if (j == 0) {
                        j = shtctl->top - 1;
                    }
                    key_win = shtctl->sheets[j];
                    keywin_on(key_win);
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
                    fifo32_put(&key_win->task->fifo, 10 + 256);
                }
                if (data == 256 + 0x1d && shtctl->top > 2) {
                    sheet_updown(shtctl->sheets[1], shtctl->top - 1);
                }
                if (data == 256 + 0xfa) {   // keyboard get data
                    keycmd_wait = -1;
                }
                if (data == 256 + 0xfe) {   // keyboard fail to get data
                    wait_KBC_sendready();
                    io_out8(PORT_KEYDAT, keycmd_wait);
                }
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
                    new_mx = mx;
                    new_my = my;
                    if (mdec.btn & 0x01) {
                        // mouse left click
                        // search sheet from top to bottom
                        if (mmx < 0) {
                            for (j = shtctl->top - 1; j > 0; j--) {
                                sht = shtctl->sheets[j];
                                x = mx - sht->vx0;
                                y = my - sht->vy0;
                                if (0 <= x && x < sht->bxsize && 0 <= y && y < sht->bysize) {
                                    if (sht->buf[y * sht->bxsize + x] != sht->col_inv) {
                                        sheet_updown(sht, shtctl->top - 1);
                                        if (sht != key_win) {
                                            keywin_off(key_win);
                                            key_win = sht;
                                            keywin_on(key_win);
                                        }
                                        if (3 <= x && x < sht->bxsize - 3 && 3 <= y && y < 21) {
                                            mmx = mx;
                                            mmy = my;
                                            mmx2 = sht->vx0;
                                            new_wy = sht->vy0;
                                        }       
                                        if (sht->bxsize - 21 <= x && x < sht->bxsize - 5 && 5 <= y && y < 19) {
                                            // click x "close"
                                            if ((sht->flags & 0x10) != 0) { // if app window?
                                                task = sht->task;
                                                cons_putstr(task->cons, "\nBreak(mouse) :\n");
                                                io_cli();
                                                task->tss.eax = (int) &(task->tss.esp0);
                                                task->tss.eip = (int) asm_end_app;
                                                io_sti();
                                            } else { // console window
                                                task = sht->task;
                                                io_cli();
                                                fifo32_put(&task->fifo, 4);
                                                io_sti();
                                            }
                                        }     
                                        break;                        
                                    }
                                }
                            }
                        } else {
                            x = mx - mmx;
                            y = my - mmy;
                            new_wx = (mmx2 + x + 2) & ~3;
                            new_wy = new_wy + y;
                            mmy = my;
                        }
                    } else {
                        mmx = -1;
                        if (new_wx != 0x7fffffff) {
                            sheet_slide(sht, new_wx, new_wy);
                            new_wx = 0x7fffffff;
                        }
                    }
                }
            } else if (768 <= data && data <= 1023) {
                close_console(shtctl->sheets0 + (data - 768));
            } else if (1024 <= data && data <= 2023) {
                close_cons_task(taskctl->tasks0 + (data - 1024));
            }
        }
    }

    return 0;
}
