#-----------------------------------------------------------------------------------------------------------------------
# Build Configuration
#-----------------------------------------------------------------------------------------------------------------------

# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

.PHONY: force

# Use clang by default
CC				:= clang
AR				:= llvm-ar

# Should we compile in debug
DEBUG			?= 0

# Should we compile in debug or not
SPIDIR_DEBUG	?= $(DEBUG)

# The spidir compilation target (given to cargo)
SPIDIR_TARGET	?= x86_64-unknown-none

# The cflags
CFLAGS			?=

#-----------------------------------------------------------------------------------------------------------------------
# Build constants
#-----------------------------------------------------------------------------------------------------------------------

# The output directories
ifeq ($(DEBUG),1)
OUT_DIR			:= out/debug
else
OUT_DIR			:= out/release
endif

BIN_DIR 		:= $(OUT_DIR)/bin
BUILD_DIR		:= $(OUT_DIR)/build

# Add some flags that we require to work
EQ_CFLAGS		:= $(CFLAGS)
EQ_CFLAGS		+= -Wall -Werror
EQ_CFLAGS		+= -g
EQ_CFLAGS		+= -Iinclude

# Get the sources along side all of the objects and dependencies
SRCS 			:= src/linux/rbtree.c src/eevdf.c
OBJS 			:= $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS 			:= $(OBJS:%.o=%.d)

TEST_SRCS 		:= test/main.c
TEST_OBJS 		:= $(TEST_SRCS:%=$(BUILD_DIR)/%.o)
TEST_DEPS 		:= $(TEST_OBJS:%.o=%.d)


# The default rule
.PHONY: default
default: all

# All the rules
.PHONY: all
all: $(BIN_DIR)/libeevdf-queue.a

test: $(BIN_DIR)/eevdf-queue


#-----------------------------------------------------------------------------------------------------------------------
# Rules
#-----------------------------------------------------------------------------------------------------------------------

-include $(DEPS)
-include $(TEST_DEPS)

$(BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(EQ_CFLAGS) -MMD -c $< -o $@

$(BIN_DIR)/libeevdf-queue.a: $(OBJS)
	@echo AR $@
	@mkdir -p $(@D)
	@$(AR) rc $@ $^

$(BIN_DIR)/eevdf-queue: $(TEST_OBJS)
	@echo LD $@
	@mkdir -p $(@D)
	@$(CC) $(EQ_CFLAGS) $^ -o $@

clean:
	rm -rf out
