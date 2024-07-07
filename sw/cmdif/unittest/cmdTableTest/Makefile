# SDKSYSROOT is an env variable which should be exported by doing "source <path-to-SDK>/SDK-***/sysroot/env.sh
# which is automatically done by running atbuild-sdk.sh"
SDK_SYSROOT_DIR		:= $(SDKSYSROOT)
SDK_USR_DIR		:= $(SDK_SYSROOT_DIR)/usr
SDK_LIB_DIR		:= $(SDK_USR_DIR)/lib
SDK_INC_DIR		:= $(SDK_USR_DIR)/include

ROOT_DIR 	:= $(shell git rev-parse --show-toplevel)
TARGET 		:= cmdTableTest
BIN_DIR 	:= $(ROOT_DIR)/sw/cmdif/unittest/cmdTableTest/bin

CFLAGS 		:= -c -Wall -Wextra
CXX 		:= g++

INCLUDE_DIR 	:= \
		-I$(ROOT_DIR)/sw/cmdif/if \
		-I$(ROOT_DIR)/sw/cmdif/inc \
		-I$(ROOT_DIR)/sw/cmdif/unittest/cmdTableTest \
		-I$(ROOT_DIR)/sw/common/if \
		-I$(SDK_INC_DIR)

SOURCE1 	:= $(ROOT_DIR)/sw/cmdif/src/cmdTableImpl.cc
SOURCE2 	:= $(ROOT_DIR)/sw/cmdif/src/cmdSyntaxGraph.cc
TEST 		:= $(ROOT_DIR)/sw/cmdif/unittest/cmdTableTest/cmdTableTest.cc

OBJECTS 	=
OBJECTS 	+= $(BIN_DIR)/cmdTableImpl.o
OBJECTS 	+= $(BIN_DIR)/cmdSyntaxGraph.o
OBJECTS 	+= $(BIN_DIR)/cmdTableTest.o

all: create_bin $(OBJECTS) $(BIN_DIR)/$(TARGET)

create_bin:
	@mkdir -p $(BIN_DIR)

$(BIN_DIR)/cmdTableImpl.o: $(SOURCE1)
	@echo "  CXX \t\t $@"
	@$(CXX) $(CFLAGS) $^ $(INCLUDE_DIR) -o $@

$(BIN_DIR)/cmdSyntaxGraph.o: $(SOURCE2)
	@echo "  CXX \t\t $@"
	@$(CXX) $(CFLAGS) $^ $(INCLUDE_DIR) -o $@

$(BIN_DIR)/cmdTableTest.o: $(TEST)
	@echo "  CXX \t\t $@"
	@$(CXX) $(CFLAGS) $^ $(INCLUDE_DIR) -o $@

$(BIN_DIR)/$(TARGET): $(OBJECTS)
	@echo "  CXXLD \t $@"
	@$(CXX) $^ -L$(SDK_LIB_DIR) -ltraceifa -o $@

run:
	@$(BIN_DIR)/$(TARGET)

val:
	sudo valgrind --leak-check=yes --leak-check=full --show-leak-kinds=all $(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(BIN_DIR)