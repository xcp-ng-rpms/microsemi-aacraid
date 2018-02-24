# Adaptec aacraid
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/pci.h ] ; then \
	if grep shutdown ${TOPDIR}/include/linux/pci.h >/dev/null 2>/dev/null ; then \
		echo -DPCI_HAS_SHUTDOWN ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/pci.h ] ; then \
	if grep pci_enable_msi ${TOPDIR}/include/linux/pci.h >/dev/null 2>/dev/null ; then \
		echo -DPCI_HAS_ENABLE_MSI ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/pci.h ] ; then \
	if grep pci_disable_msi ${TOPDIR}/include/linux/pci.h >/dev/null 2>/dev/null ; then \
		echo -DPCI_HAS_DISABLE_MSI ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/pci.h ] ; then \
	if grep pci_set_dma_max_seg_size ${TOPDIR}/include/linux/pci.h >/dev/null 2>/dev/null ; then \
		echo -DPCI_HAS_SET_DMA_MAX_SEG_SIZE ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if uname -r | grep uek >/dev/null 2>/dev/null ; then \
		echo -DAAC_DISCOVERY_DELAY ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/drivers/scsi/hosts.h ] ; then \
	if grep vary_io ${TOPDIR}/drivers/scsi/hosts.h >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_VARY_IO ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/drivers/scsi/hosts.c ] ; then \
	if grep scsi_in_detection ${TOPDIR}/drivers/scsi/hosts.c >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_SCSI_IN_DETECTION ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/drivers/scsi/scsi_syms.c ] ; then \
	if grep scsi_scan_host ${TOPDIR}/drivers/scsi/scsi_syms.c >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_SCSI_SCAN_HOST ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/drivers/scsi/scsi_scan.c ] ; then \
	if grep EXPORT_SYMBOL.*scsi_scan_host ${TOPDIR}/drivers/scsi/scsi_scan.c >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_SCSI_SCAN_HOST ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/init/main.c ] ; then \
	if grep EXPORT_SYMBOL.*reset_devices ${TOPDIR}/init/main.c >/dev/null 2>/dev/null ; then \
		echo -DHAS_RESET_DEVICES ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/delay.h ] ; then \
	if grep ssleep ${TOPDIR}/include/linux/delay.h >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_SSLEEP ; \
	fi ; \
fi)
#AAC_FLAGS += $(shell if [ -s ${TOPDIR}/drivers/scsi/libata-core.c ] ; then \
#	if grep EXPORT_SYMBOL.*ssleep ${TOPDIR}/drivers/scsi/libata-core.c >/dev/null 2>/dev/null ; then \
#		echo -DSCSI_HAS_SSLEEP ; \
#	fi ; \
#fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/scsi/scsi_device.h ] ; then \
	if grep scsi_device_online ${TOPDIR}/include/scsi/scsi_device.h >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_SCSI_DEVICE_ONLINE ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/scsi/scsi_host.h ] ; then \
	if grep dump_poll ${TOPDIR}/include/scsi/scsi_host.h >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_DUMP ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/scsi/scsi_host.h ] ; then \
	if grep dump_sanity_check ${TOPDIR}/include/scsi/scsi_host.h >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_DUMP_SANITY_CHECK ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/drivers/scsi/scsi_dump.h ] ; then \
	echo -DSCSI_HAS_DUMP -DSCSI_HAS_DUMP_SANITY_CHECK ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/diskdump.h ] ; then \
	echo -DHAS_DISKDUMP_H ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/diskdump.h -o -s ${TOPDIR}/include/asm/diskdump.h ] ; then \
	if grep diskdump_ssleep ${TOPDIR}/include/linux/diskdump.h ${TOPDIR}/include/asm/diskdump.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_DUMP_SSLEEP ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/diskdump.h -o -s ${TOPDIR}/include/asm/diskdump.h ] ; then \
	if grep diskdump_mdelay ${TOPDIR}/include/linux/diskdump.h ${TOPDIR}/include/asm/diskdump.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_DUMP_MDELAY ; \
	fi ; \
