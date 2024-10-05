#include <stdio.h>
#include <string.h>

#include "bootpack.h"
#include "command.h"
#include "console.h"
#include "desctbl.h"
#include "elf.h"
#include "fs.h"
#include "graphic.h"
#include "memory.h"
#include "sheet.h"
#include "task.h"
#include "app.h"
#include "io.h"

void cmd_mem(struct Console *cons, unsigned int memtotal) {
  struct MemMan *memman = (struct MemMan *)MEMMAN_ADDR;
  char s[30];

  sprintf(s, "total   %dMB\nfree %dKB\n\n", memtotal / (1024 * 1024), memman_total(memman) / 1024);
  cons_putstr(cons, s);
}

void cmd_cls(struct Console *cons) {
  struct Sheet *sheet = cons->sheet;

  for (int y = 28; y < 28 + 128; y++) {
    for (int x = 8; x < 8 + 240; x++) {
      sheet->buf[x + y * sheet->bxsize] = COL8_000000;
    }
  }

  sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
  cons->cur_y = 28;
}

void cmd_dir(struct Console *cons) {
  struct FileInfo *finfo = (struct FileInfo *)(ADR_DISKIMG + 0x002600);
  char s[30];

  for (int i = 0; i < 224; i++) {
    if (finfo[i].name[0] == '\0') {
      break;
    }

    if (finfo[i].name[0] != 0xe5) {
      if (!(finfo[i].type & 0x18)) {
        sprintf(s, "filename.ext   %7d\n", finfo[i].size);

        for (int j = 0; j < 8; j++) {
          s[j] = finfo[i].name[j];
        }
        s[9] = finfo[i].ext[0];
        s[10] = finfo[i].ext[1];
        s[11] = finfo[i].ext[2];

        cons_putstr(cons, s);
      }
    }
  }

  cons_newline(cons);
}

void cmd_type(struct Console *cons, int *fat, char *cmdline) {
  struct MemMan *memman = (struct MemMan *)MEMMAN_ADDR;
  struct FileInfo *finfo = file_search(
      cmdline + 4, (struct FileInfo *)(ADR_DISKIMG + 0x002600), 224);
  char *p;

  if (finfo) {
    p = (char *)memman_alloc_4k(memman, finfo->size);
    file_load_file(finfo->clustno, finfo->size, p, fat,
                   (char *)(ADR_DISKIMG + 0x003e00));
    cons_putnstr(cons, p, finfo->size);
    memman_free_4k(memman, (int)p, finfo->size);
  } else {
    cons_putstr(cons, "File not found.\n");
  }

  cons_newline(cons);
}

void cmd_exit(struct Console *cons, int *fat) {
  struct MemMan *memman = (struct MemMan *) MEMMAN_ADDR;
  struct Task *task = task_now();
  struct Shtctl *shtctl = (struct Shtctl *) *((int *) 0x0fe4);
  struct FIFO32 *fifo = (struct FIFO32 *) *((int *) 0x0fec);
  timer_cancel(cons->timer);
  memman_free_4k(memman, (int) fat, 4 * 2880);
  io_cli();

  if (cons->sheet != 0) {
    fifo32_put(fifo, cons->sheet - shtctl->sheets0 + 768); // 768 ~1023
  } else {
    fifo32_put(fifo, cons->sheet - shtctl->sheets0 + 1024); // 1024~2023
  }

  io_sti();
  for (;;) {
    task_sleep(task);
  }
}

void cmd_start(struct Console *cons, char* cmdline, int memtotal) {
  struct Shtctl *shtctl = (struct Shtctl *) *((int *) 0x0fe4);
  struct Sheet *sht = open_console(shtctl, memtotal);
  struct FIFO32 *fifo = &sht->task->fifo;

  int i;
  sheet_slide(sht, 32, 4);
  sheet_updown(sht, shtctl->top);

  for (i = 6; cmdline[i] != 0; i++) {
    fifo32_put(fifo, cmdline[i] + 256);
  }
  fifo32_put(fifo, 10 + 256); // enter
  cons_newline(cons);
  return;
}

void cmd_ncst(struct Console *cons, char* cmdline, int memtotal) {
  struct Task *task = open_cons_task(0, memtotal);
  struct FIFO32 *fifo = &task->fifo;

  int i;

  for (i = 5; cmdline[i] != 0; i++) {
    fifo32_put(fifo, cmdline[i] + 256);
  }
  fifo32_put(fifo, 10 + 256); // enter
  cons_newline(cons);
  return;
}

int cmd_app(struct Console *cons, int *fat, char *cmdline) {
  struct MemMan *memman = (struct MemMan *)MEMMAN_ADDR;
  struct FileInfo *finfo;
  struct SegmentDescriptor *gdt = (struct SegmentDescriptor *)ADR_GDT;
  char name[18], *p, *q;
  struct Task *task = task_now();
  int i;

  struct Shtctl *shtctl;
  struct Sheet *sht;

  for (i = 0; i < 13; i++) {
    if (cmdline[i] <= ' ') {
      break;
    }

    name[i] = cmdline[i];
  }
  name[i] = '\0';

  finfo = file_search(name, (struct FileInfo *)(ADR_DISKIMG + 0x002600), 224);
  if (finfo == NULL && name[i - 1] != '.') {
    name[i] = '.';
    name[i + 1] = 'H';
    name[i + 2] = 'R';
    name[i + 3] = 'B';
    name[i + 4] = '\0';

    finfo = file_search(name, (struct FileInfo *)(ADR_DISKIMG + 0x002600), 224);
  }

  if (finfo) {
    p = (char *)memman_alloc_4k(memman, finfo->size);
    *((int *) 0xfe8) = (int) p;
    
    file_load_file(finfo->clustno, finfo->size, p, fat,
                   (char *)(ADR_DISKIMG + 0x003e00));
    Elf32_Ehdr *elfhdr = (Elf32_Ehdr *)p;

    if (elf32_validate(elfhdr)) {
      q = (char *)memman_alloc_4k(memman, 64 * 1024);
      task->ds_base = (int) q;

      set_segmdesc(task->ldt + 0, finfo->size - 1, (int)p, AR_CODE32_ER + 0x60);
      set_segmdesc(task->ldt + 1, 64 * 1024 - 1, (int)q, AR_DATA32_RW + 0x60);

      for (int i = 0; i < elfhdr->e_shnum; i++) {
        Elf32_Shdr *shdr =
            (Elf32_Shdr *)(p + elfhdr->e_shoff + sizeof(Elf32_Shdr) * i);

        if (shdr->sh_type != SHT_PROGBITS || !(shdr->sh_flags & SHF_ALLOC)) {
          continue;
        }

        for (int i = 0; i < shdr->sh_size; i++) {
          q[shdr->sh_addr + i] = p[shdr->sh_offset + i];
        }
      }
      start_app(elfhdr->e_entry, 0 * 8 + 4, 0, 1 * 8 + 4, &(task->tss.esp0));
      /* To close win */
      shtctl = (struct Shtctl *) *((int *) 0x0fe4);
      for (i = 0; i < MAX_SHEETS; i++) {
        sht = &(shtctl->sheets0[i]);
        if ((sht->flags & 0x11) == 0x11 && sht->task == task) {
          sheet_free(sht);
        }
      }
      timer_cancel_all(&task->fifo);
      memman_free_4k(memman, (int)q, 64 * 1024);
    } else {
      cons_putstr(cons, "ELF file format error.\n");
    }
    
    memman_free_4k(memman, (int)p, finfo->size);
    
    cons_newline(cons);

    return 1;
  }

  return 0;
}