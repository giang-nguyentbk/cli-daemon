SHELL_SRC_DIR		:= $(SW_DIR)/shell

TARGET_CLISHELL		:= clishell

SHELL_SRCS		=
SHELL_SRCS		+= shell.c

SHELL_OBJS		:= $(SHELL_SRCS:%.c=$(OBJ_DIR)/%.o)

SHELL_INCDIR		:= \
			-I$(SW_DIR)/common/if

all: $(SHELL_OBJS) $(EXEC_DIR)/$(TARGET_CLISHELL)

# Build target 1 objects
$(OBJ_DIR)/%.o: $(SHELL_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@cd $(<D)
	@echo "  CC \t\t $@"
	@$(SELF_CC) $(SELF_CFLAGS) $(SHELL_INCDIR) -o $@ $<

$(EXEC_DIR)/$(TARGET_CLISHELL): $(SHELL_OBJS)
	@mkdir -p $(@D)
	@cd $(<D)
	@echo "  CCLD \t\t $@"
	@$(SELF_CC) $^ -o $@