CC = gcc
CFLAGS = -I -g
OBJECT1 = oss.o
TARGET1 = oss
CONFIG = config.h
PROCESS1 = oss.c

all: $(PROCESS1) $(CONFIG)
	$(CC) $(CFLAGS) $(PROCESS1) -o $(TARGET1) 

%.o: %.c $(CONFIG)
	$(CC) $(CFLAGS) -c $@ $<

clean: 
	@rm -f *.o oss child logfile.*
	rm *.txt
