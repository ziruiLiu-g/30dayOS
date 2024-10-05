#include <stdio.h>

#include "api.h"
#include "console.h"
#include "graphic.h"
#include "task.h"
#include "sheet.h"
#include "window.h"
#include "io.h"
#include "timer.h"
#include "fifo.h"

int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx,
             int eax) {
  struct Task *task = task_now();
  int ds_base = task->ds_base;
  struct Console *cons = task->cons;
  struct Shtctl *shtctl = (struct Shtctl *)*((int *)0x0fe4);
  struct Sheet *sht;
  struct FIFO32 *sys_fifo = (struct FIFO32 *) *((int *) 0x0fec);

  int *reg = &eax + 1;

  if (edx == 1) {
    cons_putchar(cons, eax & 0xff, 1);
  } else if (edx == 2) {
    cons_putstr(cons, (char *)ebx + ds_base);
  } else if (edx == 3) {
    cons_putnstr(cons, (char *)ebx + ds_base, ecx);
  } else if (edx == 4) {
    return &(task->tss.esp0);
  } else if (edx == 5) {
    sht = sheet_alloc(shtctl);
    sht->task = task;
    sht->flags |= 0x10;
    sheet_setbuf(sht, (unsigned char *) ebx + ds_base, esi, edi, eax);
    make_window8((unsigned char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
    sheet_slide(sht, (shtctl->xsize - esi) / 2 & ~3, (shtctl->ysize - edi) / 2);
    sheet_updown(sht, shtctl->top);
    reg[7] = (int) sht;
  } else if (edx == 6) {  // print font on window
    sht = (struct Sheet *) (ebx & 0xfffffffe);
    put_fonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
    if ((ebx & 1) == 0) {
      sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
    }
  } else if (edx == 7) {  // draw block
    sht = (struct Sheet *) (ebx & 0xfffffffe);
    box_fill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
    if ((ebx & 1) == 0) {
      sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
    }
  } else if (edx == 8) {  // memman init
    memman_init((struct MemMan *) (ebx + ds_base));
    ecx &= 0xfffffff0;
    memman_free((struct MemMan *) (ebx + ds_base), eax, ecx);
  } else if (edx == 9) {  // malloc
    ecx = (ecx + 0x0f) & 0xfffffff0;
    reg[7] = memman_alloc((struct MemMan *) (ebx + ds_base), ecx);
  } else if (edx == 10) { // free
    ecx = (ecx + 0x0f) & 0xfffffff0;
    memman_free((struct MemMan *) (ebx + ds_base), eax, ecx);
  } else if (edx == 11) { // draw point
    sht = (struct Sheet *)(ebx & 0xfffffffe);
    sht->buf[sht->bxsize * edi + esi] = eax;
    if (!(ebx & 1)) {
      sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
    }
  } else if (edx == 12) {
    sht = (struct Sheet *) ebx;
    sheet_refresh(sht, eax, ecx, esi, edi);
  } else if (edx == 13) { // draw line
    sht = (struct Sheet *) (ebx & 0xfffffffe);
    api_line_win(sht, eax, ecx, esi, edi, ebp);
    if ((ebx & 1) == 0) {
      sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
    }
  } else if (edx == 14) { // close win
    sheet_free((struct Sheet *) ebx);
  } else if (edx == 15) { // kb input
    for (;;) {
      io_cli();
      if (fifo32_status(&task->fifo) == 0) {
        if (eax != 0) {
          task_sleep(task); /* fifo empty sleep and wait */
        } else {
          io_sti();
          reg[7] = -1;
          return 0;
        }
      }
      int i = fifo32_get(&task->fifo);
      io_sti();
      if (i <= 1) { // timer for cursor
        timer_init(cons->timer, &task->fifo, 1);
        timer_set_timer(cons->timer, 50);
      }
      if (i == 2) { // cursor on
        cons->cur_c = COL8_FFFFFF;
      }
      if (i == 3) { // cursor off
        cons->cur_c = -1;
      }
      if (i == 4) { // close console win
        timer_cancel(cons->timer);
        io_cli();
        fifo32_put(sys_fifo, cons->sheet - shtctl->sheets0 + 2024); /* 2024 - 2279 */
        cons->sheet = NULL;
        io_sti();
      }
      if (256 <= i) { // keyboard input
        reg[7] = i - 256;
        return 0;
      }
    }
  } else if (edx == 16) { // get timer
    reg[7] = (int) timer_alloc();
    ((struct Timer *) reg[7])->flags2 = 1;
  } else if (edx == 17) { // init timer
    timer_init((struct Timer *) ebx, &task->fifo, eax + 256);
  } else if (edx == 18) { // set time
    timer_set_timer((struct Timer *) ebx, eax);
  } else if (edx == 19) { // free timer
    timer_free((struct Timer *) ebx);
  } else if (edx == 20) { // free timer
    if (eax == 0) { // off
      int i = io_in8(0x61);
      io_out8(0x61, i & 0x0d);
    } else { // on
      int i = 1193180000 / eax;
      io_out8(0x43, 0xb6);
      io_out8(0x42, i & 0xff); // page 516
      io_out8(0x42, i >> 8);
      i = io_in8(0x61);
      io_out8(0x61, (i | 0x03) & 0x0f);
    }
  }

  return 0;
}


void api_line_win(struct Sheet *sht, int x0, int y0, int x1, int y1, int col) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  int x = x0 << 10;
  int y = y0 << 10;
  int len = 0;

  if (dx < 0) {
    dx = -dx;
  }
  if (dy < 0) {
    dy = -dy;
  }

  if (dx >= dy) {
    len = dx + 1;
    if (x0 > x1) {
      dx = -1024;
    } else {
      dx = 1024;
    }

    if (y0 <= y1) {
      dy = ((y1 - y0 + 1) << 10) / len;
    } else {
      dy = ((y1 - y0 - 1) << 10) / len;
    }
  } else {
    len = dy + 1;
    if (y0 > y1) {
      dy = -1024;
    } else {
      dy = 1024;
    }

    if (x0 <= x1) {
      dx = ((x1 - x0 + 1) << 10) / len;
    } else {
      dx = ((x1 - x0 - 1) << 10) / len;
    }
  }

  for (int i = 0; i < len; i++) {
    sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
    x += dx;
    y += dy;
  }
}