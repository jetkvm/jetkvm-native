export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)
RK_SDK_BASE ?= /opt/jetkvm-native-buildkit
RK_APP_CROSS := $(RK_SDK_BASE)/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf
RK_MEDIA_OUTPUT := $(RK_SDK_BASE)/media/out
RK_MEDIA_INCLUDE_PATH := $(RK_MEDIA_OUTPUT)/include
RK_APP_MEDIA_LIBS_PATH :=  $(RK_MEDIA_OUTPUT)/lib

RK_APP_LDFLAGS = -L $(RK_APP_MEDIA_LIBS_PATH) -lpthread -lrockit -lrockchip_mpp  -lrga

LVGL_DIR_NAME 	?= lvgl
LVGL_DIR 		?= .
CC = $(RK_APP_CROSS)-gcc

CFLAGS = -I $(RK_MEDIA_INCLUDE_PATH) -I $(RK_MEDIA_INCLUDE_PATH)/libdrm -I$(LVGL_DIR)/ -I./ui
LDFLAGS ?=  -L $(RK_APP_MEDIA_LIBS_PATH) -lpthread -lrockit -lrockchip_mpp -lrga -lm -O3 -g0
BIN 			= jetkvm_native

#Collect the files to compile
MAINSRC = $(wildcard ./*.c ./ui/*.c)
BUILD_DIR 		= ./build
BUILD_OBJ_DIR 	= $(BUILD_DIR)/obj
BUILD_BIN_DIR 	= $(BUILD_DIR)/bin

include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

OBJEXT 			?= .o

AOBJS 			= $(ASRCS:.S=$(OBJEXT))
COBJS 			= $(CSRCS:.c=$(OBJEXT))

MAINOBJ 		= $(MAINSRC:.c=$(OBJEXT))

SRCS 			= $(ASRCS) $(CSRCS) $(MAINSRC)
OBJS 			= $(AOBJS) $(COBJS) $(MAINOBJ)
TARGET 			= $(addprefix $(BUILD_OBJ_DIR)/, $(patsubst ./%, %, $(OBJS)))

all: default

$(BUILD_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "CC $<"

default: $(TARGET)
	@mkdir -p $(dir $(BUILD_BIN_DIR)/)
	$(CC) -o $(BUILD_BIN_DIR)/$(BIN) $(TARGET) $(LDFLAGS)

clean:
	@echo "clean"
	@rm -rf build
