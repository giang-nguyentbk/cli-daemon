# --------------------------------------------------
# Call Makefiles for init setups and build database
# --------------------------------------------------
include Makefile.config

# --------------------------------------------------
# Call Makefiles for modules of the project
# --------------------------------------------------
include $(SW_DIR)/shell/Makefile
include $(SW_DIR)/clid/Makefile
include $(SW_DIR)/cmdif/Makefile




all: rootMakefile

rootMakefile:
	@echo "sw/make/Makefile: Compilation $(PROJ_NAME) done!!! [OK]"

clean:
	@echo
	@echo "  RMV \t\t $(BIN_DIR)"
	@$(SELF_RMV) $(BIN_DIR)