fi)
# AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/diskdumplib.h ] ; then \
#	echo -DHAS_DISKDUMPLIB_H ; \
# fi)
AAC_FLAGS += $(shell if [ ! -s ${TOPDIR}/include/asm/setup.h ] ; then \
	if [ -s ${TOPDIR}/include/asm/bootsetup.h ] ; then \
		echo -DHAS_BOOTSETUP_H ; \
	else \
		echo -DHAS_NOT_SETUP ; \
	fi \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linix/compile.h ] ; then \
	echo -DHAS_COMPILE_H ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/scsi/scsi_host.h ] ; then \
	if grep my_devices ${TOPDIR}/include/scsi/scsi_host.h >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_MY_DEVICES ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/scsi/scsi_host.h ] ; then \
	if grep "enum.*shost_state" ${TOPDIR}/include/scsi/scsi_host.h >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_SHOST_STATE_ENUM ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/scsi/scsi_cmnd.h ] ; then \
	if grep "scsi_dma_map *(.*)" ${TOPDIR}/include/scsi/scsi_cmnd.h >/dev/null 2>/dev/null ; then \
		echo -DSCSI_HAS_DMA_MAP ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/types.h ] ; then \
	if grep __bitwise ${TOPDIR}/include/linux/types.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_BITWISE_TYPE ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/byteorder/generic.h ] ; then \
	if grep le32_add_cpu ${TOPDIR}/include/linux/byteorder/generic.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_LE32_ADD_CPU ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/types.h ] ; then \
	if grep sector_t ${TOPDIR}/include/linux/types.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_SECTOR_T ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/asm-i386/types.h ] ; then \
	if grep sector_t ${TOPDIR}/include/asm-i386/types.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_SECTOR_T ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/delay.h ] ; then \
	if grep "void[ 	][ 	]*msleep[ 	]*(unsigned int msecs)" ${TOPDIR}/include/linux/delay.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_MSLEEP ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/sched.h ] ; then \
	if grep "static inline.*find_task_by_pid" ${TOPDIR}/include/linux/sched.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_FIND_TASK_BY_PID ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/modules/ksyms.ver ] ; then \
	if grep find_task_by_pid ${TOPDIR}/include/linux/modules/ksyms.ver >/dev/null 2>/dev/null ; then \
		echo -DHAS_FIND_TASK_BY_PID ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/Module.symvers ] ; then \
	if grep find_task_by_pid ${TOPDIR}/Module.symvers >/dev/null 2>/dev/null ; then \
		echo -DHAS_FIND_TASK_BY_PID ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/kernel/kthread.c ] ; then \
	if grep "EXPORT_SYMBOL(kthread_stop)" ${TOPDIR}/kernel/kthread.c >/dev/null 2>/dev/null ; then \
		echo -DHAS_KTHREAD ; \
	fi ; \
fi)
AAC_FLAGS += $(shell echo "${EXTRAVERSION}" | if grep BOOT >/dev/null 2>/dev/null ; then \
	echo -DHAS_BOOT_CONFIG ; \
fi)
AAC_FLAGS += $(shell echo "${EXTRAVERSION}" | if grep kdump >/dev/null 2>/dev/null ; then \
	echo -DHAS_KDUMP_CONFIG ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/.config ] ; then \
	if grep "CONFIG_LOCALVERSION.*-kdump" ${TOPDIR}/.config >/dev/null 2>/dev/null ; then \
		echo -DHAS_KDUMP_CONFIG ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/mm/slab.c ] ; then \
	if grep "EXPORT_SYMBOL(kzalloc)" ${TOPDIR}/mm/slab.c >/dev/null 2>/dev/null ; then \
		echo -DHAS_KZALLOC ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/scatterlist.h ] ; then \
	if grep "sg_page(sg)" ${TOPDIR}/include/linux/scatterlist.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_SG_PAGE ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if [ -s ${TOPDIR}/include/linux/interrupt.h ] ; then \
	if !grep "typedef.*irqreturn_t.*irq_handler_t.*int.*void" ${TOPDIR}/include/linux/interrupt.h >/dev/null 2>/dev/null ; then \
		echo -DHAS_NEW_IRQ_HANDLER_T ; \
	fi ; \
fi)
AAC_FLAGS += $(shell if grep RHEL_VERSION ${TOPDIR}/Makefile >/dev/null 2>/dev/null ; then \
	RHEL_VERSION=`sed -n 's/RHEL_VERSION[ 	]*=[ 	]*\(.*\)/\1/p' ${TOPDIR}/Makefile` ; \
	RHEL_UPDATE=`sed -n 's/RHEL_UPDATE[ 	]*=[ 	]*\(.*\)/\1/p' ${TOPDIR}/Makefile` ; \
	if [ ! -z "$${RHEL_UPDATE}" ] ; then \
		echo -DRHEL_VERSION=$${RHEL_VERSION} -DRHEL_UPDATE=$${RHEL_UPDATE} ; \
	else \
		echo -DRHEL_VERSION=$${RHEL_VERSION} ; \
	fi ; \
fi)

