
CFLAGS = -O3 -I. -Wall


all: nethash netview extract

clean :
	rm nethash netview extract

nethash: nethash.c
	gcc -o $@ $<  $(CFLAGS)

netview: netview.c
	gcc -o $@ $<  $(CFLAGS)

extract: extract.c
	gcc -o $@ $< $(CFLAGS)
