void io_hlt(void);

void io_cli(void);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);

void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);

void write_mem8(int addr, int data);
void box_fill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1);

#define COL8_000000 0
#define COL8_FF0000 1
#define COL8_00FF00 2
#define COL8_FFFF00 3
#define COL8_0000FF 4
#define COL8_FF00FF 5
#define COL8_00FFFF 6
#define COL8_FFFFFF 7
#define COL8_C6C6C6 8
#define COL8_840000 9
#define COL8_008400 10
#define COL8_848400 11
#define COL8_000084 12
#define COL8_840084 13
#define COL8_008484 14
#define COL8_848484 15

int main(void)
{
    char *vram;
    int xsize, ysize;

    init_palette();
    vram = (char *)0xa0000;
    xsize = 320;
    ysize = 200;

    box_fill8(vram, xsize, COL8_008484, 0, 0, xsize - 1, ysize - 29);
    box_fill8(vram, xsize, COL8_C6C6C6, 0, ysize - 28, xsize - 1, ysize - 28);
    box_fill8(vram, xsize, COL8_FFFFFF, 0, ysize - 27, xsize - 1, ysize - 27);
    box_fill8(vram, xsize, COL8_C6C6C6, 0, ysize - 26, xsize - 1, ysize - 1);

    box_fill8(vram, xsize, COL8_FFFFFF, 3, ysize - 24, 59, ysize - 24);
    box_fill8(vram, xsize, COL8_FFFFFF, 2, ysize - 24, 2, ysize - 4);
    box_fill8(vram, xsize, COL8_848484, 3, ysize - 4, 59, ysize - 4);
    box_fill8(vram, xsize, COL8_848484, 59, ysize - 23, 59, ysize - 5);
    box_fill8(vram, xsize, COL8_000000, 2, ysize - 4, 59, ysize - 3);
    box_fill8(vram, xsize, COL8_000000, 60, ysize - 24, 60, ysize - 3);

    box_fill8(vram, xsize, COL8_848484, xsize - 47, ysize - 24, xsize - 4,
              ysize - 24);
    box_fill8(vram, xsize, COL8_848484, xsize - 47, ysize - 23, xsize - 47,
              ysize - 4);
    box_fill8(vram, xsize, COL8_FFFFFF, xsize - 47, ysize - 3, xsize - 4,
              ysize - 3);
    box_fill8(vram, xsize, COL8_FFFFFF, xsize - 3, ysize - 24, xsize - 3,
              ysize - 3);

    for (;;)
    {
        io_hlt();
    }
}

// int main(void)
// {
//     int i;

//     char *p;

//     init_palette(); // set palette

//     p = (char *)0xa0000;

//     box_fill8(p, 320, COL8_FF0000, 20, 20, 120, 120);
//     box_fill8(p, 320, COL8_00FF00, 70, 50, 170, 150);
//     box_fill8(p, 320, COL8_0000FF, 120, 80, 220, 180);

//     // for (i = 0; i <= 0xffff; i++)
//     // {
//     //     p[i] = i & 0x0f;
//     // }

//     for (;;)
//     {
//         io_hlt();
//     }
// }

void init_palette(void)
{
    static unsigned char table_rgb[16 * 3] = {
        0x00, 0x00, 0x00, // 黑色
        0xff, 0x00, 0x00, // 亮红色
        0x00, 0xff, 0x00, // 亮绿色
        0xff, 0xff, 0x00, // 亮黄色
        0x00, 0x00, 0xff, // 亮蓝色
        0xff, 0x00, 0xff, // 亮紫色
        0x00, 0xff, 0xff, // 浅亮蓝色
        0xff, 0xff, 0xff, // 白色
        0xc6, 0xc6, 0xc6, // 亮灰色
        0x84, 0x00, 0x00, // 暗红色
        0x00, 0x84, 0x00, // 暗绿色
        0x84, 0x84, 0x00, // 暗黄色
        0x00, 0x00, 0x84, // 暗蓝色
        0x84, 0x00, 0x84, // 暗紫色
        0x00, 0x84, 0x84, // 浅暗蓝色
        0x84, 0x84, 0x84  // 暗灰色
    };

    set_palette(0, 15, table_rgb);
    return;
}

void set_palette(int start, int end, unsigned char *rgb)
{
    int i, eflags;
    eflags = io_load_eflags();
    io_cli();
    io_out8(0x03c8, start);
    for (i = start; i <= end; i++)
    {
        io_out8(0x03c9, rgb[0] / 4);
        io_out8(0x03c9, rgb[1] / 4);
        io_out8(0x03c9, rgb[2] / 4);
        rgb += 3;
    }
    io_store_eflags(eflags);
    return;
}

void box_fill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0,
               int x1, int y1)
{
    for (int y = y0; y <= y1; y++)
    {
        for (int x = x0; x <= x1; x++)
        {
            vram[y * xsize + x] = c;
        }
    }

    return;
}