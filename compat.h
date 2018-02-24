/*
 *	Adaptec AAC series RAID controller driver
 *	(c) Copyright 2001 Red Hat Inc.	<alan@redhat.com>
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
 *   compat.h
 *
 *
 * Abstract: This file is for backwards compatibility with older kernel versions
 */

#ifndef _COMPAT_H_
#define _COMPAT_H_

#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
#include <linux/aer.h>
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)) */

#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>

#include "adbg.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15))
#ifndef sdev_channel
#define sdev_channel(x)	(x)->channel
#endif
#ifndef scmd_channel
#define scmd_channel(x)	sdev_channel((x)->device)
#endif
#ifndef sdev_id
#define sdev_id(x)	(x)->id
#endif
#ifndef scmd_id
#define scmd_id(x)	sdev_id((x)->device)
#endif

#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
#ifndef scsi_sglist
#define scsi_sglist(cmd) ((struct scatterlist *)((cmd)->request_buffer))
#endif
#ifndef scsi_bufflen
#define scsi_bufflen(cmd) ((cmd)->request_bufflen)
#endif
#ifndef scsi_sg_count
#define scsi_sg_count(cmd) ((cmd)->use_sg)
#endif
#ifndef SCSI_HAS_DMA_MAP
#define scsi_dma_unmap(cmd) if(scsi_sg_count(cmd))pci_unmap_sg(((struct aac_dev *)cmd->device->host->hostdata)->pdev,scsi_sglist(cmd),scsi_sg_count(cmd),cmd->sc_data_direction)
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
#define scsi_dma_map(cmd) scsi_sg_count(cmd)
#else
#define scsi_dma_map(cmd) ((scsi_sg_count(cmd))?pci_map_sg(((struct aac_dev *)cmd->device->host->hostdata)->pdev,scsi_sglist(cmd),scsi_sg_count(cmd),cmd->sc_data_direction):0)
#endif
#endif
#if (!defined(__VMKLNX__))
#define sg_next(sg) ((sg)+1)
#endif
#ifndef scsi_resid
#define scsi_resid(cmd) ((cmd)->resid)
#define scsi_set_resid(cmd,res) (cmd)->resid = res
#endif
#ifndef scsi_for_each_sg
#define scsi_for_each_sg(cmd, sg, nseg, __i) \
	for (__i = 0, sg = scsi_sglist(cmd); __i < (nseg); __i++, sg = sg_next(sg))
#endif

#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
# define uintptr_t ptrdiff_t
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)) && !defined(SCSI_HAS_SSLEEP))
#undef ssleep
#define ssleep(x) scsi_sleep((x)*HZ)
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(HAS_MSLEEP))
#define msleep(x) set_current_state(TASK_UNINTERRUPTIBLE); schedule_timeout(x)
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,19))
#ifndef __builtin_expect
#define __builtin_expect(x,expected_value) (x)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x),0)
#endif
#ifndef likely
#define likely(x) __builtin_expect(!!(x),1)
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#ifndef BUG_ON
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while (0)
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10))
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef min_t
#define min_t(a,b,c) (((a)(b)<(a)(c))?(a)(b):(a)(c))
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0))
 typedef unsigned long dma_addr_t;
#include <linux/kcomp.h>
#define PCI_ANY_ID (~0)
#define SCSI_DATA_UNKNOWN	0
#define SCSI_DATA_WRITE		1
#define SCSI_DATA_READ		2
#define SCSI_DATA_NONE		3
 /* Sigh ... a *lot* more needs to be done for this Grandpa */

#endif

#if ( ((defined(RHEL_MAJOR) && RHEL_MAJOR == 7 && RHEL_MINOR >= 2)) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)))
#define SERVICE_ACTION_IN SERVICE_ACTION_IN_16
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0))
#define SCSI_ADDR_FORMAT    "%d,%d,%d,%llu"
#else
#define SCSI_ADDR_FORMAT    "%d,%d,%d,%d"
#endif

/*****************************************************************************/
/*
 * SATD-935: Linux: Backport timespec64 patch to OOB driver
 *
 * Description : 32-bit architectures as it cannot go past year 2038
 * 	Using timespec64 behave properly on 32-bit and 64-bit architectures, 
 * 	and avoids relying on signed integer overflow to pass times into the second interface.
 *
 * Versions : time64_t, timespec64 and ktime_get_real_ts64 are introduced in
 * 	kernel version 3.17 onwards but RHEL have backported these changes
 * 	to RHEL 7.2 onwards and also backported these changes to RHEL 6.8, 6.9 and 6.10 kernels.
 *
 * To-do - time64_to_tm is the addition in kernel 4.8 and we may have to add the RHEL 
 * 	conditional when REDHAT backports the same.
 */

