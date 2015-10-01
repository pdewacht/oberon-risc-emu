CFLAGS = -Wall -Wextra -O3 -march=native

TESTS = add flr flt mul div idiv

VPATH = $(VERILOG)

compile: $(TESTS)
test: $(addprefix test-, $(TESTS))

test-%: %
	./$<

%: %.c
	gcc -std=c99 -o $@ $(filter %.c, $^) $(CFLAGS)

RISC_FP = ../risc-fp.c ../risc-fp.h
add: add.c $(RISC_FP) numbers.inc FPAdder.inc
flr: flr.c $(RISC_FP) numbers.inc FPAdder.inc
flt: flt.c $(RISC_FP) numbers.inc FPAdder.inc
mul: mul.c $(RISC_FP) numbers.inc FPMultiplier.inc
div: div.c $(RISC_FP) numbers.inc FPDivider.inc
idiv: idiv.c $(RISC_FP) numbers.inc Divider.inc

numbers.inc: numbers.py
	python3 $< > $@

%.inc: %.v v2c.py
	python3 v2c.py < $< > $@

%.inc:
# If the previous rule failed...
	$(error Set the VERILOG environment variable to the directory with the Verilog sources)

clean:
	rm -f $(TESTS) *.inc
