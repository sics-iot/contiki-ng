CONTIKI_PROJECT = edhoc-demo.csc
all: $(CONTIKI_PROJECT)

EDHOC_TARGETS := edhoc-client edhoc-server

CONTIKI=../..
include $(CONTIKI)/Makefile.include

clean:	clean_edhoc

distclean: distclean_edhoc

clean_edhoc:
	$(Q)for TARG in $(EDHOC_TARGETS); do \
		(cd $$TARG && $(MAKE) TARGET=$(TARGET) clean); \
	done

distclean_edhoc:
	$(Q)for TARG in $(EDHOC_TARGETS); do \
		(cd $$TARG && $(MAKE) TARGET=$(TARGET) distclean); \
	done
