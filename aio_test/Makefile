ifndef $(CC)
CC = aarch64-linux-gcc
endif

all: aio_read-test aio_write-test

aio_read-test: aio_read_test.o
	$(CC) -lrt -o $@ $<

aio_write-test: aio_write_test.o
	$(CC) -lrt -o $@ $<

clean:
	rm -rf *.o aio_read-test aio_write-test
