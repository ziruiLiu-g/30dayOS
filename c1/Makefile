all:
	nasm hello.asm -o hello.bin
clean:
	rm -rf hello.bin
qemu: all
	qemu-system-i386 -fda hello.bin

.PHONY:
	all