AAC_FLAGS += $(shell if grep "Server 15 SP1" /etc/os-release >/dev/null 2>/dev/null ; then \
	echo -DAAC_SAS_SMP_BSG_JOB; \
fi)

AAC_FLAGS += $(shell if grep "SUSE Linux Enterprise Server 12 SP5" /etc/os-release >/dev/null 2>/dev/null ; then \
        echo -DAAC_SAS_SMP_BSG_JOB; \
fi)
#
# Legacy makefile syntax series 2.x.x kernel versions
#
ifeq (${VERSION},2) # 2.x.x
ifeq (${PATCHLEVEL},2) # 2.2.x

CFILES_DRIVER=linit.c aachba.c commctrl.c comminit.c commsup.c \
	dpcsup.c rx.c sa.c rkt.c nark.c src.c fwdebug.c csmi.c adbg.c

IFILES_DRIVER=aacraid.h compat.h adbg.h

ALL_SOURCE=${CFILES_DRIVER} ${IFILES_DRIVER} 

TARGET_OFILES=${CFILES_DRIVER:.c=.o}

ifndef GCCVERSION
GCCVERSION=2.96
endif

GCCMACHINE:=$(shell ls -d /usr/lib/gcc-lib/*/${GCCVERSION} | sed -n 1s@/${GCCVERSION}@@p)

INCS=-I. -I.. -I../../../include -I/usr/src/linux/include -I/usr/src/linux/drivers/scsi 
INCS=-nostdinc -I${GCCMACHINE}/${GCCVERSION}/include -I. -I..

WARNINGS= -w -Wall -Wno-unused -Wno-switch -Wno-missing-prototypes -Wno-implicit

COMMON_FLAGS=\
	-D__KERNEL__=1 -DUNIX -DCVLOCK_USE_SPINLOCK -DLINUX \
	-Wall -Wstrict-prototypes \
	${INCS} \
	${WARNINGS} \
	-O2 -fomit-frame-pointer

AACFLAGS=${COMMON_FLAGS} ${CFLAGS} ${EXTRA_FLAGS} ${AAC_FLAGS}
COMPILE.c=${CC} ${AACFLAGS} ${TARGET_ARCH} -c

.SUFFIXES:
.SUFFIXES: .c .o .h .a

all: source ${TARGET_OFILES} aacraid.o

modules: all

source: ${ALL_SOURCE}

clean:
	rm *.o

aacraid.o: source ${TARGET_OFILES}
	ld -r -o $@ $(TARGET_OFILES)
	cp -r aacraid.o ../

endif # 2.2.x

ifeq (${PATCHLEVEL},4) # 2.4.x

EXTRA_CFLAGS	+= -I$(TOPDIR)/drivers/scsi ${EXTRA_FLAGS} ${AAC_FLAGS}

O_TARGET	:= aacraid.o
obj-m		:= $(O_TARGET)

obj-y		:= linit.o aachba.o commctrl.o comminit.o commsup.o \
		   dpcsup.o rx.o sa.o rkt.o nark.o src.o fwdebug.o csmi.o adbg.o

include $(TOPDIR)/Rules.make

endif # 2.4.x

#
#This needs to be merged with the else case
#since they are the same
#
ifeq (${PATCHLEVEL},6) # 2.6.x

obj-m := aacraid.o

aacraid-objs	:= linit.o aachba.o commctrl.o comminit.o commsup.o \
		   dpcsup.o rx.o sa.o rkt.o nark.o src.o fwdebug.o csmi.o adbg.o

EXTRA_CFLAGS	:= -Idrivers/scsi ${EXTRA_FLAGS} ${AAC_FLAGS}
endif # 2.6.x

else

obj-m := aacraid.o

aacraid-objs	:= linit.o aachba.o commctrl.o comminit.o commsup.o \
		   dpcsup.o rx.o sa.o rkt.o nark.o src.o fwdebug.o csmi.o adbg.o

EXTRA_CFLAGS	:= -Idrivers/scsi ${EXTRA_FLAGS} ${AAC_FLAGS}

endif


KERNEL_BUILD_PATH := "/lib/modules/$(shell uname -r)/build"
KNPROC := ${shell nproc}

all: release

debug:
	make -j$(KNPROC) EXTRA_CFLAGS="-g -DDEBUG" -C ${KERNEL_BUILD_PATH} M=$(PWD) modules

release:
	make -j$(KNPROC) -C ${KERNEL_BUILD_PATH} M=$(PWD) modules

cscope:
	cscope -q -R -b -i aacraid_cscope.files

clean:
	make -C ${KERNEL_BUILD_PATH} M=$(PWD) clean
