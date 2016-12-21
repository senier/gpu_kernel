TARGET = hello_gpu
SRC_CC = main.cc
LIBS   = base config

# For kernel/interface.h stub
INC_DIR += $(PRG_DIR)

# For spec/x86_64/translation_table.h
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include

# For base/internal/page_size.h
INC_DIR += $(BASE_DIR)/src/include
