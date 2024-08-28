CLID_SRC_DIR		:= $(SW_DIR)/clid

# TARGET_CLID_W_LIBAR	:= clid_ar
TARGET_CLID_W_LIBSO	:= clid_so

CLID_SRCS		=
CLID_SRCS		+= clid.c

CLID_OBJS		:= $(CLID_SRCS:%.c=$(OBJ_DIR)/%.o)

CLID_INCDIR		:= \
			-I$(SW_DIR)/common/if \
			-I$(SW_DIR)/cmdif/inc \
			-I$(SDK_INC_DIR)

# all: $(CLID_OBJS) $(EXEC_DIR)/$(TARGET_CLID_W_LIBAR) $(EXEC_DIR)/$(TARGET_CLID_W_LIBSO)
all: $(CLID_OBJS) $(EXEC_DIR)/$(TARGET_CLID_W_LIBSO)

# Build target 1 objects
$(OBJ_DIR)/%.o: $(CLID_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@cd $(<D)
	@echo "  CC \t\t $@"
	@$(SELF_CC) $(SELF_CFLAGS) $(CLID_INCDIR) -o $@ $<

##### CANNOT BUILD STATIC LIBRARY FOR LEVEL-2 LIBRARY OR UPWARD #####
##### TO DO THIS, COMBINE THIS LIBRARY'S OBJECTS WITH ALL OTHER LEVEL-1 OBJECT BY YOURSELF WITH 'AR' COMMAND #####
##### OR JUST USING GNU-LIBTOOL OR SIMILARITIES INSTEAD #####
# $(EXEC_DIR)/$(TARGET_CLID_W_LIBAR): $(CLID_OBJS)
# 	@mkdir -p $(@D)
# 	@cd $(<D)
# 	@echo "  CCLD \t\t $@"
# 	@$(SELF_CC) $^ -L$(SDK_LIB_DIR) -litca -ltraceifa -o $@

$(EXEC_DIR)/$(TARGET_CLID_W_LIBSO): $(CLID_OBJS)
	@mkdir -p $(@D)
	@cd $(<D)
	@echo "  CCLD \t\t $@"
	@$(SELF_CC) $^ -L$(SDK_LIB_DIR) -litc -ltraceif -o $@