#if (!defined(RHEL_MAJOR))
#define NON_RHEL
#endif

#if (defined(RHEL_MAJOR) && ((RHEL_MAJOR < 7) || (RHEL_MAJOR == 7 && RHEL_MINOR < 2)))
#define RHEL_LESS_THAN_7_2
#endif

#if (defined(RHEL_MAJOR) && ((RHEL_MAJOR == 6) && (RHEL_MINOR >= 8 && RHEL_MINOR <= 10)))
#define RHEL_TIMESPEC64_DEFINED
#endif

#if ((defined(NON_RHEL) && (LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0))) || (defined(RHEL_LESS_THAN_7_2) && !defined(RHEL_TIMESPEC64_DEFINED)))
#define timespec64_undefined
#define time64_t unsigned long
#define timespec64 timeval
#define ktime_get_real_ts64 do_gettimeofday
#elif (defined(RHEL_TIMESPEC64_DEFINED))
#define ktime_get_real_ts64 getnstimeofday
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,1))
#define ktime_get_real_seconds get_seconds
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0))
#define time64_to_tm time_to_tm
#endif

/*****************************************************************************/

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
#include <linux/time.h>
static inline unsigned long get_seconds(void)
{
	struct timeval now;
	do_gettimeofday(&now);
	return now.tv_sec;
}
#define scsi_host_template SHT
#define DMA_BIDIRECTIONAL	SCSI_DATA_UNKNOWN
#define DMA_TO_DEVICE		SCSI_DATA_WRITE
#define DMA_FROM_DEVICE		SCSI_DATA_READ
#define DMA_NONE		SCSI_DATA_NONE
#if (defined(__ESXi4__))
#define imajor(x) MAJOR(x->i_rdev) //ESXi4 support
#else
#define iminor(x) MINOR(x->i_rdev)
#endif
#define scsi_host_alloc(t,s) scsi_register(t,s)
#if (defined(CONFIG_VMNIX) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)))
#define scsi_host_put(s)
#else
#define scsi_host_put(s) scsi_unregister(s)
#endif
#ifndef pci_set_consistent_dma_mask
#define pci_set_consistent_dma_mask(d,m) 0
#endif
#ifndef SCSI_HAS_SCSI_SCAN_HOST
#ifndef scsi_scan_host
#define scsi_scan_host(s)
#endif
#endif
#define scsi_add_host(s,d) 0
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20)) && !defined(list_for_each_entry))
#if (!defined(_LINUX_PREFETCH_H))
static inline void prefetch(const void *x) {;}
#endif
#define list_for_each_entry(pos, head, member)                          \
        for (pos = list_entry((head)->next, typeof(*pos), member),      \
                     prefetch(pos->member.next);                        \
             &pos->member != (head);                                    \
             pos = list_entry(pos->member.next, typeof(*pos), member),  \
                     prefetch(pos->member.next))
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
#define scsi_remove_host(s)
#else
#define scsi_remove_host(s)					\
	struct proc_dir_entry * entry = NULL;			\
	struct scsi_device *device;				\
	extern struct proc_dir_entry * proc_scsi;		\
	if (proc_scsi != (struct proc_dir_entry *)NULL)		\
	for (entry = proc_scsi->subdir;				\
	  entry != (struct proc_dir_entry *)NULL &&		\
	  (!entry->low_ino ||					\
	    (entry->namelen != 4) ||				\
	    memcmp ("scsi", entry->name, 4));			\
	  entry = entry->next);					\
	if (entry && entry->write_proc)				\
	for (device = s->host_queue;				\
	  device != (struct scsi_device *)NULL;			\
	  device = device->next)				\
		if (!device->access_count && !s->in_recovery) {	\
			char buffer[80];			\
			int length;				\
			mm_segment_t fs;			\
			device->removable = 1;			\
			sprintf (buffer, "scsi "		\
			  "remove-single-device %d %d %d %d\n", \
			  s->host_no, device->channel,		\
			  device->id, device->lun);		\
			length = strlen (buffer);		\
			fs = get_fs();				\
			set_fs(get_ds());			\
			length = entry->write_proc(		\
			  NULL, buffer, length, NULL);		\
			set_fs(fs);				\
		}
#endif
#if (!defined(__devexit_p))
# if (defined(MODULE))
#  define __devexit_p(x) x
# else
#  define __devexit_p(x) NULL
# endif
#endif
#define __user
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6)) && (!defined(SCSI_HAS_SCSI_DEVICE_ONLINE)))
#define scsi_device_online(d) ((d)->online)
#ifndef SDEV_RUNNING
#define SDEV_RUNNING 1
#endif
#ifndef SDEV_OFFLINE
#define SDEV_OFFLINE 0
#endif
#define scsi_device_set_state(d,s) ((d)->online=(s!=SDEV_OFFLINE))
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#define __iomem
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)) && !defined(HAS_BITWISE_TYPE))
typedef u64 __le64;
typedef u32 __le32;
typedef u16 __le16;

