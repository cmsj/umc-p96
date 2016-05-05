CC = m68k-amigaos-gcc -noixemul -s
CFLAGS = -Os -Wall -fomit-frame-pointer -msmall-code -m68000
LDFLAGS = -lm

umc: umc.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

lha: ../umc.lha umc
../umc.lha:
	./make-lha.sh

clean:
	rm -f umc

dist-clean:
	rm -f ../umc.lha
