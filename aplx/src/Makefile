#MKFILES = Makefile.tgsdp Makefile.monitor Makefile.srcsink
MKFILES = Makefile.tgsdp Makefile.monitor

RM := rm -rf
LS := ls -l
MK := make -f

all: $(APPS)
	for mf in $(MKFILES); do $(MK) $$mf; done

clean: $(APPS)
	for mf in $(MKFILES); do $(MK) $$mf clean; done

