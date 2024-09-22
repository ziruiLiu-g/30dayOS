#include <stdio.h>
#include <string.h>

#include "console.h"
#include "graphic.h"
#include "bootpack.h"
#include "sheet.h"
#include "sheet.h"
#include "task.h"
#include "fifo.h"
#include "fs.h"
#include "io.h"
#include "desctbl.h"

int cons_newline(int cursor_y, struct Sheet *sheet) {
    int x, y;
    if (cursor_y < 28 + 112) {
        cursor_y += 16;
    } else {
        for (y = 28; y < 28 + 112; y++) {
            for (x = 8; x < 8 + 240; x++) {
                sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
            }
        }

        for (y = 28 + 112; y < 28 + 128; y++) {
            for (x = 8; x < 8 + 240; x++) {
                sheet->buf[x + y * sheet->bxsize] = COL8_000000;
            }
        }

        sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
    }

    return cursor_y;
}

void console_task(struct Sheet *sheet, unsigned int memtotal) {
    struct Timer *timer;
    struct Task *task = task_now();
    char s[30], cmdline[30], *p;
    struct MemMan *memman = (struct MemMan *) MEMMAN_ADDR;
    struct FileInfo *finfo = (struct FileInfo *) (ADR_DISKIMG + 0x002600); // filename storage
    struct SegmentDescriptor *gdt = (struct SegmentDescriptor *) ADR_GDT;
    int *fat = (int *) memman_alloc_4k(memman, 4 * 2880);

    int i, fifobuf[128], cursor_x = 16, cursor_y = 28, cursor_c = -1;
    fifo32_init(&task->fifo, 128, fifobuf, task);

    int x, y;

    timer = timer_alloc();
    timer_init(timer, &task->fifo, 1);
    timer_set_timer(timer, 50);

    file_read_fat(fat, (unsigned char *) (ADR_DISKIMG + 0X000200));

    put_fonts8_asc_sht(sheet, 8, 28, COL8_FFFFFF, COL8_000000, ">", 1);

    for (;;) {
        io_cli();

        if (fifo32_status(&task->fifo) == 0) {
            task_sleep(task);
            io_sti();
        } else {
            i = fifo32_get(&task->fifo);
            io_sti();
            
            if (i <= 1) {
                if (i != 0) {
                    timer_init(timer, &task->fifo, 0);
                    if (cursor_c >= 0) {
                        cursor_c = COL8_FFFFFF;
                    }
                } else {
                    timer_init(timer, &task->fifo, 1);
                    if (cursor_c >= 0) {
                        cursor_c = COL8_000000;
                    }
                }
                timer_set_timer(timer, 50);
            }
            if (i == 2) {
                cursor_c = COL8_FFFFFF;
            }
            if (i == 3) {
                box_fill8(sheet->buf, sheet->bxsize, COL8_000000, cursor_x, cursor_y, cursor_x + 7, 43);
                cursor_c = -1;
            }

            if (i >= 256 && i <= 511) {
                if (i == 8 + 256) {
                    // backspace
                    if (cursor_x > 16) {
                        put_fonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
                        cursor_x -= 8;
                    }
                } else if (i == 256 + 10) { // enter
                    put_fonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
                    cmdline[cursor_x / 8 - 2] = 0;
                    cursor_y = cons_newline(cursor_y, sheet);
                    
                    if (strcmp(cmdline, "mem") == 0) {
                        /* mem */
                        sprintf(s, "total   %dMB", memtotal / (1024 * 1024));
                        put_fonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
                        cursor_y = cons_newline(cursor_y, sheet);
                        sprintf(s, "free   %dKB", memman_total(memman) / 1024);
                        put_fonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
                        cursor_y = cons_newline(cursor_y, sheet);
                        cursor_y = cons_newline(cursor_y, sheet);
                    } else if (strcmp(cmdline, "clear") == 0) {
                        for (y = 28; y < 28 + 128; y++) {
                            for (x = 8; x < 8 + 240; x++) {
                                sheet->buf[x + y * sheet->bxsize] = COL8_000000;
                            }
                        }

                        sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
                        cursor_y = 28;
                    } else if (strcmp(cmdline, "ls") == 0) {
                        for (x = 0; x < 224; x++) {
                            if (finfo[x].name[0] == 0x00) {
                                break;
                            }
                            if (finfo[x].name[0] != 0xe5) {
                                if ((finfo[x].type & 0x18) == 0) {
                                    sprintf(s, "filename.ext    %7d", finfo[x].size);
                                    for (y = 0; y < 8; y++) {
                                        s[y] = finfo[x].name[y];
                                    }

                                    s[9] = finfo[x].ext[0];
                                    s[10] = finfo[x].ext[1];
                                    s[11] = finfo[x].ext[2];

                                    put_fonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
                                    cursor_y = cons_newline(cursor_y, sheet);
                                }
                            }
                        }
                        cursor_y = cons_newline(cursor_y, sheet);
                    } else if (!strncmp(cmdline, "cat ", 4)) {
                        for (y = 0; y < 11; y++) {
                            s[y] = ' ';
                        }
                        y = 0;

                        for (x = 4; y < 11 && cmdline[x] != '\0'; x++) {
                            if (cmdline[x] == '.' && y <= 8) {
                                y = 8;
                            } else {
                                s[y] = cmdline[x];
                                if ('a' <= s[y] && s[y] <= 'z') {
                                    s[y] -= 0x20;
                                }
                                y++;
                            }
                        }

                        for (x = 0; x < 224; ) {
                            if (finfo[x].name[0] == 0x00) break;
                            if ((finfo[x].type & 0x18) == 0) {
                                for (y = 0; y < 11; y++) {
                                    if (finfo[x].name[y] != s[y]) {
                                        goto type_next_file;
                                    }
                                }
                                break;
                            }
        type_next_file:
                            x++;
                        }

                        if (x < 224 && finfo[x].name[0] != 0x00) {
                            p = (char *) memman_alloc_4k(memman, finfo[x].size);
                            file_load_file(finfo[x].clustno, finfo[x].size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
                            cursor_x = 8;
                            for (y = 0; y < finfo[x].size; y++) {
                                s[0] = p[y];
                                s[1] = 0;

                                if (s[0] == '\t')  { // make table sign '0x09'
                                    for (;;) {
                                        put_fonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
                                        cursor_x += 8;
                                        if (cursor_x == 8 + 240) {
                                            cursor_x = 8;
                                            cursor_y = cons_newline(cursor_y, sheet);
                                        }
                                        if (((cursor_x - 8) & 0x1f) == 0) { // could be devided by 32
                                            break;
                                        }
                                    }
                                } else if (s[0] == '\n') { // new line '0x0a'
                                    cursor_x = 8;
                                    cursor_y = cons_newline(cursor_y, sheet);
                                } else if (s[0] == '\r') { // enter "carriage return" '0x0d'

                                } else {
                                    put_fonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
                                    cursor_x += 8;
                                    if (cursor_x == 8 + 240) {
                                        cursor_x = 8;
                                        cursor_y = cons_newline(cursor_y, sheet);
                                    }
                                }
                            }
                            memman_free_4k(memman, (int) p, finfo[x].size);
                        } else {
                            /* can not find */
                            put_fonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File Not Found.", 15);
                            cursor_y = cons_newline(cursor_y, sheet);
                        }
                        cursor_y = cons_newline(cursor_y, sheet);
                    } else if (!strcmp(cmdline, "hlt")) {
                        for (y = 0; y < 11; y++) {
                            s[y] = ' ';
                        }

                        s[0] = 'H';
                        s[1] = 'L';
                        s[2] = 'T';
                        s[8] = 'H';
                        s[9] = 'R';
                        s[10] = 'B';
                        
                        for (x = 0; x < 224; ) {
                            if (finfo[x].name[0] == '\0') {
                                break;
                            }
                            if (!(finfo[x].type & 0x18)) {
                                for (y = 0; y < 11; y++) {
                                    if (finfo[x].name[y] != s[y]) {
                                        goto hlt_next_file;
                                    }
                                }
                                break;
                            }
                            hlt_next_file:
                            x++;
                        }

                        if (x < 224 && finfo[x].name[0] != '\0') {
                            char *p = (char *) memman_alloc_4k(memman, finfo[x].size);
                            file_load_file(finfo[x].clustno, finfo[x].size, p, fat, (char *)(ADR_DISKIMG + 0x003e00));
                            set_segmdesc(gdt + 1003, finfo[x].size - 1, (int) p, AR_CODE32_ER);
                            far_jmp(0, 1003 * 8);
                            memman_free_4k(memman, (int) p, finfo[x].size);
                        } else {
                            put_fonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
                            cursor_y = cons_newline(cursor_y, sheet);
                        }
                        cursor_y = cons_newline(cursor_y, sheet);
                    } else if (cmdline[0] != 0) {
                        put_fonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "Bad command.", 12);
                        cursor_y = cons_newline(cursor_y, sheet);
                        cursor_y = cons_newline(cursor_y, sheet);
                    }

                    put_fonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, ">", 1);
                    cursor_x = 16;
                } else {
                    if (cursor_x < 240) {
                        s[0] = i - 256;
                        s[i] = 0;
                        cmdline[cursor_x / 8 - 2] = i - 256;
                        put_fonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
                        cursor_x += 8;
                    } 
                    // else {
                    //     cursor_y += 16;
                    //     put_fonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, ">", 1);
                    //     cursor_x = 16;
                    // }
                }
            }

            if (cursor_c >= 0) {
                box_fill8(sheet->buf, sheet->bxsize, cursor_c, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
            }
            sheet_refresh(sheet, cursor_x, cursor_y, cursor_x + 8, cursor_y + 16);
        }
    }
}