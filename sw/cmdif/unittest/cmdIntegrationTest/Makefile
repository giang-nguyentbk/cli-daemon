# SDKSYSROOT is an env variable which should be exported by doing "source <path-to-SDK>/SDK-***/sysroot/env.sh
# which is automatically done by running atbuild-sdk.sh"
SDK_SYSROOT_DIR		:= $(SDKSYSROOT)
SDK_USR_DIR		:= $(SDK_SYSROOT_DIR)/usr
SDK_LIB_DIR		:= $(SDK_USR_DIR)/lib
SDK_INC_DIR		:= $(SDK_USR_DIR)/include

ROOT_DIR 	:= $(shell git rev-parse --show-toplevel)
BIN_DIR 	:= $(ROOT_DIR)/sw/cmdif/unittest/cmdIntegrationTest/bin
TARGET 		:= $(BIN_DIR)/cmdIntegrationTest

CFLAGS 		:= -c -Wall -Wextra -g
CXX 		:= g++

INCLUDE_DIR 	:= \
		-I$(ROOT_DIR)/sw/cmdif/if \
		-I$(ROOT_DIR)/sw/cmdif/inc \
		-I$(ROOT_DIR)/sw/cmdif/unittest/cmdIntegrationTest \
		-I$(SDK_INC_DIR)

SOURCE_PATH	:= $(ROOT_DIR)/sw/cmdif/src

SOURCES 	=
SOURCES 	+= cmdJobImpl.cc
SOURCES 	+= cmdRegisterImpl.cc
SOURCES 	+= cmdSyntaxGraph.cc
SOURCES 	+= cmdTableImpl.cc

OBJECTS 	:= $(SOURCES:%.cc=$(BIN_DIR)/%.o)

TEST 		:= $(ROOT_DIR)/sw/cmdif/unittest/cmdIntegrationTest/cmdIntegrationTest.cc
OBJECT_TEST	:= $(BIN_DIR)/cmdIntegrationTest.o

all: create_bin $(OBJECTS) $(OBJECT_TEST) $(TARGET)

create_bin:
	@mkdir -p $(BIN_DIR)

$(BIN_DIR)/%.o: $(SOURCE_PATH)/%.cc
	@echo "  CXX \t\t $@"
	@$(CXX) $(CFLAGS) $^ $(INCLUDE_DIR) -o $@

$(OBJECT_TEST): $(TEST)
	@echo "  CXX \t\t $@"
	@$(CXX) $(CFLAGS) $^ $(INCLUDE_DIR) -o $@

$(TARGET): $(OBJECTS) $(OBJECT_TEST)
	@echo "  CXXLD \t $@"
	@$(CXX) $^ -L$(SDK_LIB_DIR) -ltraceifa -litca -leventloopa -litcpubsuba -o $@

run:
	@$(TARGET)

val:
	sudo valgrind --leak-check=yes --leak-check=full --show-leak-kinds=all $(TARGET)

clean:
	rm -rf $(BIN_DIR)