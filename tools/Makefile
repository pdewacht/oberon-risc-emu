RUSTFLAGS = -O

all: asciidecoder ob2unix

clean:
	rm -f asciidecoder ob2unix

%: %.rs
	rustc $(RUSTFLAGS) $<