#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
#include <linux/dma-mapping.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5))
#ifndef DMA_64BIT_MASK
#define DMA_64BIT_MASK ((dma_addr_t)0xffffffffffffffffULL)
#endif
#ifndef DMA_32BIT_MASK
#define DMA_32BIT_MASK ((dma_addr_t)0xffffffffULL)
#endif
#endif
#ifndef DMA_31BIT_MASK
#define DMA_31BIT_MASK ((dma_addr_t)0x7fffffffULL)
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))
#define pci_channel_offline(x) 0
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
#define __devinit
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
#ifndef spin_trylock_irqsave
#define spin_trylock_irqsave(lock, flags) \
({ \
	local_irq_save(flags); \
	spin_trylock(lock) ? \
	1 : ({local_irq_restore(flags); 0 ;}); \
})
#endif

#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,1)) && defined(SCSI_HAS_MY_DEVICES))
#define shost_for_each_device(sdev, shost) \
	for ((sdev) = __scsi_iterate_devices((shost), NULL); \
	     (sdev); \
	     (sdev) = __scsi_iterate_devices((shost), (sdev)))
#define __shost_for_each_device(sdev, shost) \
	list_for_each_entry((sdev), &((shost)->my_devices), siblings)
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
static inline struct scsi_device *__scsi_iterate_devices(struct Scsi_Host *shost,
			                   struct scsi_device *prev)
{
	struct list_head *list = (prev ? &prev->siblings : &shost->my_devices);
	struct scsi_device *next = NULL;
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);
	while (list->next != &shost->my_devices) {
		next = list_entry(list->next, struct scsi_device, siblings);
		/* skip devices that we can't get a reference to */
		if (!scsi_device_get(next))
			break;
		list = list->next;
	}
	spin_unlock_irqrestore(shost->host_lock, flags);

	if (prev)
		scsi_device_put(prev);
	return next;
}

#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,18)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,9)) && (LINUX_VERSION_CODE != KERNEL_VERSION(2,4,13))
# define dma_handle ptr

#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,11))
#include <linux/blk.h>

static inline unsigned int block_size(kdev_t dev)
{
	int retval = BLOCK_SIZE;
	int major = MAJOR(dev);

	if (blksize_size[major]) {
		int minor = MINOR(dev);
		if (blksize_size[major][minor])
			retval = blksize_size[major][minor];
	}
	return retval;
}

#endif

/*
 * ESXI4 does not support list_is_singular
 */

#if (defined(RHEL_MAJOR) && RHEL_MAJOR == 5 && RHEL_MINOR <= 2)
#define RHEL5_2
#endif

#if (defined(__ONLY_ESXi4__)) || defined(RHEL5_2) || (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18) && !(defined(__VMKLNX__)))
 static inline int list_is_singular(const struct list_head *head)
{
         return !list_empty(head) && (head->next == head->prev);
}
#endif

/*
 * Vmware does not suppot get_unaligned_be64
 */

#if (defined(__VMKLNX__)) || (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,18))
static inline u32 __get_unaligned_be32(const u8 *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];

}

static inline u32 get_unaligned_be32(const void *p)
{
        return __get_unaligned_be32((const u8 *)p);
}

static inline u64 __get_unaligned_be64(const u8 *p)
{
         return (u64)__get_unaligned_be32(p) << 32 |
                __get_unaligned_be32(p + 4);
}

static inline u64 get_unaligned_be64(const void *p)
{
         return __get_unaligned_be64((const u8 *)p);
}

#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,4))
# define pci_disable_device(x)
#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,7))

#ifndef COMPLETION_INITIALIZER

#include <linux/wait.h>

struct completion {
	unsigned int done;
	wait_queue_head_t wait;
};
#define COMPLETION_INITIALIZER(work) \
	{ 0, __WAIT_QUEUE_HEAD_INITIALIZER((work).wait) }

#define DECLARE_COMPLETION(work) \
	struct completion work = COMPLETION_INITIALIZER(work)
#define INIT_COMPLETION(x)	((x).done = 0)

static inline void init_completion(struct completion *x)
{
	x->done = 0;
	init_waitqueue_head(&x->wait);
}
#endif

#ifndef complete_and_exit
static inline void complete_and_exit(struct completion *comp, long code)
{
	/*
	if (comp)
		complete(comp);

	do_exit(code);
	*/
}
#endif

