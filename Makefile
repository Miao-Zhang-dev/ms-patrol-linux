#
# Makefile
# LVGL framebuffer + GUI Guider + fire_robot app
#

CC := arm-linux-gnueabihf-gcc

LVGL_DIR := /home/linux/nfs/rootfs/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer
APP_DIR := $(LVGL_DIR)/fire_robot/imx6ull_app
DRIVER_DIR := $(APP_DIR)/drivers

SYSROOT := /home/linux/nfs/rootfs

export LVGL_DIR_NAME := lvgl
export LV_DRIVERS_DIR_NAME := lv_drivers

BIN := demo

KERNELDIR ?= /home/linux/imx6ull/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek

CFLAGS := -O3 -g0 -std=gnu99

SQLITE_INC := /home/linux/nfs/rootfs/usr/include
SQLITE_LIB := /home/linux/nfs/rootfs/usr/lib/libsqlite3.a

LDFLAGS := $(SQLITE_LIB) -ldl -pthread -lm

MAINSRC := ./main.c

INC_FLAGS := \
	-I$(LVGL_DIR) \
	-I$(LVGL_DIR)/lvgl \
	-I$(LVGL_DIR)/lvgl/src \
	-I$(LVGL_DIR)/lv_drivers \
	-I$(LVGL_DIR)/lv_drivers/display \
	-I$(LVGL_DIR)/lv_drivers/indev \
	-I$(LVGL_DIR)/gui/custom \
	-I$(LVGL_DIR)/gui/generated \
	-I$(LVGL_DIR)/gui/generated/guider_fonts \
	-I$(LVGL_DIR)/gui/generated/guider_customer_fonts \
	-I$(LVGL_DIR)/gui/generated/images \
	-I$(APP_DIR)/config \
	-I$(APP_DIR)/include \
	-I$(APP_DIR)/common \
	-I$(APP_DIR)/common/mailbox \
	-I$(APP_DIR)/app \
	-I$(APP_DIR)/sensors \
	-I$(APP_DIR)/threads \
	-I$(APP_DIR)/ui \
	-I$(APP_DIR)/camera \
	-I$(APP_DIR)/storage \
	-I$(SYSROOT)/usr/include \
	-I$(SQLITE_INC)

include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

CSRCS += $(LVGL_DIR)/mouse_cursor_icon.c

CSRCS += $(wildcard $(LVGL_DIR)/gui/custom/*.c)
CSRCS += $(wildcard $(LVGL_DIR)/gui/generated/*.c)
CSRCS += $(wildcard $(LVGL_DIR)/gui/generated/guider_fonts/*.c)
CSRCS += $(wildcard $(LVGL_DIR)/gui/generated/images/*.c)

CSRCS += $(wildcard $(APP_DIR)/common/*.c)
CSRCS += $(wildcard $(APP_DIR)/common/mailbox/*.c)
CSRCS += $(wildcard $(APP_DIR)/sensors/*.c)

CSRCS += $(APP_DIR)/app/fire_robot_app.c
CSRCS += $(APP_DIR)/app/app_alarm.c

CSRCS += $(APP_DIR)/threads/sensor_thread.c
CSRCS += $(APP_DIR)/threads/warning_thread.c
CSRCS += $(APP_DIR)/threads/db_thread.c

CSRCS += $(APP_DIR)/ui/ui_update.c

CSRCS += $(APP_DIR)/camera/camera_capture.c
CSRCS += $(APP_DIR)/camera/video.c

CSRCS += $(APP_DIR)/storage/db.c

OBJEXT ?= .o

AOBJS := $(ASRCS:.S=$(OBJEXT))
COBJS := $(CSRCS:.c=$(OBJEXT))
MAINOBJ := $(MAINSRC:.c=$(OBJEXT))

.PHONY: all app ko clean show

all: app ko

app: $(AOBJS) $(COBJS) $(MAINOBJ)
	$(CC) -o $(BIN) $(MAINOBJ) $(AOBJS) $(COBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@
	@echo "CC $<"

ko:
	@echo "===== Build kernel modules in $(DRIVER_DIR) ====="
	$(MAKE) -C $(DRIVER_DIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KERNELDIR=$(KERNELDIR)

clean:
	rm -f $(BIN) $(AOBJS) $(COBJS) $(MAINOBJ)
	@echo "===== Clean kernel modules ====="
	-$(MAKE) -C $(DRIVER_DIR) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KERNELDIR=$(KERNELDIR) clean

show:
	@echo "LVGL_DIR = $(LVGL_DIR)"
	@echo "APP_DIR = $(APP_DIR)"
	@echo "DRIVER_DIR = $(DRIVER_DIR)"
	@echo "KERNELDIR = $(KERNELDIR)"
	@echo "SYSROOT = $(SYSROOT)"
	@echo "INC_FLAGS = $(INC_FLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "CSRCS = $(CSRCS)"

camera_test: $(APP_DIR)/camera/camera_test.c \
	$(APP_DIR)/camera/camera_capture.c \
	$(APP_DIR)/camera/video.c
	$(CC) $(CFLAGS) $(INC_FLAGS) \
		$(APP_DIR)/camera/camera_test.c \
		$(APP_DIR)/camera/camera_capture.c \
		$(APP_DIR)/camera/video.c \
		-o camera_test \
		-lm -pthread

WEB_BIN := fire_robot_web
SQLITE_INC := /home/linux/nfs/rootfs/usr/include
SQLITE_LIB := /home/linux/nfs/rootfs/usr/lib/libsqlite3.a

INC_FLAGS += -I$(APP_DIR)/web
INC_FLAGS += -I$(SQLITE_INC)

.PHONY: web

web: $(APP_DIR)/web/web_server.c
	$(CC) $(CFLAGS) $(INC_FLAGS) \
		$(APP_DIR)/web/web_server.c \
		-o $(WEB_BIN) \
		$(SQLITE_LIB) -ldl -pthread -lm