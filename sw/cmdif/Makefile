CMDIF_DIR	:= $(SW_DIR)/cmdif
CMDIF_SRC_DIR	:= $(CMDIF_DIR)/src

CMDIF_CXXFLAGS	:= -L$(SDK_LIB_DIR) -litc -ltraceif
CMDIF_LIBAR	:= libcmdifa.a # Static libary
CMDIF_LIBSO	:= libcmdif.so # Dynamic libary

CMDIF_SRCS	=
CMDIF_SRCS	+= cmdJobImpl.cc
CMDIF_SRCS	+= cmdRegisterImpl.cc
CMDIF_SRCS	+= cmdSyntaxGraph.cc
CMDIF_SRCS	+= cmdTableImpl.cc

CMDIF_OBJS	:= $(CMDIF_SRCS:%.cc=$(OBJ_DIR)/%.o)

CMDIF_INCS	:= \
		-I$(CMDIF_DIR)/if \
		-I$(CMDIF_DIR)/inc \
		-I$(SW_DIR)/common/if \
		-I$(SDK_INC_DIR)

all: $(CMDIF_OBJS) $(LIB_DIR)/$(CMDIF_LIBAR) $(LIB_DIR)/$(CMDIF_LIBSO) install-header-files-cmdif

# Build target 1 objects
$(OBJ_DIR)/%.o: $(CMDIF_SRC_DIR)/%.cc
	@mkdir -p $(@D)
	@cd $(<D)
	@echo "  CXX \t\t $@"
	@$(SELF_CXX) $(SELF_LDFLAGS) $(SELF_CFLAGS) $(CMDIF_CXXFLAGS) $(CMDIF_INCS) -o $@ $^

$(LIB_DIR)/$(CMDIF_LIBAR): $(CMDIF_OBJS)
	@mkdir -p $(@D)
	@cd $(<D)
	@echo "  AR \t\t $@"
	@$(SELF_AR) $(SELF_ARFLAGS) $@ $^

$(LIB_DIR)/$(CMDIF_LIBSO): $(CMDIF_OBJS)
	@mkdir -p $(@D)
	@cd $(<D)
	@echo "  CXXLD \t $@"
	@$(SELF_CXX) $(SELF_SOFLAGS) -o $@ $^

install-header-files-cmdif:
	@mkdir -p $(INC_DIR)
	@echo "  CPY \t\t $(CMDIF_DIR)/if"
	@$(SELF_CPY) $(CMDIF_DIR)/if/*.h $(INC_DIR)

clean-cmdif:
	$(SELF_RMV) $(CMDIF_OBJS) $(LIB_DIR)/$(CMDIF_LIBAR) $(LIB_DIR)/$(CMDIF_LIBSO)