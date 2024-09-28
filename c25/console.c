#include <stdio.h>
#include <string.h>

#include "bootpack.h"
#include "command.h"
#include "console.h"
#include "desctbl.h"
#include "fifo.h"
#include "fs.h"
#include "graphic.h"
#include "io.h"
#include "memory.h"
#include "mouse.h"
#include "sheet.h"
#include "task.h"
#include "timer.h"

void console_task(struct Sheet *sheet, unsigned int memtotal) {
  struct Task *task = task_now();
  char s[30], cmdline[30];
  struct MemMan *memman = (struct MemMan *)MEMMAN_ADDR;
  int *fat = (int *)memman_alloc_4k(memman, 4 * 2880);
  struct SegmentDescriptor *gdt = (struct SegmentDescriptor *)ADR_GDT;
  int x, y;
  struct Console cons;

  cons.sheet = sheet;
  cons.cur_x = 8;
  cons.cur_y = 28;
  cons.cur_c = -1;
  task->cons = &cons;
  
  struct Timer *timer = timer_alloc();
  timer_init(timer, &task->fifo, 1);
  timer_set_timer(timer, 50);

  file_read_fat(fat, (unsigned char *)(ADR_DISKIMG + 0x000200));

  cons_putchar(&cons, '>', 1);

  for (;;) {
    io_cli();

    if (!fifo32_status(&task->fifo)) {
      task_sleep(task);
      io_sti();
    } else {
      int data = fifo32_get(&task->fifo);
      io_sti();
      if (data <= 1) {
        if (data) {
          timer_init(timer, &task->fifo, 0);
          if (cons.cur_c >= 0) {
            cons.cur_c = COL8_FFFFFF;
          }
        } else {
          timer_init(timer, &task->fifo, 1);
          if (cons.cur_c >= 0) {
            cons.cur_c = COL8_000000;
          }
        }

        timer_set_timer(timer, 50);
      }

      // 光标ON
      if (data == 2) {
        cons.cur_c = COL8_FFFFFF;
      }
      // 光标OFF
      if (data == 3) {
        box_fill8(sheet->buf, sheet->bxsize, COL8_000000, cons.cur_x,
                  cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
        cons.cur_c = -1;
      }

      if (data >= 256 && data <= 511) {
        if (data == 8 + 256) {
          // 退格键
          if (cons.cur_x > 16) {
            // 用空白擦除光标后将光标前移一位
            cons_putchar(&cons, ' ', 0);
            cons.cur_x -= 8;
          }
        } else if (data == 10 + 256) {
          // 回车键
          // 用空格将光标擦除
          cons_putchar(&cons, ' ', 0);
          cmdline[cons.cur_x / 8 - 2] = '\0';
          cons_newline(&cons);
          cons_run_cmd(cmdline, &cons, fat, memtotal);

          cons_putchar(&cons, '>', 1);
        } else {
          if (cons.cur_x < 240) {
            cmdline[cons.cur_x / 8 - 2] = data - 256;
            cons_putchar(&cons, data - 256, 1);
          }
        }
      }

      if (cons.cur_c >= 0) {
        box_fill8(sheet->buf, sheet->bxsize, cons.cur_c, cons.cur_x, cons.cur_y,
                  cons.cur_x + 7, cons.cur_y + 15);
      }
      sheet_refresh(sheet, cons.cur_x, cons.cur_y, cons.cur_x + 8,
                    cons.cur_y + 16);
    }
  }
}

void cons_putchar(struct Console *cons, int ch, char move) {
  char s[2];

  s[0] = ch;
  s[1] = '\0';

  if (s[0] == '\t') {
    for (;;) {
      put_fonts8_asc_sht(cons->sheet, cons->cur_x, cons->cur_y, COL8_FFFFFF,
                         COL8_000000, " ", 1);
      cons->cur_x += 8;
      if (cons->cur_x == 8 + 240) {
        cons_newline(cons);
      }

      if (!((cons->cur_x - 8) & 0x1f)) {
        break; // 被32整除则break
      }
    }
  } else if (s[0] == '\n') {
    // 换行符
    cons_newline(cons);
  } else if (s[0] == '\r') {
    // 回车符，暂不处理
  } else {
    put_fonts8_asc_sht(cons->sheet, cons->cur_x, cons->cur_y, COL8_FFFFFF,
                       COL8_000000, s, 1);
    if (move) {
      cons->cur_x += 8;
      if (cons->cur_x == 8 + 240) {
        cons_newline(cons);
      }
    }
  }
}

void cons_newline(struct Console *cons) {
  struct Sheet *sheet = cons->sheet;

  if (cons->cur_y < 28 + 112) {
    cons->cur_y += 16;
  } else {
    // 滚动
    for (int y = 28; y < 28 + 112; y++) {
      for (int x = 8; x < 8 + 240; x++) {
        sheet->buf[x + y * sheet->bxsize] =
            sheet->buf[x + (y + 16) * sheet->bxsize];
      }
    }
    for (int y = 28 + 112; y < 28 + 128; y++) {
      for (int x = 8; x < 8 + 240; x++) {
        sheet->buf[x + y * sheet->bxsize] = COL8_000000;
      }
    }

    sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
  }

  cons->cur_x = 8;
}

void cons_putstr(struct Console *cons, char *s) {
  while (*s) {
    cons_putchar(cons, *s, 1);
    s++;
  }
}

void cons_putnstr(struct Console *cons, char *s, int n) {
  for (int i = 0; i < n; i++) {
    cons_putchar(cons, s[i], 1);
  }
}

void cons_run_cmd(char *cmdline, struct Console *cons, int *fat,
                  unsigned int memtotal) {
  if (!strcmp(cmdline, "mem")) {
    cmd_mem(cons, memtotal);
  } else if (!strcmp(cmdline, "clean")) {
    cmd_cls(cons);
  } else if (!strcmp(cmdline, "ls")) {
    cmd_dir(cons);
  } else if (!strncmp(cmdline, "cat ", 4)) {
    cmd_type(cons, fat, cmdline);
  } else if (strcmp(cmdline, "")) {
    if (!cmd_app(cons, fat, cmdline)) {
      cons_putstr(cons, "Bad command.\n\n");
    }
  }
}
