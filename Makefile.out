CFLAGS = -g -Wall -Werror
LINUX_CLI=linux_cli
CC=gcc
OBJECT=nvme-ioctl.o suffix.o nvme-dll.o\
	   nvme-print.o nvme-cli.o

$(LINUX_CLI): $(OBJECT)                  
	$(CC) $(CFLAGS) -o $(LINUX_CLI) $^ 
	ctags -R

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $^

.PHONY: clean
clean:
	-rm -rf *.o $(LINUX_CLI) tags ./log/*
