all:
	nasm hello.asm -o hello.bin -l hello.lst
	dd if=/dev/zero of=hello.img bs=512 count=2880
	dd if=hello.bin of=hello.img bs=512 count=1 conv=notrunc

clean:
	rm -rf hello.bin hello.img

qemu: all
	qemu-system-i386 -fda hello.img -gdb tcp::1234 -S

.PHONY:
	all