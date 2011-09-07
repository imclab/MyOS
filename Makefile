CC = gcc
LD = ld
LIBS = uclibc/lib

CFLAGS = -I uclibc/include -fno-builtin 
LDFLAGS = -t -L uclibc/lib -static

KCFLAGS = 
KLDFLAGS = -T src/kernel/linker.ld 


all: main

main: kernel

init:
	mkdir -p build/kernel 2> /dev/null || true
	mkdir bin 2> /dev/null || true
    
clean:
	rm -r build/*
	rm -r bin/*
        
kernel: init
	echo
	nasm -felf -o   build/kernel/bootstrap.o                        src/kernel/bootstrap.asm
	$(CC) -c -o     build/kernel/entry.o    $(CFLAGS) $(KCFLAGS)    src/kernel/entry.c

	echo
	nasm -felf -o   build/kernel/interrupts.o                       src/kernel/interrupts.asm
	$(CC) -c -o     build/kernel/idt.o      $(CFLAGS) $(KCFLAGS)    src/kernel/idt.c
	$(CC) -c -o     build/kernel/gdt.o      $(CFLAGS) $(KCFLAGS)    src/kernel/gdt.c
	$(CC) -c -o     build/kernel/isr.o      $(CFLAGS) $(KCFLAGS)    src/kernel/isr.c
	nasm -felf -o   build/kernel/util.o                             src/kernel/util.asm

	$(CC) -c -o     build/kernel/timer.o    $(CFLAGS) $(KCFLAGS)    src/kernel/timer.c

	echo
	$(CC) -c -o     build/kernel/kutils.o   $(CFLAGS) $(KCFLAGS)    src/kernel/kutils.cpp 
	$(CC) -c -o     build/kernel/terminal.o $(CFLAGS) $(KCFLAGS)    src/kernel/terminal.cpp 
	
	echo
	$(LD) $(KLDFLAGS) $(LDFLAGS) build/kernel/*.o -o bin/kernel
	
mount: umount
	vmware-mount ~/vmware/MyOS/MyOS-0.vmdk fs
	
umount:		
	vmware-mount -d fs || true
	sleep 2
	vmware-mount -d fs || true
		
deploy: main
	echo
	vmware-mount ~/vmware/MyOS/MyOS-0.vmdk fs
	sudo cp bin/kernel fs/kernel
	vmware-mount -d fs || true
	sleep 2
	vmware-mount -d fs || true
	
.PHONY: init mount umount main kernel deploy 
