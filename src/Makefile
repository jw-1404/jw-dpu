ifndef $(CC)
CC = aarch64-linux-gcc
endif

ifndef $(DESTDIR)
DESTDIR = /
endif

all: jw_reg_rw jw_to_device jw_from_device jw_test_chrdev

jw_to_device: dma_to_device.o
	$(CC) -lrt -o $@ $< -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGE_FILE_SOURCE

jw_from_device: dma_from_device.o
	$(CC) -lrt -o $@ $< -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGE_FILE_SOURCE

jw_reg_rw: reg_rw.o
	$(CC) -o $@ $<

jw_test_chrdev: test_chrdev.o
	$(CC) -o $@ $<

%.o: %.c
	$(CC) -c -std=c99 -o $@ $< -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGE_FILE_SOURCE

install:
	install -t $(DESTDIR)/usr/bin jw_reg_rw jw_to_device jw_from_device jw_test_chrdev
	install -d $(DESTDIR)/usr/share/xdma
	cp -r ../tests/ $(DESTDIR)/usr/share/xdma/

clean:
	rm -rf *.o *.bin jw_reg_rw jw_to_device jw_from_device jw_test_chrdev
