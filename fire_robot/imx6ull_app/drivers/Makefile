KER_DIR := /home/linux/imx6ull/linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
CUR_DIR := $(shell pwd)
ROOTFS_DIR := /home/linux/nfs/rootfs

obj-m += sht30.o
obj-m += bh1750.o
obj-m += flame.o

all:
	$(MAKE) -C $(KER_DIR) M=$(CUR_DIR) modules
	cp *.ko $(ROOTFS_DIR)/

clean:
	$(MAKE) -C $(KER_DIR) M=$(CUR_DIR) clean
	rm -f $(ROOTFS_DIR)/*.ko