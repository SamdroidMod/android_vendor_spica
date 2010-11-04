###############################################
#
# Makefile
#
#

CC := gcc

PROGRAM = dtun
SOURCES = dtun_clnt.c
CFLAGS += -I . -Wall -g -I  ../include -DDTUN_STANDALONE

#CFLAGS  += `pkg-config glib --cflags` 

LDFLAGS +=  -lpthread #`pkg-config glib --libs`

all: $(PROGRAM)

$(PROGRAM): $(patsubst %.c, %.o, $(SOURCES))
	$(CC) -o $@ $? $(LDFLAGS)
 
%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -f $(PROGRAM) *.o *~ *.bak

