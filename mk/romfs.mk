ROMDIR = $(DATDIR)/test-romfs

$(OUTDIR)/$(ROMDIR).o: $(OUTDIR)/$(ROMDIR).bin
	@mkdir -p $(dir $@)
	@echo "    OBJCOPY "$@
	@$(CROSS_COMPILE)objcopy -I binary -O elf32-littlearm -B arm \
		--prefix-sections '.romfs' $< $@

$(OUTDIR)/$(ROMDIR).bin: $(ROMDIR)
	@mkdir -p $(dir $@)
	@echo "    MKROMFS "$@
	@genromfs -v -d $< -f $@

$(ROMDIR):
	@mkdir -p $@
