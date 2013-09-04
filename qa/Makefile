LIBS=$(shell find . -mindepth 1 -maxdepth 1 -type d)
all: $(LIBS)
.PHONY: force

# Add the ability to run one test with "make 3b"

$(LIBS): force
	for dir in $(LIBS); do $(MAKE) -C $$dir; done

clean:
	for dir in $(LIBS); do $(MAKE) -C $$dir clean; done
