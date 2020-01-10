BINARY=ethertest
#ETHERCAT_ROOT=/projects/ethercat/etherlab-master
ETHERCAT_ROOT=/home/laxetti/ethercat-1.5.2/
CC=gcc
SRC=main.c

INC_PATH=$(ETHERCAT_ROOT)/include
LIB_PATH=$(ETHERCAT_ROOT)/lib/.libs

LD_FLAGS=-lethercat -lrt
C_FLAGS=-Wall -std=c99

OBJECTS=$(SRC:.c=.o)

.PHONY: all clean

all: $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) -L$(LIB_PATH) $^ -o $@ $(LD_FLAGS)  

%.o : %.c
	$(CC) -I$(INC_PATH) $(C_FLAGS) -c -o $@ $<

clean:
	rm -f *.o $(BINARY)
