#ifndef _BOOTPACK_H_
#define _BOOTPACK_H_

#define ADR_BOOTINFO 0x00000ff0
#define ADR_DISKIMG 0x00100000

struct BootInfo
{
    char cyls, leds, vmode, reserve;
    short scrnx, scrny;
    unsigned char *vram;
};


#endif // _BOOTPACK_H_
