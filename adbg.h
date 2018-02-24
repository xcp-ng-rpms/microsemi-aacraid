/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2001 Red Hat Inc.	<alan@redhat.com>
 *
 * based on the old aacraid driver that is..
 * Adaptec aacraid device driver for Linux.
 *
 * Copyright (c) 2000-2010 Adaptec, Inc. (aacraid@adaptec.com)
 * Copyright (c) 2010-2015 PMC-Sierra, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Module Name:
 *  adbg.h
 *
 * Abstract: Contains all routines for control of the debug data.
 *
 */

#ifndef _ADBG_H_
#define _ADBG_H_

/*Suppress unused variable warning*/
#define UNUSED(x)       (void)(x)

#define FLAG_SIZE(x)	((uint64_t)x)

#define AAC_STATUS_INFO		FLAG_SIZE(1<<0)
#define AAC_DEBUG_INIT			FLAG_SIZE(1<<1)
#define AAC_DEBUG_SETUP		FLAG_SIZE(1<<2)
#define AAC_DEBUG_TIMING		FLAG_SIZE(1<<3)
#define AAC_DEBUG_AIF			FLAG_SIZE(1<<4)
#define AAC_DEBUG_IOCTL		FLAG_SIZE(1<<5)
#define AAC_DEBUG_IOCTL_SENDFIB	FLAG_SIZE(1<<6)
#define AAC_DEBUG_AAC_CONFIG		FLAG_SIZE(1<<7)
#define AAC_DEBUG_RESET		FLAG_SIZE(1<<8)
#define AAC_DEBUG_FIB			FLAG_SIZE(1<<9)
#define AAC_DEBUG_CONTEXT		FLAG_SIZE(1<<10)
#define AAC_DEBUG_2TB			FLAG_SIZE(1<<11)
#define AAC_DEBUG_IO			FLAG_SIZE(1<<12)
#define AAC_DEBUG_SG			FLAG_SIZE(1<<13)
#define AAC_DEBUG_VM_NAMESERVE		FLAG_SIZE(1<<14)
#define AAC_DEBUG_SERIAL		FLAG_SIZE(1<<15)
#define AAC_DEBUG_SYNCHRONIZE		FLAG_SIZE(1<<16)
#define AAC_DEBUG_SHUTDOWN		FLAG_SIZE(1<<17)
#define AAC_DEBUG_MSIX			FLAG_SIZE(1<<18)
#define AAC_DEBUG_LOG			FLAG_SIZE(1<<19)
#define AAC_DEBUG_SMP			FLAG_SIZE(1<<20)
#define AAC_DEBUG_SAS			FLAG_SIZE(1<<21)
#define AAC_DEBUG_SRB			FLAG_SIZE(1<<22)

/*
 * 22 to 31 bits are for future debug
 */

#define AAC_PCI_HAS_MSI			FLAG_SIZE(1<<32)
#define AAC_SUPPORTED_POWER_MANAGEMENT  FLAG_SIZE(1<<33)
#define AAC_SUPPORTED_JBOD		FLAG_SIZE(1<<34)
#define AAC_BOOTCD			FLAG_SIZE(1<<35)
#define AAC_SCSI_HAS_VARY_IO		FLAG_SIZE(1<<37)
#define AAC_SAI_READ_CAPACITY_16	FLAG_SIZE(1<<38)
#define AAC_FWPRINTF			FLAG_SIZE(1<<39)
#define AAC_DPRINTK			FLAG_SIZE(1<<40)

#define LOG_SETUP			(AAC_DEBUG_INIT| AAC_DEBUG_IOCTL)

//#define CONFIG_SCSI_AACRAID_LOGGING
#define CONFIG_SCSI_AACRAID_LOGGING_PRINTK
//#define CONFIG_SCSI_AACRAID_LOGGING_FUNCTION_PRINTK

/*
 * Disable fw printing for now
 */
//#define CONFIG_SCSI_AACRAID_LOGGING_FW_DEBUG

/********** Enable other features  ***********/

