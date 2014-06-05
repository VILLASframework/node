
.PHONY: all html latex clean

all: latex html pdf

latex:
html:
	doxygen

pdf: latex
	$(MAKE) -C latex pdf

clean:
	$(RM) -r latex
	$(RM) -r html