#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)) && !defined(sdev_printk))
#define sdev_printk(prefix, sdev, fmt, a...) \
	printk(prefix " %d:%d:%d:%d: " fmt, sdev->host->host_no, \
		sdev_channel(sdev), sdev_id(sdev), sdev->lun, ##a)
#endif
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,2))
#include <linux/pci.h>

//static inline void scsi_set_pci_device(struct Scsi_Host *SHpnt,
//                                       struct pci_dev *pdev)
//{
///*	SHpnt->pci_dev = pdev;	*/
//}
#define scsi_set_pci_device(SHpnt,pdev)

static inline void wait_for_completion(struct completion *x)
{
	spin_lock_irq(&x->wait.lock);
	if (!x->done) {
		DECLARE_WAITQUEUE(wait, current);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
		wait.flags |= WQ_FLAG_EXCLUSIVE;
#endif
		__add_wait_queue_tail(&x->wait, &wait);
		do {
			__set_current_state(TASK_UNINTERRUPTIBLE);
			spin_unlock_irq(&x->wait.lock);
			schedule();
			spin_lock_irq(&x->wait.lock);
		} while (!x->done);
		__remove_wait_queue(&x->wait, &wait);
	}
	x->done--;
	spin_unlock_irq(&x->wait.lock);
}

static inline int pci_set_dma_mask(struct pci_dev *dev, dma_addr_t mask)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0))
    dev->dma_mask = mask;
#endif

    return 0;
}

#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18))

#ifndef IRQF_SHARED
# define IRQF_SHARED SA_SHIRQ
#endif
#ifndef IRQF_DISABLED
# define IRQF_DISABLED SA_INTERRUPT /* Counter intuitive? */
#endif
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30))
static inline struct delayed_work *to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
#define kmalloc_array(num,size,flags) kzalloc(size * num, flags)
#endif

static inline void aac_scsi_set_device(struct Scsi_Host *shost,
	struct device *device)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
	scsi_set_device(shost, device);
#else
	UNUSED(shost);
	UNUSED(device);
#endif
}

static inline int aac_pci_set_dma_max_seg_size(struct pci_dev *pdev,
	unsigned int size)
{
#if defined(PCI_HAS_SET_DMA_MAX_SEG_SIZE)
	return pci_set_dma_max_seg_size(pdev, size);
#elif (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	UNUSED(pdev);
	UNUSED(size);
	return 0;
#else
	return dma_set_max_seg_size(&pdev->dev, size);
#endif
}

static inline int aac_scsi_init_shared_tag_map(struct Scsi_Host *shost,
	int depth)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24) && LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0))
	return scsi_init_shared_tag_map(shost, depth);
#else
	UNUSED(shost);
	UNUSED(depth);
	return 0;
#endif
}

static inline void aac_pci_enable_pcie_error_reporting_and_save_state(
	struct pci_dev *pdev)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
	pci_enable_pcie_error_reporting(pdev);
	pci_save_state(pdev);
#else
	UNUSED(pdev);
#endif
}

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
static inline unsigned char _bin2bcd(unsigned val)
{
	return ((val / 10) << 4) + val % 10;
}

#define const_bin2bcd(x)	((((x) / 10) << 4) + (x) % 10)
#define bin2bcd(x)                                      \
		(__builtin_constant_p((u8 )(x)) ?       \
		const_bin2bcd(x) :                      \
		_bin2bcd(x))
#endif

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))

static void inline aac_init_completion(struct semaphore * wait_completion)
{
        sema_init(wait_completion, 0);
}

static int inline aac_wait_for_completion_interruptible(struct semaphore * wait_completion)
{
        return down_interruptible(wait_completion);
}

static void inline aac_complete(struct semaphore *wait_completion)
{
        up(wait_completion);
}

static bool inline aac_try_wait_for_completion(struct semaphore *wait_completion)
{
        return down_trylock(wait_completion);
}

static inline void *aac_kmemdup(const void *src, size_t len, gfp_t gfp)
{
        char *cname = kmalloc(len, GFP_ATOMIC);

        if (cname)
                memcpy(cname, src, len);

        return cname;

}
#else
static inline void aac_init_completion(struct completion * wait_completion)
{
        init_completion(wait_completion);
}

static inline int aac_wait_for_completion_interruptible(struct completion * wait_completion)
{
        return wait_for_completion_interruptible(wait_completion);
}

static inline void aac_complete(struct completion *wait_completion)
{
        complete(wait_completion);
}

static inline bool aac_try_wait_for_completion(struct completion *wait_completion)
{
        return !(try_wait_for_completion(wait_completion));
}

static inline void *aac_kmemdup(const void *src, size_t len, gfp_t gfp)
{
        return kmemdup(src, len, gfp);
}
#endif
#endif