#ifdef CONFIG_SCSI_AACRAID_LOGGING
#define AAC_CHECK_LOGGING(DEV, BITS, LVL, TEST, ...)   \
({                                          \
	if(DEV->logging_level & BITS)           \
	{                                       \
		printk(LVL "%s%u: " TEST,DEV->name,DEV->id, ##__VA_ARGS__);\
		fwprintf((DEV, HBA_FLAGS_DBG_FW_PRINT_B, ##__VA_ARGS__));\
	}\
})
#elif  defined(CONFIG_SCSI_AACRAID_LOGGING_PRINTK)
#define AAC_CHECK_LOGGING(DEV, BITS, LVL, TEST, ...) \
({                                              \
	printk(LVL "%s:%s%u: " TEST,AAC_DRIVERNAME,DEV->name,DEV->id,##__VA_ARGS__);        \
})
#elif  defined(CONFIG_SCSI_AACRAID_LOGGING_FUNCTION_PRINTK)
#define AAC_CHECK_LOGGING(DEV, BITS, LVL, TEST, ...) \
({                                              \
	printk(LVL "%s:%s%u: %s:" TEST,AAC_DRIVERNAME,DEV->name,DEV->id,__FUNCTION__,##__VA_ARGS__);        \
})
#else
#define AAC_CHECK_LOGGING(DEV, BITS, CMD, TEST,...)
#endif

/********** Setup FW print ************/
#if defined(CONFIG_SCSI_AACRAID_LOGGING_FW_DEBUG)
#define AAC_FW_LOG(DEV, BITS, FLAGS, TEST, ...) \
({						\
	aac_fw_printf(DEV, FLAGS, TEST, ##__VA_ARGS__); \
})
#else
#define AAC_FW_LOG(DEV, BITS, FLAGS, TEST, ...)
#endif

#define adbg_dev() \
	printk(KERN_ERR,"Line-%d,Function-%s,File-%s",__LINE__,__FUNCTION__,__FILE__);

#define adbg(DEV, LVL, TEST,...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_LOG, LVL, TEST, ##__VA_ARGS__)

#if defined(AAC_DETAILED_STATUS_INFO)
#define adbg_info(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_STATUS_INFO, LVL, TEST, ##__VA_ARGS__)
#define fdbg_info(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_info(DEV, LVL, TEST, ...)
#define fdbg_info(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_INIT)
#define adbg_init(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_INIT,  LVL, TEST,##__VA_ARGS__)
#define fdbg_init(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_init(DEV, LVL, TEST, ...)
#define fdbg_init(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SETUP)
#define adbg_setup(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SETUP, LVL, TEST, ##__VA_ARGS__)
#define fdbg_setup(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_setup(DEV, LVL, TEST, ...)
#define fdbg_setup(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_AIF)
#define adbg_aif(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_AIF, LVL ,TEST,##__VA_ARGS__)
#define fdbg_aif(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_aif(DEV, LVL, TEST, ...)
#define fdbg_aif(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_IOCTL)
#define adbg_ioctl(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_IOCTL, LVL, TEST, ##__VA_ARGS__)
#define fdbg_ioctl(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_ioctl(DEV, LVL, TEST, ...)
#define fdbg_ioctl(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_AAC_CONFIG)
#define adbg_conf(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_AAC_CONFIG, LVL, TEST,  ##__VA_ARGS__)
#define fdbg_conf(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_conf(DEV, LVL, TEST, ...)
#define fdbg_conf(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_RESET)
#define adbg_reset(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_RESET, LVL, TEST,  ##__VA_ARGS__)
#define fdbg_reset(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_reset(DEV, LVL, TEST, ...)
#define fdbg_reset(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_FIB)
#define adbg_fib(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_FIB, LVL, TEST,  ##__VA_ARGS__)
#define fdbg_fib(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_fib(DEV, LVL, TEST, ...)
#define fdbg_fib(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_CONTEXT)
#define adbg_context(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_CONTEXT, LVL, TEST,  ##__VA_ARGS__)
#define fdbg_context(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_context(DEV, LVL, TEST, ...)
#define fdbg_context(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_2TB)
#define adbg_2tb(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_2TB,  LVL, TEST, ##__VA_ARGS__)
#define fdbg_2tb(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_2tb(DEV, LVL, TEST, ...)
#define fdbg_2tb(DEV, LVL, TEST, ...)
#endif


#if defined(AAC_DEBUG_INSTRUMENT_SENDFIB)
#define adbg_sendfib(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_SENDFIB, LVL, TEST,  ##__VA_ARGS__)
#define fdbg_sendfib(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_sendfib(DEV, LVL, TEST, ...)
#define fdbg_sendfib(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_IO)
#define adbg_io(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_IO, LVL, TEST,  ##__VA_ARGS__)
#define fdbg_io(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_io(DEV, LVL, TEST, ...)
#define fdbg_io(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SG)
#define adbg_sg(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV,  AAC_DEBUG_SG,  LVL, TEST, ##__VA_ARGS__)
#define fdbg_sg(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_sg(DEV, LVL, TEST, ...)
#define fdbg_sg(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE)
#define adbg_vm(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_VM_NAMESERVE, LVL, TEST, ##__VA_ARGS__)
#define fdbg_vm(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_vm(DEV, LVL, TEST, ...)
#define fdbg_vm(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE)
#define adbg_sync(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SYNCHRONIZE, LVL, TEST, ##__VA_ARGS__)
#define fdbg_sync(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_sync(DEV, LVL, TEST, ...)
#define fdbg_sync(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN)
#define adbg_shut(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SHUTDOWN, LVL, TEST, ##__VA_ARGS__)
#define fdbg_shut(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_shut(DEV, LVL, TEST, ...)
#define fdbg_shut(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_MSIX)
#define adbg_msix(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_MSIX, LVL, TEST, ##__VA_ARGS__)
#define fdbg_msix(DEV, FLAGS, TEST, ...) \
	AAC_FW_LOG(DEV, AAC_DEBUG_INIT, FLAGS, TEST, ##__VA_ARGS__)
#else
#define adbg_msix(DEV, LVL, TEST, ...)
#define fdbg_msix(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SMP)
#define adbg_smp(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SMP, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_smp(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SAS)
#define adbg_sas(DEV, LVL, TEST, ...) \
	AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SMP, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_sas(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SRB)
#define adbg_srb(DEV, LVL, TEST, ...) \
    AAC_CHECK_LOGGING(DEV, AAC_DEBUG_SRB, LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_srb(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_FIB) || defined(AAC_DEBUG_INSTRUMENT_MSIX)
#define adbg_fib_or_msix(DEV, LVL,TEST, ...) \
	AAC_CHECK_LOGGING(DEV, (AAC_DEBUG_FIB|AAC_DEBUG_MSIX), LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_fib_or_msix(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_IOCTL) && defined(AAC_DEBUG_INSTRUMENT_AIF)
#define adbg_ioctl_and_aif(DEV, LVL, TEST, ...)\
	AAC_CHECK_LOGGING(DEV, (AAC_DEBUG_IOCTL|AAC_DEBUG_AIF), LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_ioctl_and_aif(DEV, LVL, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE)
#define adbg_init_or_vm(DEV, LVL, TEST, ...)\
	AAC_CHECK_LOGGING(DEV, (AAC_DEBUG_INIT|AAC_DEBUG_VM), LVL, "%s: "TEST,__FUNCTION__,##__VA_ARGS__)
#define fdbg_init_or_vm(DEV, FLAGS, TEST, ...)\
	AAC_FW_LOG(DEV, (AAC_DEBUG_INIT|AAC_DEBUG_VM), FLAGS, "%s: "TEST,__FUNCTION__,##__VA_ARGS__)
#else
#define adbg_init_or_vm(DEV, LVL, TEST, ...)
#define fdbg_init_or_vm(DEV, FLAGS, TEST, ...)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_INIT) || defined(AAC_DEBUG_INSTRUMENT_AIF)
#define adbg_init_or_aif(DEV, LVL, TEST, ...)\
	AAC_CHECK_LOGGING(DEV, (AAC_DEBUG_INIT|AAC_DEBUG_AIF), LVL, TEST, ##__VA_ARGS__)
#else
#define adbg_init_or_aif(DEV, LVL, TEST, ...)
#endif

#define aac_emerg(a, fmt, ...)\
	dev_emerg(&(a)->pdev->dev,"%s%d:%s:"fmt,a->name,a->id,__FUNCTION__,##__VA_ARGS__)
#define aac_alert(a, fmt, ...)\
	dev_alert(&(a)->pdev->dev,"%s%d:%s:"fmt,a->name,a->id,__FUNCTION__,##__VA_ARGS__)
#define aac_crit(a, fmt, ...)\
	dev_crit(&(a)->pdev->dev,"%s%d:%s:"fmt,a->name,a->id,__FUNCTION__,##__VA_ARGS__)
#define aac_err(a, fmt, ...)\
	dev_err(&(a)->pdev->dev,"%s%d:%s:"fmt,a->name,a->id,__FUNCTION__,##__VA_ARGS__)
#define aac_warn(a, fmt, ...)\
	dev_warn(&(a)->pdev->dev,"%s%d:%s:"fmt,a->name,a->id,__FUNCTION__,##__VA_ARGS__)
#define aac_notice(a, fmt, ...)\
	dev_notice(&(a)->pdev->dev,"%s%d:%s:"fmt,a->name,a->id,__FUNCTION__,##__VA_ARGS__)
#define aac_info(a, fmt, ...)\
	dev_info(&(a)->pdev->dev,"%s%d:"fmt,a->name,a->id,##__VA_ARGS__)
#define aac_dbg(a, fmt, ...)\
	dev_dbg(&(a)->pdev->dev,"%s%d:%s:"fmt,a->name,a->id,__FUNCTION__,##__VA_ARGS__)

/********** Below contains struct definitions  ***********/
struct scsi_cmnd;
struct aac_dev;
struct fib;
struct pci_dev;
struct sgmap64;
struct sgmap;
struct user_aac_srb;
struct aac_srb_unit;
struct request;
struct bsg_job;
struct aac_srb_reply;

#if defined(AAC_SAS_TRANSPORT)
struct aac_compat_bsg_job {
#if defined(AAC_SAS_SMP_BSG_JOB)
	struct bsg_job *bsg_job;
	u32  reslen;
#else
	struct request *smp_req;

#endif
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0))
#define bio_multiple_segments(bio)		(bio_segments(bio) > 1)
#endif

unsigned int aac_compat_bsg_job_req_bytes(struct aac_compat_bsg_job *smp_job);
unsigned int aac_compat_bsg_job_resp_bytes(struct aac_compat_bsg_job *smp_job);
void aac_compat_bsg_job_req_data(struct aac_compat_bsg_job *smp_job, void **buf);
void aac_compat_bsg_job_resp_data(struct aac_compat_bsg_job *smp_job, void **buf);
int aac_compat_bsg_job_response_space(struct aac_dev *dev,
					struct aac_compat_bsg_job *smp_job);
int aac_compat_bsg_job_multiple_segments(struct aac_dev *dev,
					struct aac_compat_bsg_job *smp_job);
void aac_compat_build_bsg_job_reply(struct aac_srb_reply *smp_reply,
					struct aac_compat_bsg_job *smp_job);
void aac_compat_build_bsg_smp_request(struct aac_smp_request *smp_request,
					struct aac_compat_bsg_job *smp_job);
void aac_compat_build_bsg_smp_response(struct aac_smp_response *smp_response,
					struct aac_compat_bsg_job *smp_job);
#endif

/********** Use below section for adding debug routines & macros ***********/

void print_raw_buffer(struct aac_dev *aac, u8 *buf, u32 buf_size, char *name);

void print_sg_info64(struct aac_dev *aac, struct sgmap64 *psg);
void print_sg_info32(struct aac_dev *aac, struct sgmap *psg);
void dump_srb(struct aac_dev *aac, void *buf, u32 buf_size);
void dump_srb_reply(struct aac_dev *aac, void *buf, u32 buf_size);

#if defined(AAC_SAS_TRANSPORT)
void ioctl_dump_srb_csmi(struct aac_dev *aac, struct user_aac_srb *usrb,
				const char *name, void *buf, u32 buf_size);
void ioctl_dump_srb_csmi_reply(struct aac_dev *aac, struct user_aac_srb *usrb,
				const char *name, void *buf, u32 buf_size);


void dump_smp_srb_requst(struct aac_dev *aac, struct aac_srb_unit *srbu,
						struct aac_csmi_smp_cmd *smp_cmd,
						struct aac_compat_bsg_job *smp_job);
#endif

#if defined(AAC_DEBUG_INSTRUMENT_RESET)

void dump_pending_fibs(struct aac_dev *aac, struct scsi_cmnd *cmd);
int dump_command_queue(struct scsi_cmnd* cmd);

#define DBG_OVERFLOW_CHK(dev, vno) \
{\
	if(atomic_read(&dev->rrq_outstanding[vno]) > dev->vector_cap)\
		adbg_reset(dev, KERN_ERR, \
		"%s:%d, Host RRQ overloaded vec: %u, Outstanding IOs: %u\n",\
		__FUNCTION__,__LINE__, \
		vno,\
		atomic_read(&dev->rrq_outstanding[vno])); \
}
#define DBG_SET_STATE(FIB, S)	atomic_set(&FIB->state, S);
#define adbg_dump_pending_fibs(AAC, CMD) dump_pending_fibs(AAC, CMD)
#define adbg_dump_command_queue(CMD) dump_command_queue(CMD)
#else
#define DBG_SET_STATE(FIB, S)
#define DBG_OVERFLOW_CHK(dev, vno)
#define adbg_dump_pending_fibs(AAC, CMD)
#define adbg_dump_command_queue(CMD)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SMP)
#define adbg_smp_dump_ioctl_srb_reply(A, S, N, B, BS) ioctl_dump_srb_csmi_reply(A, S, N, B, BS)
#define adbg_smp_dump_ioctl_srb_csmi(A, S, N, B, BS) ioctl_dump_srb_csmi(A, S, N, B, BS)
#define adbg_smp_dump_smp_srb_request(A, SRBU, CSMI, SR) dump_smp_srb_requst(A, SRBU, CSMI, SR)
#else
#define adbg_smp_dump_ioctl_srb_reply(A, S, N, B, BS)
#define adbg_smp_dump_ioctl_srb_csmi(A, S, N, B, BS)
#define adbg_smp_dump_smp_srb_request(A, SRBU, CSMI, SR)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SRB)
#define adbg_srb_dump_srb_unit(AAC, BUF, SIZE) dump_srb_unit(AAC, BUF, SIZE)
#else
#define adbg_srb_dump_srb_unit(AAC, REQ, H)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_IOCTL)
#define adbg_ioctl_print_raw_srb(AAC, BUF, SIZE) dump_srb(AAC, BUF, SIZE)
#define adbg_ioctl_print_raw_fib(AAC, BUF, SIZE) print_raw_buffer(AAC, BUF, SIZE, "FIB= ")
#else
#define adbg_ioctl_print_raw_srb(AAC, BUF, SIZE)
#define adbg_ioctl_print_raw_fib(AAC, BUF, SIZE)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_2TB)
#define adbg_2tb_print_cdb(AAC, BUF, SIZE) print_raw_buffer(AAC, BUF, SIZE, "cdb=")
#define adbg_2tb_print_cap(AAC, BUF, SIZE) print_raw_buffer(AAC, BUF, SIZE, "cap=")
#else
#define adbg_2tb_print_cdb(AAC, BUF, SIZE)
#define adbg_2tb_print_cap(AAC, BUF, SIZE)
#endif

#if defined(AAC_DEBUG_INSTRUMENT_SG)
#define adbg_print_sg_info64(AAC, PSG) print_sg_info64(AAC, PSG)
#define adbg_print_sg_info32(AAC, PSG) print_sg_info32(AAC, PSG)
#else
#define adbg_print_sg_info64(AAC, PSG)
#define adbg_print_sg_info32(AAC, PSG)
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_AAC_CONFIG))

void debug_aac_config(struct scsi_cmnd* scsicmd, __le32 count, unsigned long byte_count);

#define adbg_debug_aac_config(CMD,COUNT,BYTE_COUNT) debug_aac_config(CMD,COUNT,BYTE_COUNT)
#else
#define adbg_debug_aac_config(CMD,COUNT,BYTE_COUNT)
#endif

#endif


