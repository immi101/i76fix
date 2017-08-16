CC=i686-w64-mingw32-gcc
CFLAGS=-std=c99 -Wall -g

PATCHER=patch_i76.exe \
	patch_nitro.exe

PATCHED_EXE=i76_25fps.exe \
	    nitro_25fps.exe

all: delay.S $(PATCHER)


.PHONY: patch clean
patch: $(PATCHED_EXE)


clean:
	rm -f *.o $(PATCHER) $(PATCHED_EXE) delay.S

delay.S: delay.c
	$(CC) -O2 -S -o $@ $<

patch_i76.exe: patcher.o patch_i76.o
	$(CC) -o $@ $+

patch_nitro.exe: patcher.o patch_nitro.o
	$(CC) -o $@ $+

patcher.o: patcher.c
	$(CC) $(CFLAGS)  -c -O2 -o $@ -std=c99 -s -g  $+

patch_i76.o: patch.s
	$(CC) -c -o $@ -Wa,--defsym,TARGET=1  $<

patch_nitro.o: patch.s
	$(CC) -c -o $@  -Wa,--defsym,TARGET=2 $<


# run patch
i76_25fps.exe: i76.exe patch_i76.exe
	wine patch_i76.exe i76.exe $@

nitro_25fps.exe: nitro.exe patch_nitro.exe
	wine patch_nitro.exe nitro.exe $@

