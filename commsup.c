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
 *  commsup.c
 *
 * Abstract: Contain all routines that are required for FSA host/adapter
 *    communication.
 *
 */

#include <linux/version.h>	/* Needed for Comparing version code */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/sched.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
#include <linux/sched/signal.h>
#endif
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/blkdev.h>
#include <linux/delay.h>
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,5)) || defined(HAS_KTHREAD))
#include <linux/kthread.h>
#endif
#include <linux/interrupt.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
#include <linux/bcd.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include "scsi.h"
#include "hosts.h"
#if (!defined(SCSI_HAS_HOST_LOCK))
#include <linux/blk.h>	/* for io_request_lock definition */
#endif
#else
#if (((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE))) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__)
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>
#endif
#include <scsi/scsi.h>
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,1)) && !defined(DID_OK))
#define DID_OK 0x00
#endif
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9))
#include <scsi/scsi_eh.h>
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) && defined(MODULE))
#include <scsi/scsi_driver.h>
#endif
#endif
#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
#include <scsi/scsi_transport_sas.h>
#endif

#include "aacraid.h"
#include "fwdebug.h"
#include "compat.h"

extern int aac_removable;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32))
struct tm {
	/*
	 * the number of seconds after the minute, normally in the range
	 * 0 to 59, but can be up to 60 to allow for leap seconds
	 */
	int tm_sec;
	/* the number of minutes after the hour, in the range 0 to 59*/
	int tm_min;
	/* the number of hours past midnight, in the range 0 to 23 */
	int tm_hour;
	/* the day of the month, in the range 1 to 31 */
	int tm_mday;
	/* the number of months since January, in the range 0 to 11 */
	int tm_mon;
	/* the number of years since 1900 */
	long tm_year;
	/* the number of days since Sunday, in the range 0 to 6 */
	int tm_wday;
	/* the number of days since January 1, in the range 0 to 365 */
	int tm_yday;
};
/*
 * Nonzero if YEAR is a leap year (every 4 years,
 * except every 100th isn't, and every 400th is).
 */
static int __isleap(long year)
{
	return (year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0);
}

/* do a mathdiv for long type */
static long math_div(long a, long b)
{
	return a / b - (a % b < 0);
}

/* How many leap years between y1 and y2, y1 must less or equal to y2 */
static long leaps_between(long y1, long y2)
{
	long leaps1 = math_div(y1 - 1, 4) - math_div(y1 - 1, 100)
		+ math_div(y1 - 1, 400);
	long leaps2 = math_div(y2 - 1, 4) - math_div(y2 - 1, 100)
		+ math_div(y2 - 1, 400);
	return leaps2 - leaps1;
}

/* How many days come before each month (0-12). */
static const unsigned short __mon_yday[2][13] = {
	/* Normal years. */
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
	/* Leap years. */
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

#define SECS_PER_HOUR	(60 * 60)
#define SECS_PER_DAY	(SECS_PER_HOUR * 24)

/**
 * time_to_tm - converts the calendar time to local broken-down time
 *
 * @totalsecs	the number of seconds elapsed since 00:00:00 on January 1, 1970,
 *		Coordinated Universal Time (UTC).
 * @offset	offset seconds adding to totalsecs.
 * @result	pointer to struct tm variable to receive broken-down time
 */
void time_to_tm(time_t totalsecs, int offset, struct tm *result)
{
	long days, rem, y;
	const unsigned short *ip;

	days = totalsecs / SECS_PER_DAY;
	rem = totalsecs % SECS_PER_DAY;
	rem += offset;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}

	result->tm_hour = rem / SECS_PER_HOUR;
	rem %= SECS_PER_HOUR;
	result->tm_min = rem / 60;
	result->tm_sec = rem % 60;

	/* January 1, 1970 was a Thursday. */
	result->tm_wday = (4 + days) % 7;
	if (result->tm_wday < 0)
		result->tm_wday += 7;

	y = 1970;

	while (days < 0 || days >= (__isleap(y) ? 366 : 365)) {
		/* Guess a corrected year, assuming 365 days per year. */
		long yg = y + math_div(days, 365);

		/* Adjust DAYS and Y to match the guessed year. */
		days -= (yg - y) * 365 + leaps_between(y, yg);
		y = yg;
	}

	result->tm_year = y - 1900;

	result->tm_yday = days;

	ip = __mon_yday[__isleap(y)];
	for (y = 11; days < ip[y]; y--)
		continue;
	days -= ip[y];

	result->tm_mon = y;
	result->tm_mday = days + 1;
}
#endif

/**
 *	fib_map_alloc		-	allocate the fib objects
 *	@dev: Adapter to allocate for
 *
 *	Allocate and map the shared PCI space for the FIB blocks used to
 *	talk to the Adaptec firmware.
 */

static int fib_map_alloc(struct aac_dev *dev)
{
	if (AAC_MAX_NATIVE_SIZE > dev->max_fib_size)
		dev->max_cmd_size = AAC_MAX_NATIVE_SIZE;
	else
		dev->max_cmd_size = dev->max_fib_size;

	dprintk((KERN_INFO
	  "allocate hardware fibs aac_pci_alloc_consistent(%p, %d * (%d + %d), %p)\n",
	  dev->pdev, dev->max_cmd_size, dev->scsi_host_ptr->can_queue,
	  AAC_NUM_MGT_FIB, &dev->hw_fib_pa));

	if((dev->hw_fib_va = aac_pci_alloc_consistent(dev->pdev, 
		(dev->max_cmd_size + sizeof(struct aac_fib_xporthdr))
		* (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB) + 31,
		&dev->hw_fib_pa)) == NULL) {
		aac_err(dev, "aac_pci_alloc_consistent failed\n");
		return -ENOMEM;
	}
	return 0;
}

/**
 *	aac_fib_map_free		-	free the fib objects
 *	@dev: Adapter to free
 *
 *	Free the PCI mappings and the memory allocated for FIB blocks
 *	on this adapter.
 */

void aac_fib_map_free(struct aac_dev *dev)
{
	size_t alloc_size;
	size_t fib_size;
	int num_fibs;

	if(!dev->hw_fib_va || !dev->max_cmd_size)
		return;

	num_fibs = dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB;
	fib_size = dev->max_fib_size + sizeof(struct aac_fib_xporthdr);
	alloc_size = fib_size * num_fibs + ALIGN32 - 1;

	pci_free_consistent(dev->pdev, alloc_size, dev->hw_fib_va,
							dev->hw_fib_pa);
	dev->hw_fib_va = NULL;
	dev->hw_fib_pa = 0;
}

void aac_fib_vector_assign(struct aac_dev *dev)
{
	u32 i = 0;
	u32 vector = 1;
	struct fib *fibptr = NULL;
	u32 fc = dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB;

	for (i = 0, fibptr = &dev->fibs[i]; i < fc; i++, fibptr++) {
		if (dev->max_msix ==  1 || (i > ((fc - 1) - dev->vector_cap)))
			fibptr->vector_no = 0;
		else {
			fibptr->vector_no = vector;
			vector++;
			if (vector == dev->max_msix)
				vector  = 1;
		}
	}
}

/**
 *	aac_fib_setup	-	setup the fibs
 *	@dev: Adapter to set up
 *
 *	Allocate the PCI space for the fibs, map it and then intialise the
 *	fib area, the unmapped fib data and also the free list
 */

int aac_fib_setup(struct aac_dev * dev)
{
	struct fib *fibptr;
	struct hw_fib *hw_fib;
	dma_addr_t hw_fib_pa;
	int i;
	u32 max_cmds;

	while (((i = fib_map_alloc(dev)) == -ENOMEM)
	 && (dev->scsi_host_ptr->can_queue > (64 - AAC_NUM_MGT_FIB))) {
		max_cmds = (dev->scsi_host_ptr->can_queue+AAC_NUM_MGT_FIB) >> 1;
		dev->scsi_host_ptr->can_queue = max_cmds - AAC_NUM_MGT_FIB;
		if (dev->comm_interface != AAC_COMM_MESSAGE_TYPE3)
			dev->init->r7.MaxIoCommands = cpu_to_le32(max_cmds);
	}
	if (i<0) {
		aac_err(dev, "fib_map_alloc failed-%d", -ENOMEM);
		return -ENOMEM;
	}

	memset(dev->hw_fib_va, 0, 
		(dev->max_cmd_size + sizeof(struct aac_fib_xporthdr)) * 
		(dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB));

    /* 32 byte alignment for PMC */
	hw_fib_pa = (dev->hw_fib_pa + (ALIGN32 - 1)) & ~(ALIGN32 - 1);
	hw_fib    = (struct hw_fib *)((unsigned char *)dev->hw_fib_va +
					(hw_fib_pa - dev->hw_fib_pa));

	/* add Xport header */
	hw_fib = (struct hw_fib *)((unsigned char *)hw_fib +
		sizeof(struct aac_fib_xporthdr));
	hw_fib_pa += sizeof(struct aac_fib_xporthdr);

	/*
	 *	Initialise the fibs
	 */
	for (i = 0, fibptr = &dev->fibs[i];
		i < (dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB);
		i++, fibptr++)
	{
		fibptr->flags = 0;
		fibptr->size = sizeof(struct fib);
		fibptr->dev = dev;
		fibptr->index = i;
		fibptr->hw_fib_va = hw_fib;
		fibptr->data = (void *) fibptr->hw_fib_va->data;
		fibptr->next = fibptr+1;	/* Forward chain the fibs */
		aac_init_completion(&fibptr->event_wait);
		spin_lock_init(&fibptr->event_lock);
		INIT_LIST_HEAD(&fibptr->fiblink);
		hw_fib->header.XferState = cpu_to_le32(0xffffffff);
		hw_fib->header.SenderSize = cpu_to_le16(dev->max_fib_size);
		fibptr->hw_fib_pa = hw_fib_pa;
		fibptr->hw_sgl_pa = hw_fib_pa +
			offsetof(struct aac_hba_cmd_req, sge[2]);
		/* one element is for the ptr to the separate sg list,
		   second element for 32 byte alignment */
		fibptr->hw_error_pa = hw_fib_pa +
			offsetof(struct aac_native_hba, resp.resp_bytes[0]);
		hw_fib = (struct hw_fib *)((unsigned char *)hw_fib +
			dev->max_cmd_size + sizeof(struct aac_fib_xporthdr));
		hw_fib_pa = hw_fib_pa +
			dev->max_cmd_size + sizeof(struct aac_fib_xporthdr);
		DBG_SET_STATE(fibptr, DBG_STATE_FREE);
	}

	/*
	 * Assign vector numbers to fib
	 */
	aac_fib_vector_assign(dev);

	/*
	 *	Add the fib chain to the free list
	 */
	dev->fibs[dev->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB - 1].next = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	/* Reserving the last 8 as management fibs */
	dev->free_fib = &dev->fibs[dev->scsi_host_ptr->can_queue];
#else
	/*
	 *	Enable this to debug out of queue space
	 */
	dev->free_fib = &dev->fibs[0];
#endif
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
static inline struct fib *aac_fib_alloc_io(struct aac_dev *dev,
	struct scsi_cmnd *scmd)
{
	return &dev->fibs[scmd->request->tag];
}
#endif

static struct fib *aac_fib_alloc_mgt(struct aac_dev *dev)
{
	struct fib * fibptr;
	unsigned long flags;

	spin_lock_irqsave(&dev->fib_lock, flags);
	fibptr = dev->free_fib;
	if (!fibptr) {
		spin_unlock_irqrestore(&dev->fib_lock, flags);
		return fibptr;
	}
	dev->free_fib = fibptr->next;
	spin_unlock_irqrestore(&dev->fib_lock, flags);

	fibptr->flags = FIB_CONTEXT_POOL;

	return fibptr;
}


/**
 *	aac_fib_alloc: allocate a fib
 *	@dev: Adapter to allocate the fib for
 *	@scmd: Scsicmd to allocate a fib for
 *
 *	Allocate a fib from the io pool if the scmd arguemnt is not NULL.
 *	otherwise allocate  a mgt fib. If no mgt fib then return NULL.
 **/
struct fib *aac_fib_alloc(struct aac_dev *dev, struct scsi_cmnd *scmd)
{
	struct fib *fibptr;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	if (likely(scmd)) {

		fibptr = aac_fib_alloc_io(dev, scmd);
		fibptr->flags = 0;
	}
	else
#endif
	{
		fibptr = aac_fib_alloc_mgt(dev);
		if (!fibptr)
			return fibptr;
	}

	/*
	 *	Set the proper node type code and node byte size
	 */
	fibptr->type = FSAFS_NTC_FIB_CONTEXT;
	/*
	 *	Null out fields that depend on being zero at the start of
	 *	each I/O
	 */
	fibptr->hw_fib_va->header.XferState = 0;
	fibptr->callback = NULL;
	fibptr->callback_data = NULL;
#if defined(FIB_COMPLETION_TIMING)
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4,19,84))
	{
		struct timeval now;
		do_gettimeofday(&now);
		fibptr->DriverTimeStartS = now.tv_sec;
		fibptr->DriverTimeStartuS = now.tv_usec;
	}
#else
	{
		struct timespec64 now;
		ktime_get_ts64(&now);
		fibptr->DriverTimeStartS = now.tv_sec;
		fibptr->DriverTimeStartuS = now.tv_nsec / NSEC_PER_USEC;
	}
#endif
	fibptr->DriverTimeDoneS = 0;
	fibptr->DriverTimeDoneuS = 0;
#endif
	DBG_SET_STATE(fibptr, DBG_STATE_ALLOCATED_AIF);

	return fibptr;
}

static inline int aac_is_mgt_fib(struct fib *fib)
{
	return fib->flags & FIB_CONTEXT_POOL;
}

static void aac_fib_free_mgt(struct fib *fibptr)
{
	unsigned long flags;

	/* fib with status wait timeout? */
	if (fibptr->done == 2) {
		return;
	}

	spin_lock_irqsave(&fibptr->dev->fib_lock, flags);
	if (!(fibptr->flags & FIB_CONTEXT_FLAG_NATIVE_HBA) &&
		fibptr->hw_fib_va->header.XferState != 0) {
		aac_warn(fibptr->dev, "XferState != 0, XferState = 0x%x\n",
			 le32_to_cpu(fibptr->hw_fib_va->header.XferState));
	}
	fibptr->next = fibptr->dev->free_fib;
	fibptr->dev->free_fib = fibptr;
	spin_unlock_irqrestore(&fibptr->dev->fib_lock, flags);
	DBG_SET_STATE(fibptr, DBG_STATE_FREED_AIF);
}

/**
 *	aac_fib_free: free a fib
 *	@fibptr: fib to free up
 *
 *	Frees up a fib if it is a mgt fib and places it on the appropriate
 *	queue, io fibs need not be freed.
 */
void aac_fib_free(struct fib *fibptr)
{
#if defined(FIB_COMPLETION_TIMING)
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4,19,84))
	{
		struct timeval now;
		do_gettimeofday(&now);
		fibptr->DriverTimeDoneS = now.tv_sec;
		fibptr->DriverTimeDoneuS = now.tv_usec;
	}
#else
	{
		struct timespec64 now;
                ktime_get_ts64(&now);
                fibptr->DriverTimeDoneS = now.tv_sec;
                fibptr->DriverTimeDoneuS = now.tv_nsec / NSEC_PER_USEC;
	}
#endif
#endif

	if (unlikely(aac_is_mgt_fib(fibptr)))
		aac_fib_free_mgt(fibptr);

	DBG_SET_STATE(fibptr, DBG_STATE_FREED_IO);
}

/**
 *	aac_fib_init	-	initialise a fib
 *	@fibptr: The fib to initialize
 *
 *	Set up the generic fib fields ready for use
 */

void aac_fib_init(struct fib *fibptr)
{
	struct hw_fib *hw_fib = fibptr->hw_fib_va;

	memset(&hw_fib->header, 0, sizeof(struct aac_fibhdr));
	hw_fib->header.StructType = FIB_MAGIC;
	hw_fib->header.Size = cpu_to_le16(fibptr->dev->max_fib_size);
	hw_fib->header.XferState = cpu_to_le32(HostOwned | FibInitialized | FibEmpty | FastResponseCapable);
	hw_fib->header.u.ReceiverFibAddress = cpu_to_le32(fibptr->hw_fib_pa);
	hw_fib->header.SenderSize = cpu_to_le16(fibptr->dev->max_fib_size);
}

/**
 *	fib_deallocate		-	deallocate a fib
 *	@fibptr: fib to deallocate
 *
 *	Will deallocate and return to the free pool the FIB pointed to by the
 *	caller.
 */

static void fib_dealloc(struct fib * fibptr)
{
	struct hw_fib *hw_fib = fibptr->hw_fib_va;
	hw_fib->header.XferState = 0;
}

/*
 *	Commuication primitives define and support the queuing method we use to
 *	support host to adapter commuication. All queue accesses happen through
 *	these routines and are the only routines which have a knowledge of the
 *	 how these queues are implemented.
 */

/**
 *	aac_get_entry		-	get a queue entry
 *	@dev: Adapter
 *	@qid: Queue Number
 *	@entry: Entry return
 *	@index: Index return
 *	@nonotify: notification control
 *
 *	With a priority the routine returns a queue entry if the queue has free entries. If the queue
 *	is full(no free entries) than no entry is returned and the function returns 0 otherwise 1 is
 *	returned.
 */

static int aac_get_entry (struct aac_dev * dev, u32 qid, struct aac_entry **entry, u32 * index, unsigned long *nonotify)
{
	struct aac_queue * q;
	unsigned long idx;

	/*
	 *	All of the queues wrap when they reach the end, so we check
	 *	to see if they have reached the end and if they have we just
	 *	set the index back to zero. This is a wrap. You could or off
	 *	the high bits in all updates but this is a bit faster I think.
	 */

	q = &dev->queues->queue[qid];

	idx = *index = le32_to_cpu(*(q->headers.producer));
	/* Interrupt Moderation, only interrupt for first two entries */
	if (idx != le32_to_cpu(*(q->headers.consumer))) {
		if (--idx == 0) {
			if (qid == AdapNormCmdQueue)
				idx = ADAP_NORM_CMD_ENTRIES;
			else
				idx = ADAP_NORM_RESP_ENTRIES;
		}
		if (idx != le32_to_cpu(*(q->headers.consumer)))
			*nonotify = 1;
	}

	if (qid == AdapNormCmdQueue) {
		if (*index >= ADAP_NORM_CMD_ENTRIES)
			*index = 0; /* Wrap to front of the Producer Queue. */
	} else {
		if (*index >= ADAP_NORM_RESP_ENTRIES)
			*index = 0; /* Wrap to front of the Producer Queue. */
	}

	/* Queue is full */
	if ((*index + 1) == le32_to_cpu(*(q->headers.consumer))) {
		printk(KERN_WARNING "Queue %d full, %u outstanding.\n",
				qid, atomic_read(&q->numpending));
		return 0;
	} else {
		*entry = q->base + *index;
		return 1;
	}
}

/**
 *	aac_queue_get		-	get the next free QE
 *	@dev: Adapter
 *	@index: Returned index
 *	@priority: Priority of fib
 *	@fib: Fib to associate with the queue entry
 *	@wait: Wait if queue full
 *	@fibptr: Driver fib object to go with fib
 *	@nonotify: Don't notify the adapter
 *
 *	Gets the next free QE off the requested priorty adapter command
 *	queue and associates the Fib with the QE. The QE represented by
 *	index is ready to insert on the queue when this routine returns
 *	success.
 */

int aac_queue_get(struct aac_dev * dev, u32 * index, u32 qid, struct hw_fib * hw_fib, int wait, struct fib * fibptr, unsigned long *nonotify)
{
	struct aac_entry * entry = NULL;
	int map = 0;

	if (qid == AdapNormCmdQueue) {
		/*  if no entries wait for some if caller wants to */
		while (!aac_get_entry(dev, qid, &entry, index, nonotify)) {
			printk(KERN_ERR "GetEntries failed\n");
		}
		/*
		 *	Setup queue entry with a command, status and fib mapped
		 */
		entry->size = cpu_to_le32(le16_to_cpu(hw_fib->header.Size));
		map = 1;
	} else {
		while (!aac_get_entry(dev, qid, &entry, index, nonotify)) {
			/* if no entries wait for some if caller wants to */
		}
		/*
		 *	Setup queue entry with command, status and fib mapped
		 */
		entry->size = cpu_to_le32(le16_to_cpu(hw_fib->header.Size));
		entry->addr = hw_fib->header.SenderFibAddress;
			/* Restore adapters pointer to the FIB */
		hw_fib->header.u.ReceiverFibAddress = hw_fib->header.SenderFibAddress;	/* Let the adapter now where to find its data */
		map = 0;
	}
	/*
	 *	If MapFib is true than we need to map the Fib and put pointers
	 *	in the queue entry.
	 */
	if (map)
		entry->addr = cpu_to_le32(fibptr->hw_fib_pa);
	return 0;
}

/*
 *	Define the highest level of host to adapter communication routines.
 *	These routines will support host to adapter FS commuication. These
 *	routines have no knowledge of the commuication method used. This level
 *	sends and receives FIBs. This level has no knowledge of how these FIBs
 *	get passed back and forth.
 */

/**
 *	aac_fib_send	-	send a fib to the adapter
 *	@command: Command to send
 *	@fibptr: The fib
 *	@size: Size of fib data area
 *	@priority: Priority of Fib
 *	@wait: Async/sync select
 *	@reply: True if a reply is wanted
 *	@callback: Called with reply
 *	@callback_data: Passed to callback
 *
 *	Sends the requested FIB to the adapter and optionally will wait for a
 *	response FIB. If the caller does not wish to wait for a response than
 *	an event to wait on must be supplied. This event will be set when a
 *	response FIB is received from the adapter.
 */
#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
#undef aac_fib_send
fib_send_t aac_fib_send_switch = aac_fib_send;
#endif

int aac_fib_send(u16 command, struct fib *fibptr, unsigned long size,
		int priority, int wait, int reply, fib_callback callback,
		void *callback_data)
#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
#define aac_fib_send aac_fib_send_switch
#endif
{
	struct aac_dev * dev = fibptr->dev;
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	unsigned long flags = 0;
	unsigned long mflags = 0;
	unsigned long sflags;

	if (!(hw_fib->header.XferState & cpu_to_le32(HostOwned)))
		return -EBUSY;

	if (hw_fib->header.XferState & cpu_to_le32(AdapterProcessed))
		return -EINVAL;

	/*
	 *	There are 5 cases with the wait and reponse requested flags.
	 *	The only invalid cases are if the caller requests to wait and
	 *	does not request a response and if the caller does not want a
	 *	response and the Fib is not allocated from pool. If a response
	 *	is not requesed the Fib will just be deallocaed by the DPC
	 *	routine when the response comes back from the adapter. No
	 *	further processing will be done besides deleting the Fib. We
	 *	will have a debug mode where the adapter can notify the host
	 *	it had a problem and the host can log that fact.
	 */
	fibptr->flags &= FIB_CONTEXT_POOL;

	if (wait && !reply) {
		return -EINVAL;
	} else if (!wait && reply) {
		hw_fib->header.XferState |= cpu_to_le32(Async | ResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.AsyncSent);
	} else if (!wait && !reply) {
		hw_fib->header.XferState |= cpu_to_le32(NoResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.NoResponseSent);
	} else if (wait && reply) {
		hw_fib->header.XferState |= cpu_to_le32(ResponseExpected);
		FIB_COUNTER_INCREMENT(aac_config.NormalSent);
#if (defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
		/* If wait >0 and reply != 0 then there's a potential
		 * to land in down() below which will eventually land
		 * us where we'll try to sleep. We can't sleep in this
		 * routine because we're in the issuing path for the
		 * VMkernel. Setting wait <0 causes us to spin for up
		 * to three minutes with down_trylock() instead of down()
		 * so we won't trip the World_AssertIsSafeToBlock().
		 */
		wait = -1;
#endif
	}
	/*
	 *	Map the fib into 32bits by using the fib number
	 */

	hw_fib->header.SenderFibAddress =
		cpu_to_le32(((u32)(fibptr - dev->fibs)) << 2);

	/* use the same shifted value for handle to be compatible
	 * with the new native hba command handle
	 */
	hw_fib->header.Handle =
		cpu_to_le32((((u32)(fibptr - dev->fibs)) << 2) + 1);

	/*
	 *	Set FIB state to indicate where it came from and if we want a
	 *	response from the adapter. Also load the command from the
	 *	caller.
	 *
	 *	Map the hw fib pointer as a 32bit value
	 */
	hw_fib->header.Command = cpu_to_le16(command);
	hw_fib->header.XferState |= cpu_to_le32(SentFromHost);
	/*
	 *	Set the size of the Fib we want to send to the adapter
	 */
	hw_fib->header.Size = cpu_to_le16(sizeof(struct aac_fibhdr) + size);
	if (le16_to_cpu(hw_fib->header.Size) > le16_to_cpu(hw_fib->header.SenderSize))
		return -EMSGSIZE;
	/*
	 *	Get a queue entry connect the FIB to it and send an notify
	 *	the adapter a command is ready.
	 */
	hw_fib->header.XferState |= cpu_to_le32(NormalPriority);

	/*
	 *	Fill in the Callback and CallbackContext if we are not
	 *	going to wait.
	 */
	if (!wait) {
		fibptr->callback = callback;
		fibptr->callback_data = callback_data;
		fibptr->flags |= FIB_CONTEXT_FLAG;
	}

	fibptr->done = 0;

#	if (defined(AAC_DEBUG_INSTRUMENT_FIB))
		printk(KERN_INFO "Fib content %p[%d] P=%llx:\n",
		  hw_fib, le16_to_cpu(hw_fib->header.Size), fibptr->hw_fib_pa);
		{
			int size = le16_to_cpu(hw_fib->header.Size)
					/ sizeof(u32);
			char buffer[80];
			u32 * up = (u32 *)hw_fib;

			while (size > 0) {
				sprintf (buffer,
				  "  %08x %08x %08x %08x %08x %08x %08x %08x\n",
				  up[0], up[1], up[2], up[3], up[4], up[5],
				  up[6], up[7]);
				up += 8;
				size -= 8;
				if (size < 0) {
					buffer[73+(size*9)] = '\n';
					buffer[74+(size*9)] = '\0';
				}
				printk(KERN_INFO "%s", buffer);
			}
		}
#	endif
	FIB_COUNTER_INCREMENT(aac_config.FibsSent);

	dprintk((KERN_DEBUG "Fib contents:.\n"));
	dprintk((KERN_DEBUG "  Command =               %d.\n", le32_to_cpu(hw_fib->header.Command)));
	dprintk((KERN_DEBUG "  SubCommand =            %d.\n", le32_to_cpu(((struct aac_query_mount *)fib_data(fibptr))->command)));
	dprintk((KERN_DEBUG "  XferState  =            %x.\n", le32_to_cpu(hw_fib->header.XferState)));
	dprintk((KERN_DEBUG "  hw_fib va being sent=%p\n",fibptr->hw_fib_va));
	dprintk((KERN_DEBUG "  hw_fib pa being sent=%lx\n",(ulong)fibptr->hw_fib_pa));
	dprintk((KERN_DEBUG "  fib being sent=%p\n",fibptr));

	if (!dev->queues)
	{
#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
		printk(KERN_INFO "aac_fib_send: dev->queues gone!\n");
#endif
		return -EBUSY;
	}

	if(wait) {
		spin_lock_irqsave(&dev->manage_lock, mflags);
		if (dev->management_fib_count >= AAC_NUM_MGT_FIB) {
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
			return -EBUSY;
		}
		dev->management_fib_count++;
		spin_unlock_irqrestore(&dev->manage_lock, mflags);
		spin_lock_irqsave(&fibptr->event_lock, flags);
	}

	if (dev->sync_mode) {
		if (wait)
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
		spin_lock_irqsave(&dev->sync_lock, sflags);
		if (dev->sync_fib) {
			list_add_tail(&fibptr->fiblink, &dev->sync_fib_list);
			spin_unlock_irqrestore(&dev->sync_lock, sflags);
		} else {
			dev->sync_fib = fibptr;
			spin_unlock_irqrestore(&dev->sync_lock, sflags);
			aac_adapter_sync_cmd(dev, SEND_SYNCHRONOUS_FIB, 
				(u32)fibptr->hw_fib_pa, 0, 0, 0, 0, 0, 
				NULL, NULL, NULL, NULL, NULL);
		}
		if (wait) {
			fibptr->flags |= FIB_CONTEXT_FLAG_WAIT;
			if (aac_wait_for_completion_interruptible(&fibptr->event_wait)) {
				fibptr->flags &= ~(FIB_CONTEXT_FLAG_WAIT);
				return -EFAULT;
			}
			fibptr->flags &= ~(FIB_CONTEXT_FLAG_WAIT);
			return 0;
		}
		return -EINPROGRESS;
	}

	DBG_SET_STATE(fibptr, DBG_STATE_INITIALIZED);

	if (aac_adapter_deliver(fibptr) != 0) {
		if (wait) {
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
			spin_lock_irqsave(&dev->manage_lock, mflags);
			dev->management_fib_count--;
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
		}
		return -EBUSY;
	}

	/*
	 *	If the caller wanted us to wait for response wait now.
	 */

	if (wait) {
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		/* Only set for first known interruptable command */
		if (wait < 0) {
			/*
			 * *VERY* Dangerous to time out a command, the
			 * assumption is made that we have no hope of
			 * functioning because an interrupt routing or other
			 * hardware failure has occurred.
			 */
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
			unsigned long count = 180000L;  /* 3 minutes */
#else
			unsigned long count = jiffies + (180 * HZ); /* 3 minutes */
#endif
			while (aac_try_wait_for_completion(&fibptr->event_wait)) {
				int blink;
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
				if (--count == 0) 
#else
				if (time_after(jiffies, count)) 
#endif
				{
					struct aac_queue * q = &dev->queues->queue[AdapNormCmdQueue];
					atomic_dec(&q->numpending);
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: first asynchronous command timed out.\n"
						  "Usually a result of a PCI interrupt routing problem;\n"
						  "update mother board BIOS or consider utilizing one of\n"
						  "the SAFE mode kernel options (acpi, apic etc)\n");
					}
					spin_lock_irqsave(&fibptr->event_lock, flags);
					fibptr->done = 2;
					spin_unlock_irqrestore(&fibptr->event_lock, flags);
					dprintk((KERN_ERR "aacraid: sync. command timed out after 180 seconds\n"));
					return -ETIMEDOUT;
				}

				/* Function returns 0 if there
				 * has not been an EEH error; otherwise returns
				 * a non-zero value.
				 *
				 * Need to be called before any PCI operation,
				 * i.e.,before aac_adapter_check_health()
				 */
				if (unlikely(aac_pci_offline(dev))) {
					/* The EEH mechanisms will handle the error and reset the
					 * device if necessary.
					 */
					return -EFAULT;
				}

				if ((blink = aac_adapter_check_health(dev)) > 0) {
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: adapter blinkLED 0x%x.\n"
						  "Usually a result of a serious unrecoverable hardware problem\n",
						  blink);
					}
					return -EFAULT;
				}
#if (defined(CONFIG_XEN) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
				udelay(5);
#else
				msleep(1);
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
				/**
				* We are waiting for an interrupt but might be in a deadlock here.
				* Since vmkernel is non-preemtible and we are hogging the CPU,
				* in special circumstances the interrupt handler may not get to run.
				* For more details, please see PR 414597.
				*/
				if ((count % 5L) == 0L) {
					// busy waiting for at most 5 ms at a time.
					dprintk((KERN_INFO "aacraid: aac_fib_send: Calling interrupt handler\n"));
					if (!dev->msix_enabled) {
						if (!aac_is_src(dev)) {
							disable_irq(dev->scsi_host_ptr->irq);
							aac_adapter_intr(dev);
							enable_irq(dev->scsi_host_ptr->irq);
						}
					}
				}
#endif
			}
		} else {
            fibptr->flags |= FIB_CONTEXT_FLAG_WAIT;
            if (aac_wait_for_completion_interruptible(&fibptr->event_wait)) {
#if (defined(RHEL_MAJOR) && (RHEL_MAJOR == 7 && RHEL_MINOR == 1))
                if (dev->kdump_msix) {
                    wait_for_completion(&fibptr->event_wait);
                } else
#endif
                fibptr->done = 2;
            }
            fibptr->flags &= ~(FIB_CONTEXT_FLAG_WAIT);
        }
		spin_lock_irqsave(&fibptr->event_lock, flags);
		if ((fibptr->done == 0) || (fibptr->done == 2)) {
			fibptr->done = 2; /* Tell interrupt we aborted */
			spin_unlock_irqrestore(&fibptr->event_lock, flags);

			return -ERESTARTSYS;
		}
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		BUG_ON(fibptr->done == 0);

		if(unlikely(fibptr->flags & FIB_CONTEXT_FLAG_TIMED_OUT))
			return -ETIMEDOUT;

		return 0;
	}
	/*
	 *	If the user does not want a response than return success otherwise
	 *	return pending
	 */
	if (reply)
		return -EINPROGRESS;
	else
		return 0;
}

int aac_hba_send(u8 command, struct fib *fibptr, fib_callback callback,
		void *callback_data)
{
	struct aac_dev * dev = fibptr->dev;
	int wait;
	unsigned long flags = 0;
	unsigned long mflags = 0;

	fibptr->flags |= (FIB_CONTEXT_FLAG | FIB_CONTEXT_FLAG_NATIVE_HBA);
	if (callback) {
		wait = 0;
		fibptr->callback = callback;
		fibptr->callback_data = callback_data;
	} else {	
#if (defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
		wait = -1;
#else
		wait = 1;
#endif		
	}
	
	switch (command) {
	case HBA_IU_TYPE_SCSI_CMD_REQ:
		{
		struct aac_hba_cmd_req *hbacmd = 
		(struct aac_hba_cmd_req *)fibptr->hw_fib_va;
		
		hbacmd->iu_type = command;
		/* bit1 of request_id must be 0 */
		hbacmd->request_id =
		cpu_to_le32((((u32)(fibptr - dev->fibs)) << 2) + 1);
		fibptr->flags |= FIB_CONTEXT_FLAG_SCSI_CMD;	
		}
		break;
	case HBA_IU_TYPE_SCSI_TM_REQ:
		{
		struct aac_hba_tm_req *tmf = 
		(struct aac_hba_tm_req *)fibptr->hw_fib_va;
	
		tmf->iu_type = command;
		/* bit1 of request_id must be 0 */
		tmf->request_id = 
			cpu_to_le32((((u32)(fibptr - dev->fibs)) << 2) + 1);
		fibptr->flags |= FIB_CONTEXT_FLAG_NATIVE_HBA_TMF;
		}
		break;
	case HBA_IU_TYPE_SATA_REQ:
		{
		struct aac_hba_reset_req *hbacmd = 
		(struct aac_hba_reset_req *)fibptr->hw_fib_va;
	
		hbacmd->iu_type = command;
		/* bit1 of request_id must be 0 */
		hbacmd->request_id =
			cpu_to_le32((((u32)(fibptr - dev->fibs)) << 2) + 1);
		fibptr->flags |= FIB_CONTEXT_FLAG_NATIVE_HBA_TMF;
		}
		break;
	default:
		return -EINVAL;
	}
	if (wait) {
		spin_lock_irqsave(&dev->manage_lock, mflags);
		if (dev->management_fib_count >= AAC_NUM_MGT_FIB) {
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
			return -EBUSY;
		}
		dev->management_fib_count++;
		spin_unlock_irqrestore(&dev->manage_lock, mflags);
		spin_lock_irqsave(&fibptr->event_lock, flags);
	}	
	
	if (aac_adapter_deliver(fibptr) != 0) { 
		if (wait) {
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
			spin_lock_irqsave(&dev->manage_lock, mflags);
			dev->management_fib_count--;
			spin_unlock_irqrestore(&dev->manage_lock, mflags);
		}
		return -EBUSY;
	}
	FIB_COUNTER_INCREMENT(aac_config.NativeSent);

	if (wait) {
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		/* Only set for first known interruptable command */
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
		if (wait < 0) {
			/*
			 * *VERY* Dangerous to time out a command, the
			 * assumption is made that we have no hope of
			 * functioning because an interrupt routing or other
			 * hardware failure has occurred.
			 */
			unsigned long count = 180000L;  /* 3 minutes */
			while (aac_try_wait_for_completion(&fibptr->event_wait)) {
				int blink;
				if (--count == 0)
				{
					struct aac_queue * q = &dev->queues->queue[AdapNormCmdQueue];
					atomic_dec(&q->numpending);
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: first asynchronous command timed out.\n"
						  "Usually a result of a PCI interrupt routing problem;\n"
						  "update mother board BIOS or consider utilizing one of\n"
						  "the SAFE mode kernel options (acpi, apic etc)\n");
					}
					spin_lock_irqsave(&fibptr->event_lock, flags);
					fibptr->done = 2;
					spin_unlock_irqrestore(&fibptr->event_lock, flags);
					dprintk((KERN_ERR "aacraid: sync. command timed out after 180 seconds\n"));
					return -ETIMEDOUT;
				}

				/* Function returns 0 if there
				 * has not been an EEH error; otherwise returns
				 * a non-zero value.
				 *
				 * Need to be called before any PCI operation,
				 * i.e.,before aac_adapter_check_health()
				 */
				if (unlikely(aac_pci_offline(dev))) {
					/* The EEH mechanisms will handle the error and reset the
					 * device if necessary.
					 */
					return -EFAULT;
				}

				if ((blink = aac_adapter_check_health(dev)) > 0) {
					if (wait == -1) {
	        				printk(KERN_ERR "aacraid: aac_fib_send: adapter blinkLED 0x%x.\n"
						  "Usually a result of a serious unrecoverable hardware problem\n",
						  blink);
					}
					return -EFAULT;
				}
#if (defined(CONFIG_XEN) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
				udelay(5);
#else
				msleep(1);
#endif
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
				/**
				* We are waiting for an interrupt but might be in a deadlock here.
				* Since vmkernel is non-preemtible and we are hogging the CPU,
				* in special circumstances the interrupt handler may not get to run.
				* For more details, please see PR 414597.
				*/
				if ((count % 5L) == 0L) {
					// busy waiting for at most 5 ms at a time.
					dprintk((KERN_INFO "aacraid: aac_fib_send: Calling interrupt handler\n"));
					if (!dev->msix_enabled) {
						if (!aac_is_src(dev)) {
							disable_irq(dev->scsi_host_ptr->irq);
							aac_adapter_intr(dev);
							enable_irq(dev->scsi_host_ptr->irq);
						}
					}
				}
#endif
			}
		}
#endif
		fibptr->flags |= FIB_CONTEXT_FLAG_WAIT;
		if (aac_wait_for_completion_interruptible(&fibptr->event_wait)) {
			fibptr->done = 2;
		}
		fibptr->flags &= ~(FIB_CONTEXT_FLAG_WAIT);

		spin_lock_irqsave(&fibptr->event_lock, flags);
		if ((fibptr->done == 0) || (fibptr->done == 2)) {
			fibptr->done = 2; /* Tell interrupt we aborted */
			spin_unlock_irqrestore(&fibptr->event_lock, flags);
			return -ERESTARTSYS;
		}
		spin_unlock_irqrestore(&fibptr->event_lock, flags);
		BUG_ON(fibptr->done == 0);

		if(unlikely(fibptr->flags & FIB_CONTEXT_FLAG_TIMED_OUT))
			return -ETIMEDOUT;

		return 0;
	}
	
	return -EINPROGRESS;
}

/**
 *	aac_consumer_get	-	get the top of the queue
 *	@dev: Adapter
 *	@q: Queue
 *	@entry: Return entry
 *
 *	Will return a pointer to the entry on the top of the queue requested that
 *	we are a consumer of, and return the address of the queue entry. It does
 *	not change the state of the queue.
 */

int aac_consumer_get(struct aac_dev * dev, struct aac_queue * q, struct aac_entry **entry)
{
	u32 index;
	int status;
	if (le32_to_cpu(*q->headers.producer) == le32_to_cpu(*q->headers.consumer)) {
		status = 0;
	} else {
		/*
		 *	The consumer index must be wrapped if we have reached
		 *	the end of the queue, else we just use the entry
		 *	pointed to by the header index
		 */
		if (le32_to_cpu(*q->headers.consumer) >= q->entries)
			index = 0;
		else
			index = le32_to_cpu(*q->headers.consumer);
		*entry = q->base + index;
		status = 1;
	}
	return(status);
}

/**
 *	aac_consumer_free	-	free consumer entry
 *	@dev: Adapter
 *	@q: Queue
 *	@qid: Queue ident
 *
 *	Frees up the current top of the queue we are a consumer of. If the
 *	queue was full notify the producer that the queue is no longer full.
 */

void aac_consumer_free(struct aac_dev * dev, struct aac_queue *q, u32 qid)
{
	int wasfull = 0;
	u32 notify;

	if ((le32_to_cpu(*q->headers.producer)+1) == le32_to_cpu(*q->headers.consumer))
		wasfull = 1;

	if (le32_to_cpu(*q->headers.consumer) >= q->entries)
		*q->headers.consumer = cpu_to_le32(1);
	else
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,24)) && !defined(HAS_LE32_ADD_CPU))
		*q->headers.consumer = cpu_to_le32(le32_to_cpu(*q->headers.consumer)+1);
#else
		le32_add_cpu(q->headers.consumer, 1);
#endif

	if (wasfull) {
		switch (qid) {

		case HostNormCmdQueue:
			notify = HostNormCmdNotFull;
			break;
		case HostNormRespQueue:
			notify = HostNormRespNotFull;
			break;
		default:
			BUG();
			return;
		}
		aac_adapter_notify(dev, notify);
	}
}

/**
 *	aac_fib_adapter_complete	-	complete adapter issued fib
 *	@fibptr: fib to complete
 *	@size: size of fib
 *
 *	Will do all necessary work to complete a FIB that was sent from
 *	the adapter.
 */

int aac_fib_adapter_complete(struct fib *fibptr, unsigned short size)
{
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	struct aac_dev * dev = fibptr->dev;
	struct aac_queue * q;
	unsigned long nointr = 0;
	unsigned long qflags;

	if (dev->comm_interface == AAC_COMM_MESSAGE_TYPE1 || 
		dev->comm_interface == AAC_COMM_MESSAGE_TYPE2 || 
		dev->comm_interface == AAC_COMM_MESSAGE_TYPE3) {
		kfree (hw_fib);
		return 0;
	}

	if (hw_fib->header.XferState == 0) {
		if (dev->comm_interface == AAC_COMM_MESSAGE)
			kfree (hw_fib);
		return 0;
	}
	/*
	 *	If we plan to do anything check the structure type first.
	 */
	if (hw_fib->header.StructType != FIB_MAGIC &&
		hw_fib->header.StructType != FIB_MAGIC2 &&
		hw_fib->header.StructType != FIB_MAGIC2_64) {
		if (dev->comm_interface == AAC_COMM_MESSAGE)
			kfree (hw_fib);
		return -EINVAL;
	}
	/*
	 *	This block handles the case where the adapter had sent us a
	 *	command and we have finished processing the command. We
	 *	call completeFib when we are done processing the command
	 *	and want to send a response back to the adapter. This will
	 *	send the completed cdb to the adapter.
	 */
	if (hw_fib->header.XferState & cpu_to_le32(SentFromAdapter)) {
		if (dev->comm_interface == AAC_COMM_MESSAGE) {
			kfree (hw_fib);
		} else {
			u32 index;
			hw_fib->header.XferState |= cpu_to_le32(HostProcessed);
			if (size) {
				size += sizeof(struct aac_fibhdr);
				if (size > le16_to_cpu(hw_fib->header.SenderSize))
					return -EMSGSIZE;
				hw_fib->header.Size = cpu_to_le16(size);
			}
			q = &dev->queues->queue[AdapNormRespQueue];
			spin_lock_irqsave(q->lock, qflags);
			aac_queue_get(dev, &index, AdapNormRespQueue, hw_fib, 1, NULL, &nointr);
			*(q->headers.producer) = cpu_to_le32(index + 1);
			spin_unlock_irqrestore(q->lock, qflags);
			if (!(nointr & (int)aac_config.irq_mod))
				aac_adapter_notify(dev, AdapNormRespQueue);
		}
	} else {
		printk(KERN_WARNING "aac_fib_adapter_complete: "
			"Unknown xferstate detected.\n");
		BUG();
	}
	return 0;
}

/**
 *	aac_fib_complete	-	fib completion handler
 *	@fib: FIB to complete
 *
 *	Will do all necessary work to complete a FIB.
 */

int aac_fib_complete(struct fib *fibptr)
{
	struct hw_fib * hw_fib = fibptr->hw_fib_va;

	/*
	 *	Check for a fib which has already been completed or with a
	 *	status wait timeout
	 */

	if (fibptr->flags & FIB_CONTEXT_FLAG_NATIVE_HBA) {
		fib_dealloc(fibptr);
		return 0;
	}

	if (hw_fib->header.XferState == 0 || fibptr->done == 2)
		return 0;
	/*
	 *	If we plan to do anything check the structure type first.
	 */

	if (hw_fib->header.StructType != FIB_MAGIC &&
		hw_fib->header.StructType != FIB_MAGIC2 &&
		hw_fib->header.StructType != FIB_MAGIC2_64)
		return -EINVAL;
	/*
	 *	This block completes a cdb which orginated on the host and we
	 *	just need to deallocate the cdb or reinit it. At this point the
	 *	command is complete that we had sent to the adapter and this
	 *	cdb could be reused.
	 */

	if((hw_fib->header.XferState & cpu_to_le32(SentFromHost)) &&
		(hw_fib->header.XferState & cpu_to_le32(AdapterProcessed)))
	{
		fib_dealloc(fibptr);
	}
	else if(hw_fib->header.XferState & cpu_to_le32(SentFromHost))
	{
		/*
		 *	This handles the case when the host has aborted the I/O
		 *	to the adapter because the adapter is not responding
		 */
		fib_dealloc(fibptr);
	} else if(hw_fib->header.XferState & cpu_to_le32(HostOwned)) {
		fib_dealloc(fibptr);
	} else {
		BUG();
	}
	return 0;
}

/**
 *	aac_printf	-	handle printf from firmware
 *	@dev: Adapter
 *	@val: Message info
 *
 *	Print a message passed to us by the controller firmware on the
 *	Adaptec board
 */

void aac_printf(struct aac_dev *dev, u32 val)
{
	char *cp = dev->printfbuf;
#if (!defined(AAC_PRINTF_ENABLED))
	if (dev->printf_enabled)
#endif
	{
		int length = val & 0xffff;
		int level = (val >> 16) & 0xffff;

		/*
		 *	The size of the printfbuf is set in port.c
		 *	There is no variable or define for it
		 */
		if (length > 255)
			length = 255;
		if (cp[length] != 0)
			cp[length] = 0;
		if (level == LOG_AAC_HIGH_ERROR)
			printk(KERN_WARNING "%s:%s", dev->name, cp);
		else
			printk(KERN_INFO "%s:%s", dev->name, cp);
	}
	memset(cp, 0, 256);
}

#ifdef AAC_SAS_TRANSPORT

static inline int aac_is_safw_sas_device_exposed(struct aac_dev *aac, int bus, int target)
{
	struct aac_hba_map_info *hmi;

	hmi = &aac->hba_map[bus][target];
	return hmi->sas_info.aac_sas_port && hmi->sas_info.aac_sas_phy;
}

int aac_remove_safw_sas_device(struct aac_dev *aac, int bus, int target)
{
	struct aac_hba_map_info *dev_info;
	struct sas_port *aac_sas_port;
	struct sas_phy *aac_sas_phy;

	if (aac_is_safw_sas_device_exposed(aac, bus ,target)) {
		int next_target_id;

		dev_info = &aac->hba_map[bus][target];

		next_target_id = dev_info->sas_info.phy_identifier;
		aac_sas_port = dev_info->sas_info.aac_sas_port;
		aac_sas_phy  = dev_info->sas_info.aac_sas_phy;

		aac_info(aac, "<======REMOVE (%d:%d <-> %d:%d) ========>\n",
						bus, target, 0, next_target_id);

		sas_port_delete_phy(aac_sas_port, aac_sas_phy);
		adbg_sas(aac, KERN_INFO, "Delete phy:%d from port:%d\n",
						next_target_id,
						aac_sas_port->port_identifier);

		sas_phy_delete(aac_sas_phy);
		adbg_sas(aac, KERN_INFO, "Delete phy:%d\n",
						next_target_id);

		sas_port_delete(aac_sas_port);
		adbg_sas(aac, KERN_INFO, "Delete port:%d\n",
						aac_sas_port->port_identifier);

		dev_info->sas_info.is_sas_info_set = 0;
		adbg_sas(aac, KERN_INFO, "(%d:%d) -> is_sas_info_set:%d\n",
				bus, target,
				dev_info->sas_info.is_sas_info_set);

		dev_info->sas_info.aac_sas_port = NULL;
		dev_info->sas_info.aac_sas_phy  = NULL;
		dev_info->sas_info.aac_sas_rphy  = NULL;

		aac_info(aac, "<==== REMOVE END ====>\n");
	}

	return 0;
}

void aac_remove_all_safw_sas_devices(struct aac_dev *aac)
{

	int i = 0;
	int bus = 0;
	int target = 0;

	if (!aac_transport_enabled(aac))
		return;

	for(i = 0; i < AAC_BUS_TARGET_LOOP; i++) {

		bus	= get_bus_number(i);
		target	= get_target_number(i);

		aac_remove_safw_sas_device(aac, bus, target);
	}
}

static void aac_setup_safw_sas_identify(struct aac_dev *aac,
					struct sas_identify *identify,
					struct aac_hba_map_info *device_info)
{
	u64 sas_address = device_info->sas_info.sas_address;

	if (aac_is_safw_smp_expander(aac, device_info)){
		identify->target_port_protocols    = SAS_PROTOCOL_SMP;
		identify->device_type              = SAS_FANOUT_EXPANDER_DEVICE;
	} else {
		identify->target_port_protocols    = SAS_PROTOCOL_SSP;
		identify->device_type              = SAS_END_DEVICE;
	}

	identify->sas_address              = sas_address;
	identify->phy_identifier           = device_info->sas_info.phy_identifier;
	identify->initiator_port_protocols = 0;
}

int aac_add_safw_sas_device(struct aac_dev *aac, int bus, int target )
{
	struct sas_port *aac_port;
	struct sas_rphy *aac_rphy;
	struct sas_phy *aac_phy;
	struct sas_identify *identify;
	struct aac_hba_map_info *dev_info;
	int ret = 0;
	int next_target_id = 0;

	dev_info = &aac->hba_map[bus][target];
	next_target_id = dev_info->sas_info.phy_identifier;

	aac_info(aac,"<======== ADD (%d:%d <-> %d:%d)========>\n",
					bus, target, 0, next_target_id);

	/* alloc port */
	aac_port = sas_port_alloc_num(&aac->scsi_host_ptr->shost_gendev);
	if( !aac_port) {
		aac_err(aac, "Allocating device's sas port failed\n");
		return -ENOMEM;
	}
	adbg_sas(aac, KERN_INFO, "allocated port identifier:%d\n",
						aac_port->port_identifier);

	/*
	 * add port to the system
	 */
	ret = sas_port_add(aac_port);
	if (ret)
		goto port_add_error_out;
	adbg_sas(aac, KERN_INFO, "added port identifier:%d\n",
						aac_port->port_identifier);

	/*
	 * Filling up reference bus/target info in hba_map
	 */
	dev_info->host_bus_num = 0;
	dev_info->host_target_num = next_target_id;

	/*
	 * Save port to internal data structure
	 */
	dev_info->sas_info.aac_sas_port = aac_port;

	aac_phy = sas_phy_alloc(&aac->scsi_host_ptr->shost_gendev, next_target_id);
	if( !aac_phy) {
		aac_err(aac,"Allocating device's sas phy failed\n");
		ret = -ENOMEM;
		goto port_add_error_out;
	}
	adbg_sas(aac, KERN_INFO, "allocated phy identifier:%d\n",
						next_target_id);

	/*
	 * TODO: change initiator and target protocol to real value
	 */
	identify = &aac_phy->identify;
	aac_setup_safw_sas_identify(aac, identify, dev_info);

	aac_phy->minimum_linkrate_hw	= SAS_LINK_RATE_UNKNOWN;
	aac_phy->maximum_linkrate_hw	= SAS_LINK_RATE_UNKNOWN;
	aac_phy->minimum_linkrate	= SAS_LINK_RATE_UNKNOWN;
	aac_phy->maximum_linkrate	= SAS_LINK_RATE_UNKNOWN;
	aac_phy->negotiated_linkrate	= SAS_LINK_RATE_UNKNOWN;

	if (sas_phy_add(aac_phy)) {
		aac_err(aac,"Allocating device's sas phy failed\n");
		ret = -ENOMEM;
		goto phy_add_error_out;
	}
	adbg_sas(aac, KERN_INFO, "added phy identifier:%d\n",
						aac_phy->number);

	sas_port_add_phy(aac_port, aac_phy);
	adbg_sas(aac, KERN_INFO, "port:%d<->phy:%d\n",
						aac_port->port_identifier,
						aac_phy->number);

	/*
	 * Save phy to internal data structure
	 */
	dev_info->sas_info.aac_sas_phy = aac_phy;

	/*
	 * alloc remote phy
	 */
	if (aac_is_safw_smp_expander(aac, dev_info)) {
		aac_rphy = sas_expander_alloc(aac_port, SAS_FANOUT_EXPANDER_DEVICE);
		adbg_sas(aac, KERN_INFO, "allocated sas expander rphy:%d from port:%d\n",
						aac_rphy->scsi_target_id,
						aac_port->port_identifier);
	} else {
		aac_rphy = sas_end_device_alloc(aac_port);
		adbg_sas(aac, KERN_INFO, "allocated end device rphy:%d from port:%d\n",
						aac_rphy->scsi_target_id,
						aac_port->port_identifier);
	}

	if (!aac_rphy) {
		aac_err(aac,"Allocating device's sas rphy failed\n");
		ret = -ENOMEM;
		goto port_add_error_out;
	}

	identify = &aac_rphy->identify;
	aac_setup_safw_sas_identify(aac, identify, &aac->hba_map[bus][target]);

	/* Save rphy to internal data structure */
	dev_info->sas_info.aac_sas_rphy = aac_rphy;

	/* add remote phy */
	ret = sas_rphy_add(aac_rphy);
	if (ret) {
		aac_err(aac,"add remote phy failed");
		goto rphy_add_error_out;
	}
	adbg_sas(aac, KERN_INFO, "added rphy:%d device_type->%d\n",
						aac_rphy->scsi_target_id,
						identify->device_type);

	dev_info(&aac_rphy->dev,"added device -> (%d:%d)<->(%d:%d) -> 0x%llx",
					bus, target, 0, next_target_id,
					identify->sas_address);


out:
	aac_info(aac,"<======== ADD END ========>");
	return ret;

rphy_add_error_out:
	sas_rphy_free(aac_rphy);

phy_add_error_out:
	sas_phy_free(aac_phy);

port_add_error_out:
	sas_port_free(aac_port);

	goto out;
}
#endif

static inline int is_safw_raid_volume(struct aac_dev *aac, int bus, int target)
{
	return bus == CONTAINER_CHANNEL && target < aac->maximum_num_containers;
}

static inline int is_safw_sas_host_hba(struct aac_dev *aac, int bus, int target)
{
	return bus == CONTAINER_CHANNEL && target >= aac->maximum_num_containers;
}

static inline int is_safw_hba(struct aac_dev *aac, int bus, int target)
{
	/*
	 * for the sake of consistency
	 */
	UNUSED(aac);
	return bus > CONTAINER_CHANNEL;
}

#if defined(AAC_SAS_TRANSPORT)
static int aac_lookup_safw_sas_rphy(struct aac_dev *aac, struct scsi_device *sdev,
								u32 *bus,
								u32 *cid)
{
	u32 b,t;
	int i = 0;
	struct sas_rphy *rphy;

	if (!sdev)
		goto out;
	rphy = target_to_rphy(scsi_target(sdev));

	for (i = 0; i < AAC_BUS_TARGET_LOOP; i++) {
		b = get_bus_number(i);
		t = get_target_number(i);

		if (aac->hba_map[b][t].sas_info.aac_sas_rphy == rphy) {
			*bus = b;
			*cid = t;
			return 0;
		}
	}

	*bus = 0xff;
	*cid = 0xff;
out:
	return -ENODEV;
}
#endif

int aac_get_safw_internal_bus_cid(struct aac_dev *dev, struct scsi_device *sdev,
								u32 *bus,
								u32 *cid)
{
#if defined(AAC_SAS_TRANSPORT)
	if (is_safw_sas_host_hba(dev, sdev_channel(sdev), sdev_id(sdev))) {
		return aac_lookup_safw_sas_rphy(dev, sdev, bus, cid);
	}
#endif

	*bus = sdev_channel(sdev);
	*cid = sdev_id(sdev);

	return 0;
}

static struct scsi_device *aac_lookup_safw_scsi_device(struct aac_dev *dev,
								int bus,
								int target)
{
	int skip = 0;
#if defined(AAC_SAS_TRANSPORT)
	int host_bus;
	int host_target;

	/*
	 * Might happen for transport enable variable
	 */
	if (!aac_transport_enabled(dev))
		goto out;

	/*
	 * bus == 0 and target <= 64 (Scsi)
	 */
	if (is_safw_raid_volume(dev, bus, target))
		goto out;

	/*
	 * bus == 0 and target > 64, might interfere with (0, 64),(0,65)...
	 */
	if (is_safw_sas_host_hba(dev, bus, target)) {
		skip = 1;
		goto out;
	}

	host_bus = dev->hba_map[bus][target].host_bus_num;
	host_target = dev->hba_map[bus][target].host_target_num;

	if (host_bus != INVALID) {
		bus = host_bus;
		target = host_target;
	} else
		skip = 1;
out:
#else
	if (bus == CONTAINER_CHANNEL)
		bus = CONTAINER_CHANNEL;
	else
		bus = aac_phys_to_logical(bus);
#endif
	if (skip)
		return NULL;
	else
		return scsi_device_lookup(dev->scsi_host_ptr, bus, target, 0);
}

static int aac_add_safw_device(struct aac_dev *dev, int bus, int target)
{
#if defined(AAC_SAS_TRANSPORT)
	if(aac_transport_enabled(dev) && is_safw_hba(dev, bus, target)) {
		adbg_init_or_aif(dev, KERN_INFO, "(%d:%d)->aac_sas_add_device\n",
								bus, target);
		return aac_add_safw_sas_device(dev, bus, target);
	}
#else
	if (bus == CONTAINER_CHANNEL)
		bus = CONTAINER_CHANNEL;
	else
		bus = aac_phys_to_logical(bus);
#endif
	adbg_init_or_aif(dev, KERN_INFO, "(%d:%d)->scsi_add_device\n",
							bus, target);
	return scsi_add_device(dev->scsi_host_ptr, bus, target, 0);
}

static void aac_put_safw_scsi_device(struct scsi_device *sdev)
{
	if (sdev)
		scsi_device_put(sdev);
}

static void aac_remove_safw_device(struct aac_dev *dev, int bus, int target)
{
	struct scsi_device *sdev;

#if defined(AAC_SAS_TRANSPORT)
	if(aac_transport_enabled(dev) && is_safw_hba(dev, bus, target)) {
		adbg_init_or_aif(dev, KERN_INFO, "(%d:%d)->aac_sas_remove_device\n",
						bus, target);
		aac_remove_safw_sas_device(dev, bus, target);
		return;
	}
#endif

	sdev = aac_lookup_safw_scsi_device(dev, bus, target);
	adbg_init_or_aif(dev, KERN_INFO, "(%d:%d)->scsi_remove_device\n",
							bus, target);
	scsi_remove_device(sdev);

	aac_put_safw_scsi_device(sdev);
}

static inline int  aac_is_safw_scan_count_equal(struct aac_dev *dev, int bus, int target)
{
	/*
	 * Check for wierd bus and target numbers
	 */
	return dev->hba_map[bus][target].scan_counter == dev->scan_counter;
}

static int aac_is_safw_target_valid(struct aac_dev *dev, int bus, int target)
{
	if (is_safw_raid_volume(dev, bus, target))
		return dev->fsa_dev[target].valid;
	else
		return aac_is_safw_scan_count_equal(dev, bus, target);
}

static int aac_is_safw_device_exposed(struct aac_dev *dev, int bus, int target)
{
	int is_exposed = 0;
	struct scsi_device *sdev;

	sdev = aac_lookup_safw_scsi_device(dev, bus, target);
	if (sdev) {
		is_exposed = 1;
		goto out;
	}

#if defined(AAC_SAS_TRANSPORT)
	if(aac_is_safw_sas_device_exposed(dev, bus, target))
		is_exposed = 1;
#endif
out:
	aac_put_safw_scsi_device(sdev);
	return is_exposed;
}

static int aac_update_safw_host_devices(struct aac_dev *dev, enum aac_init_mode m)
{
	int i;
	int bus;
	int target;
	int is_exposed = 0;
	int rcode = 0;

	rcode = aac_setup_safw_adapter(dev, m);
	if (unlikely(rcode < 0)) {
		goto out;
	}

	for(i = 0; i < AAC_BUS_TARGET_LOOP; i++) {

		bus = get_bus_number(i);
		target = get_target_number(i);

		is_exposed = aac_is_safw_device_exposed(dev, bus ,target);

		if (aac_is_safw_target_valid(dev, bus, target) && !is_exposed)
			aac_add_safw_device(dev, bus, target);
		else if (!aac_is_safw_target_valid(dev, bus, target) && is_exposed)
			aac_remove_safw_device(dev, bus, target);
	}
out:
	return rcode;
}

static inline void aac_schedule_safw_scan_worker(struct aac_dev *dev)
{
	schedule_delayed_work(&dev->safw_rescan_worker, AAC_RESCAN_DELAY);
}

static inline void aac_schedule_src_reinit_aif_worker(struct aac_dev *dev)
{
	schedule_delayed_work(&dev->src_reinit_aif_worker, 
				  AAC_RESCAN_DELAY);
}

static int aac_scan_safw_host(struct aac_dev *dev, enum aac_init_mode m)
{
	int rcode = 0;

	rcode = aac_update_safw_host_devices(dev, m);
	if (rcode)
		aac_schedule_safw_scan_worker(dev);

	return rcode;
}

int aac_scan_host(struct aac_dev *dev, enum aac_init_mode m)
{
	int rcode = 0;

	mutex_lock(&dev->scan_mutex);
	if (dev->sa_firmware) {
		rcode = aac_scan_safw_host(dev, m);
		goto out;
	} else
		scsi_scan_host(dev->scsi_host_ptr);
out:
	mutex_unlock(&dev->scan_mutex);
	return rcode;
}

void aac_safw_rescan_worker(struct work_struct *work)
{
	struct aac_dev *dev = container_of(to_delayed_work(work), struct aac_dev,
				safw_rescan_worker);

	if (dev->adapter_shutdown)
		return;

	wait_event(dev->scsi_host_ptr->host_wait,
		!scsi_host_in_recovery(dev->scsi_host_ptr));

	aac_scan_host(dev, AAC_REINIT);
}

/*
 * aac_src_init_aif: Initialize the AIF notification after reset
 *
 */
void aac_src_reinit_aif_worker(struct work_struct *work)
{
	struct aac_dev *dev = container_of(to_delayed_work(work), struct aac_dev,
				src_reinit_aif_worker);

	if (dev->adapter_shutdown)
		return;

	wait_event(dev->scsi_host_ptr->host_wait,
		!scsi_host_in_recovery(dev->scsi_host_ptr));

	aac_reinit_aif(dev, dev->cardtype);
}

/**
 *	aac_handle_sa_aif		-	Handle a message from the firmware
 *	@dev: Which adapter this fib is from
 *	@fibptr: Pointer to fibptr from adapter
 *
 *	This routine handles a driver notify fib from the adapter and
 *	dispatches it to the appropriate routine for handling.
 */

static void aac_handle_sa_aif(struct aac_dev * dev, struct fib * fibptr)
{
	int i;
	u32 events = 0;

	if (fibptr->hbacmd_size & SA_AIF_HOTPLUG)
		events = SA_AIF_HOTPLUG;
	else if(fibptr->hbacmd_size & SA_AIF_HARDWARE)
		events = SA_AIF_HARDWARE;
	else if(fibptr->hbacmd_size & SA_AIF_PDEV_CHANGE)
		events = SA_AIF_PDEV_CHANGE;
	else if(fibptr->hbacmd_size & SA_AIF_LDEV_CHANGE)
		events = SA_AIF_LDEV_CHANGE;
	else if(fibptr->hbacmd_size & SA_AIF_BPSTAT_CHANGE)
		events = SA_AIF_BPSTAT_CHANGE;
	else if(fibptr->hbacmd_size & SA_AIF_BPCFG_CHANGE)
		events = SA_AIF_BPCFG_CHANGE;

	switch (events) {
	case SA_AIF_HOTPLUG:
	case SA_AIF_HARDWARE:
	case SA_AIF_PDEV_CHANGE:
	case SA_AIF_LDEV_CHANGE:
	case SA_AIF_BPCFG_CHANGE:

	aac_scan_host(dev, AAC_REINIT);
	break;

	case SA_AIF_BPSTAT_CHANGE:
		adbg_aif(dev, KERN_INFO, "SA_AIF_BPSTAT_CHANGE- do nothing\n");
		break;
	default:
		adbg_aif(dev, KERN_ERR, "Unknown AIF call -%x\n", events);
		break;
	}

	for (i = 1; i <= 10; ++i) {
		events = src_readl(dev, MUnit.IDR);
		if (events & (1<<23)) {
			printk(KERN_WARNING "aac_handle_sa_aif: AIF not cleared by firmware (attempt %d/%d)\n",
				i, 10);
			ssleep(1);
		}
	}
}

/**
 *	aac_handle_aif		-	Handle a message from the firmware
 *	@dev: Which adapter this fib is from
 *	@fibptr: Pointer to fibptr from adapter
 *
 *	This routine handles a driver notify fib from the adapter and
 *	dispatches it to the appropriate routine for handling.
 */

#define AIF_SNIFF_TIMEOUT	(500*HZ)
static void aac_handle_aif(struct aac_dev * dev, struct fib * fibptr)
{
	struct hw_fib * hw_fib = fibptr->hw_fib_va;
	struct aac_aifcmd * aifcmd = (struct aac_aifcmd *)hw_fib->data;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3))
	int busy;
#endif
	u32 channel, id, lun, container;
	struct scsi_device *device;
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,10)) && defined(MODULE))
	struct scsi_driver * drv;
#endif
	enum {
		NOTHING,
		DELETE,
		ADD,
		CHANGE
	} device_config_needed = NOTHING;
//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE))) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__)
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE)))
	extern struct proc_dir_entry * proc_scsi;
#endif

	/* Sniff for container changes */
	adbg_aif(dev,KERN_INFO,
	  "aac_handle_aif: Aif command=%x type=%x container=%d\n",
	  le32_to_cpu(aifcmd->command), le32_to_cpu(*(__le32 *)aifcmd->data),
	  le32_to_cpu(((__le32 *)aifcmd->data)[1]));

	if (!dev || !dev->fsa_dev)
		return;
	container = channel = id = lun = (u32)-1;

	/*
	 *	We have set this up to try and minimize the number of
	 * re-configures that take place. As a result of this when
	 * certain AIF's come in we will set a flag waiting for another
	 * type of AIF before setting the re-config flag.
	 */
	switch (le32_to_cpu(aifcmd->command)) {
	case AifCmdDriverNotify:
		switch (le32_to_cpu(((__le32 *)aifcmd->data)[0])) {
		case AifRawDeviceRemove:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if ((container >> 28)) {
				container = (u32)-1;
				break;
			}
			channel = (container >> 24) & 0xF;
			if (channel >= dev->maximum_num_channels) {
				container = (u32)-1;
				break;
			}
			id = container & 0xFFFF;
			if (id >= dev->maximum_num_physicals) {
				container = (u32)-1;
				break;
			}
			lun = (container >> 16) & 0xFF;
			container = (u32)-1;
			channel = aac_phys_to_logical(channel);
			device_config_needed = DELETE;
			break;

		/*
		 *	Morph or Expand complete
		 */
		case AifDenMorphComplete:
		case AifDenVolumeExtendComplete:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
		    adbg_aif(dev,KERN_INFO,"container=%d(%d,%d,%d,%d)\n",
			  container,
			  (dev && dev->scsi_host_ptr)
			    ? dev->scsi_host_ptr->host_no
			    : -1, CONTAINER_TO_CHANNEL(container),
                                CONTAINER_TO_ID(container),
                                CONTAINER_TO_LUN(container));

			/*
			 *	Find the scsi_device associated with the SCSI
			 * address. Make sure we have the right array, and if
			 * so set the flag to initiate a new re-config once we
			 * see an AifEnConfigChange AIF come through.
			 */

			if ((dev != NULL) && (dev->scsi_host_ptr != NULL)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3))
				device = scsi_device_lookup(dev->scsi_host_ptr,
					CONTAINER_TO_CHANNEL(container),
					CONTAINER_TO_ID(container),
					CONTAINER_TO_LUN(container));
				if (device) {
					dev->fsa_dev[container].config_needed = CHANGE;
					dev->fsa_dev[container].config_waiting_on = AifEnConfigChange;
					dev->fsa_dev[container].config_waiting_stamp = jiffies;
					scsi_device_put(device);
				}
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
				shost_for_each_device(device,
					dev->scsi_host_ptr)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
				list_for_each_entry(device,
					&dev->scsi_host_ptr->my_devices,
					siblings)
#else
				for (device = dev->scsi_host_ptr->host_queue;
				  device != (struct scsi_device *)NULL;
				  device = device->next)
#endif
					if ((device->channel ==
						CONTAINER_TO_CHANNEL(container))
					 && (device->id ==
						CONTAINER_TO_ID(container))
					 && (device->lun ==
						CONTAINER_TO_LUN(container))) {
						dev->fsa_dev[container].config_needed = CHANGE;
						dev->fsa_dev[container].config_waiting_on = AifEnConfigChange;
						dev->fsa_dev[container].config_waiting_stamp = jiffies;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
						scsi_device_put(device);
#endif
						break;
					}
#endif
			}
		}

		/*
		 *	If we are waiting on something and this happens to be
		 * that thing then set the re-configure flag.
		 */
		if (container != (u32)-1) {
			if (container >= dev->maximum_num_containers)
				break;
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		} else for (container = 0;
		    container < dev->maximum_num_containers; ++container) {
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		}
		break;

	case AifCmdEventNotify:
		switch (le32_to_cpu(((__le32 *)aifcmd->data)[0])) {
		case AifEnBatteryEvent:
			dev->cache_protected =
				(((__le32 *)aifcmd->data)[1] == cpu_to_le32(3));
			break;
		/*
		 *	Add an Array.
		 */
		case AifEnAddContainer:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			dev->fsa_dev[container].config_needed = ADD;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		/*
		 *	Delete an Array.
		 */
		case AifEnDeleteContainer:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			dev->fsa_dev[container].config_needed = DELETE;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		/*
		 *	Container change detected. If we currently are not
		 * waiting on something else, setup to wait on a Config Change.
		 */
		case AifEnContainerChange:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if (container >= dev->maximum_num_containers)
				break;
			if (dev->fsa_dev[container].config_waiting_on &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				break;
			dev->fsa_dev[container].config_needed = CHANGE;
			dev->fsa_dev[container].config_waiting_on =
				AifEnConfigChange;
			dev->fsa_dev[container].config_waiting_stamp = jiffies;
			break;

		case AifEnConfigChange:
			break;

		case AifEnAddJBOD:
		case AifEnDeleteJBOD:
			container = le32_to_cpu(((__le32 *)aifcmd->data)[1]);
			if ((container >> 28)) {
				container = (u32)-1;
				break;
			}
			channel = (container >> 24) & 0xF;
			if (channel >= dev->maximum_num_channels) {
				container = (u32)-1;
				break;
			}
			id = container & 0xFFFF;
			if (id >= dev->maximum_num_physicals) {
				container = (u32)-1;
				break;
			}
			lun = (container >> 16) & 0xFF;
			container = (u32)-1;
			channel = aac_phys_to_logical(channel);
			device_config_needed =
			  (((__le32 *)aifcmd->data)[0] ==
			    cpu_to_le32(AifEnAddJBOD)) ? ADD : DELETE;

			/*	ADPml11423: JBOD created in Redhat 5.3 OS are not available until System Reboot
			 *  After JBOD creation, Dynamic updation of scsi_device table was not handled earlier
			 *  Below code is to reinitialize scsi device, After creation of JBOD
			 *  Device structure is freshly initialized and discovered the same, 'fdisk -l' lists JBOD
			 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3)) 
			if (device_config_needed == ADD) {
				device = scsi_device_lookup(dev->scsi_host_ptr, channel, id, lun);
				if (device) {
					scsi_remove_device(device);
					scsi_device_put(device);
				}
			}
#endif	
			break;

		case AifEnEnclosureManagement:

			switch (le32_to_cpu(((__le32 *)aifcmd->data)[3])) {
			case EM_DRIVE_INSERTION:
			case EM_DRIVE_REMOVAL:
			case EM_SES_DRIVE_INSERTION:
			case EM_SES_DRIVE_REMOVAL:
				container = le32_to_cpu(
					((__le32 *)aifcmd->data)[2]);
				adbg_aif(dev,KERN_INFO,"dev_t=%x(%d,%d,%d,%d)\n",
				  container,
				  (dev && dev->scsi_host_ptr)
				    ? dev->scsi_host_ptr->host_no
				    : -1,
				  aac_phys_to_logical((container >> 24) & 0xF),
				  container & 0xFFF,
				  (container >> 16) & 0xFF);
				if ((container >> 28)) {
					container = (u32)-1;
					break;
				}
				channel = (container >> 24) & 0xF;
				if (channel >= dev->maximum_num_channels) {
					container = (u32)-1;
					break;
				}
				id = container & 0xFFFF;
				lun = (container >> 16) & 0xFF;
				container = (u32)-1;
				if (id >= dev->maximum_num_physicals) {
					/* legacy dev_t ? */
					if ((0x2000 <= id) || lun || channel ||
					  ((channel = (id >> 7) & 0x3F) >=
					  dev->maximum_num_channels))
						break;
					lun = (id >> 4) & 7;
					id &= 0xF;
				}
				channel = aac_phys_to_logical(channel);
				device_config_needed =
				  ((((__le32 *)aifcmd->data)[3]
				    == cpu_to_le32(EM_DRIVE_INSERTION)) ||
				    (((__le32 *)aifcmd->data)[3]
				    == cpu_to_le32(EM_SES_DRIVE_INSERTION))) ?
				    ADD : DELETE;
				break;
			}
				break;
			case AifBuManagerEvent:
				switch (le32_to_cpu(((__le32 *)aifcmd->data)[1])){
				case AifBuCacheDataLoss:
					if(le32_to_cpu(((__le32 *)aifcmd->data)[2]))
						aac_info (dev, "Backup unit had a cache data loss. Reason-code[%d]\n", le32_to_cpu(((__le32 *)aifcmd->data)[2]));
					else
						aac_info (dev, "Backup unit had a cache data loss.\n");
					break;
				case AifBuCacheDataRecover:
					if(le32_to_cpu(((__le32 *)aifcmd->data)[2]))
						aac_info(dev, "DDR cache data recovered successfully during power on.Reason-code[%d]\n", le32_to_cpu(((__le32 *)aifcmd->data)[2]));
					else
						aac_info(dev, "DDR cache data recovered successfully during power on.\n");
					break;
				}
			break;
		}

		/*
		 *	If we are waiting on something and this happens to be
		 * that thing then set the re-configure flag.
		 */
		if (container != (u32)-1) {
			if (container >= dev->maximum_num_containers)
				break;
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		} else for (container = 0;
		    container < dev->maximum_num_containers; ++container) {
			if ((dev->fsa_dev[container].config_waiting_on ==
			    le32_to_cpu(*(__le32 *)aifcmd->data)) &&
			 time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT))
				dev->fsa_dev[container].config_waiting_on = 0;
		}
		break;

	case AifCmdJobProgress:
		/*
		 *	These are job progress AIF's. When a Clear is being
		 * done on a container it is initially created then hidden from
		 * the OS. When the clear completes we don't get a config
		 * change so we monitor the job status complete on a clear then
		 * wait for a container change.
		 */
		adbg_aif(dev,KERN_INFO,
		  "aac_handle_aif: Aif command=AifCmdJobProgress job=%x [4]=%x [5]=%x [6]=%x\n",
		  le32_to_cpu(((__le32 *)aifcmd->data)[1]),
		  le32_to_cpu(((__le32 *)aifcmd->data)[4]),
		  le32_to_cpu(((__le32 *)aifcmd->data)[5]),
		  le32_to_cpu(((__le32 *)aifcmd->data)[6]));

		if (((__le32 *)aifcmd->data)[1] == cpu_to_le32(AifJobCtrZero) &&
		    (((__le32 *)aifcmd->data)[6] == ((__le32 *)aifcmd->data)[5] ||
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsSuccess) || 
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsAborted) || 
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsFailed) || 
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsPreempted) || 
		     ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsPended) )) {
			for (container = 0;
			    container < dev->maximum_num_containers;
			    ++container) {
				/*
				 * Stomp on all config sequencing for all
				 * containers?
				 */
				dev->fsa_dev[container].config_waiting_on =
					AifEnContainerChange;
				dev->fsa_dev[container].config_needed = ADD;
				dev->fsa_dev[container].config_waiting_stamp =
					jiffies;
			}
			adbg_aif(dev,KERN_INFO,
			  "aac_handle_aif: Wait=AifEnContainerChange ADD\n");
		}
		if (((__le32 *)aifcmd->data)[1] == cpu_to_le32(AifJobCtrZero) &&
		    ((__le32 *)aifcmd->data)[6] == 0 &&
		    ((__le32 *)aifcmd->data)[4] == cpu_to_le32(AifJobStsRunning)) {
			for (container = 0;
			    container < dev->maximum_num_containers;
			    ++container) {
				/*
				 * Stomp on all config sequencing for all
				 * containers?
				 */
				dev->fsa_dev[container].config_waiting_on =
					AifEnContainerChange;
				dev->fsa_dev[container].config_needed = DELETE;
				dev->fsa_dev[container].config_waiting_stamp =
					jiffies;
			}
			adbg_aif(dev,KERN_INFO,
			  "aac_handle_aif: Wait=AifEnContainerChange DELETE\n");
		}
		break;
	}

//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE))) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__)
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE)))
	container = 0;
//#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
retry_next_on_busy:
//#endif
	if (device_config_needed == NOTHING)
	for (; container < dev->maximum_num_containers; ++container) {
#else
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3))
	if (device_config_needed == NOTHING)
	for (container = 0; container < dev->maximum_num_containers;
	    ++container) {
#else
	container = 0;
retry_next:
	if (device_config_needed == NOTHING)
	for (; container < dev->maximum_num_containers; ++container) {
#endif
#endif
	
		if ((dev->fsa_dev[container].config_waiting_on == 0) &&
			(dev->fsa_dev[container].config_needed != NOTHING) &&
			time_before(jiffies, dev->fsa_dev[container].config_waiting_stamp + AIF_SNIFF_TIMEOUT)) {
			device_config_needed =
				dev->fsa_dev[container].config_needed;
			dev->fsa_dev[container].config_needed = NOTHING;
			channel = CONTAINER_TO_CHANNEL(container);
			id = CONTAINER_TO_ID(container);
			lun = CONTAINER_TO_LUN(container);
			break;
		}
	}
	if (device_config_needed == NOTHING)
		return;

	/*
	 *	If we decided that a re-configuration needs to be done,
	 * schedule it here on the way out the door, please close the door
	 * behind you.
	 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3))

	busy = 0;

#endif
	adbg_aif(dev,KERN_INFO,"id=(%d,%d,%d,%d)\n", (dev && dev->scsi_host_ptr) ?
	  dev->scsi_host_ptr->host_no : -1,
	  channel, id, lun);

	/*
	 *	Find the scsi_device associated with the SCSI address,
	 * and mark it as changed, invalidating the cache. This deals
	 * with changes to existing device IDs.
	 */

	if (!dev || !dev->scsi_host_ptr)
		return;
	/*
	 * force reload of disk info via aac_probe_container
	 */
	if ((channel == CONTAINER_CHANNEL) &&
	  (device_config_needed != NOTHING)) {
		if (dev->fsa_dev[container].valid == 1)
			dev->fsa_dev[container].valid = 2;
		aac_probe_container(dev, container);
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,3))
	device = scsi_device_lookup(dev->scsi_host_ptr, channel, id, lun);
        adbg_aif(dev,KERN_INFO,"scsi_device_lookup(%p,%d,%d,%d)=%p %s %s\n",
	  dev->scsi_host_ptr, channel, id, lun, device,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
	  ((busy) ? "BUSY" : "AVAILABLE"),
#else
	  "",
#endif
	  (device_config_needed == NOTHING)
	   ? "NOTHING"
	   : (device_config_needed == DELETE)
	     ? "DELETE"
	     : (device_config_needed == ADD)
	       ? "ADD"
	       : (device_config_needed == CHANGE)
		 ? "CHANGE"
		 : "UNKNOWN");
	if (device) {
		switch (device_config_needed) {
		case DELETE:
			if (aac_remove_devnodes > 0) {
				/* Bug in sysfs removing then adding devices quickly */
				scsi_remove_device(device);
			} else {
				if (scsi_device_online(device)) {
					scsi_device_set_state(device, SDEV_OFFLINE);
					sdev_printk(KERN_INFO, device,
						"Device offlined - %s\n",
						(channel == CONTAINER_CHANNEL) ?
							"array deleted" :
							"enclosure services event");
				}
			}
			break;
		case ADD:
			if (!scsi_device_online(device)) {
				sdev_printk(KERN_INFO, device,
					"Device online - %s\n",
					(channel == CONTAINER_CHANNEL) ?
						"array created" :
						"enclosure services event");
				scsi_device_set_state(device, SDEV_RUNNING);
			}
			/* FALLTHRU */
		case CHANGE:
			if ((channel == CONTAINER_CHANNEL)
			 && (!dev->fsa_dev[container].valid)) {
				if (aac_remove_devnodes > 0)
					scsi_remove_device(device);
				else {
					if (!scsi_device_online(device))
						break;
					scsi_device_set_state(device, SDEV_OFFLINE);
					sdev_printk(KERN_INFO, device,
						"Device offlined - %s\n",
						"array failed");
				}
				break;
			}
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)) || !defined(MODULE))
			scsi_rescan_device(&device->sdev_gendev);
#else
			if (!device->sdev_gendev.driver)
				break;
			drv = to_scsi_driver(
				device->sdev_gendev.driver);
			if (!try_module_get(drv->owner))
				break;
			/* scsi_rescan_device code fragment */
			if(drv->rescan)
				drv->rescan(&device->sdev_gendev);
			module_put(drv->owner);
#endif

		default:
			break;
		}
		scsi_device_put(device);
		device_config_needed = NOTHING;
	}
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	shost_for_each_device(device, dev->scsi_host_ptr)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	list_for_each_entry(device, &dev->scsi_host_ptr->my_devices, siblings)
#else
	for (device = dev->scsi_host_ptr->host_queue;
	  device != (struct scsi_device *)NULL;
	  device = device->next)
#endif
	{
	        adbg_aif(dev,KERN_INFO, "aifd: device (%d,%d,%d,%d)?\n",
		        dev->scsi_host_ptr->host_no, device->channel, device->id,
		        device->lun);
	        if ((device->channel == channel)
	                && (device->id == id)
	                && (device->lun == lun)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
			busy |= atomic_read(&device->access_count) ||
				test_bit(SHOST_RECOVERY,
				(const unsigned long*)&dev->scsi_host_ptr->shost_state) ||
				dev->scsi_host_ptr->eh_active;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)) || defined(SCSI_HAS_SHOST_STATE_ENUM))
			busy |= device->device_busy ||
				(SHOST_RECOVERY == device->host->shost_state);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14))
			busy |= device->device_busy ||
				test_bit(SHOST_RECOVERY,
				(const unsigned long*)&dev->scsi_host_ptr->shost_state);
#else
			busy |= device->device_busy ||
				test_bit(SHOST_RECOVERY,
				(const unsigned long*)&dev->scsi_host_ptr->shost_state) ||
				dev->scsi_host_ptr->eh_active;
#endif
#else
			busy |= device->access_count ||
				dev->scsi_host_ptr->in_recovery ||
				dev->scsi_host_ptr->eh_active;
#endif
		    adbg_aif(dev,KERN_INFO, " %s %s\n",
			  ((busy) ? "BUSY" : "AVAILABLE"),
			  (device_config_needed == NOTHING)
			   ? "NOTHING"
			   : (device_config_needed == DELETE)
			     ? "DELETE"
			     : (device_config_needed == ADD)
			       ? "ADD"
			       : (device_config_needed == CHANGE)
				 ? "CHANGE"
				 : "UNKNOWN");
			if (busy == 0) {
				device->removable = aac_removable;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
				switch (device_config_needed) {
#if 0
				case ADD:
					/*
					 *	No need to call
					 * scsi_scan_single_target
					 */
					device_config_needed = CHANGE;
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
					adbg_aif(dev,KERN_INFO
					  "scsi_add_device(%p{%d}, %d, %d, %d)\n",
					  dev->scsi_host_ptr,
					  dev->scsi_host_ptr->host_no,
					  device->channel, device->id,
					  device->lun);
					scsi_add_device(dev->scsi_host_ptr,
					  device->channel, device->id,
					  device->lun);
					break;
#endif
#endif
				case DELETE:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
				    adbg_aif(dev,KERN_INFO,
					  "scsi_remove_device(%p{%d:%d:%d:%d})\n",
					  device, device->host->host_no,
					  device->channel, device->id,
					  device->lun);
					scsi_remove_device(device);
					break;
#endif
				case CHANGE:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
					if ((channel == CONTAINER_CHANNEL) &&
					  !dev->fsa_dev[container].valid) {
						adbg_aif(dev,KERN_INFO,
						  "scsi_remove_device(%p{%d:%d:%d:%d})\n",
						  device,
						  device->host->host_no,
						  device->channel, device->id,
						  device->lun);
						scsi_remove_device(device);
						break;
					}
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)) || !defined(MODULE))
					adbg_aif(dev,KERN_INFO,
					  "scsi_rescan_device(&%p{%d:%d:%d:%d}->sdev_gendev)\n",
					  device, device->host->host_no,
					  device->channel, device->id,
					  device->lun);
					scsi_rescan_device(&device->sdev_gendev);
#else
					if (!device->sdev_gendev.driver)
						break;
					drv = to_scsi_driver(
						device->sdev_gendev.driver);
					if (!try_module_get(drv->owner))
						break;
					adbg_aif(dev,KERN_INFO,
					  "drv->rescan{%p}(&%p{%d:%d:%d:%d}->sdev_gendev)\n",
					  drv->rescan, device,
					  device->host->host_no,
					  device->channel, device->id,
					  device->lun);
					/* scsi_rescan_device code fragment */
					if(drv->rescan)
						drv->rescan(&device->sdev_gendev);
					module_put(drv->owner);
#endif

				default:
					break;
				}
#endif
			}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
			scsi_device_put(device);
#endif
			break;
		}
	}
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
	if (device_config_needed == ADD)
	{
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || !defined(MODULE))
		adbg_aif(dev,KERN_INFO,
		  "scsi_add_device(%p{%d}, %d, %d, %d)\n",
		  dev->scsi_host_ptr, dev->scsi_host_ptr->host_no,
		  channel, id, lun);
		scsi_add_device(dev->scsi_host_ptr, channel, id, lun);
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10))
		adbg_aif(dev,KERN_INFO,
		  "scsi_scan_single_target(%p{%d}, %d, %d)\n",
		  dev->scsi_host_ptr, dev->scsi_host_ptr->host_no,
		  channel, id);
		scsi_scan_single_target(dev->scsi_host_ptr, channel, id);
#elif (!defined(MODULE))
		adbg_aif(dev,KERN_INFO,
		  "scsi_scan_host_selected(%p{%d}, %d, %d, %d, 0)\n",
		  dev->scsi_host_ptr, dev->scsi_host_ptr->host_no,
		  channel, id, lun);
		scsi_scan_host_selected(dev->scsi_host_ptr, channel, id, lun, 0);
#else
		;
#endif
    }
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3))
    adbg_aif(dev,KERN_INFO ,"busy=%d\n", busy);
#endif

//#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE))) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__)
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,3)) && defined(MODULE)))
	/*
	 * if (busy == 0) {
	 *	scan_scsis(dev->scsi_host_ptr, 1,
	 *	  CONTAINER_TO_CHANNEL(container),
	 *	  CONTAINER_TO_ID(container),
	 *	  CONTAINER_TO_LUN(container));
	 * }
	 * is not exported as accessible, so we need to go around it
	 * another way. So, we look for the "proc/scsi/scsi" entry in
	 * the proc filesystem (using proc_scsi as a shortcut) and send
	 * it a message. This deals with new devices that have
	 * appeared. If the device has gone offline, scan_scsis will
	 * also discover this, but we do not want the device to
	 * go away. We need to check the access_count for the
	 * device since we are not wanting the devices to go away.
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	if (device_config_needed != NOTHING)
#endif
	if ((channel == CONTAINER_CHANNEL) && busy) {
		dev->fsa_dev[container].config_waiting_on = 0;
		dev->fsa_dev[container].config_needed = device_config_needed;
		/*
		 * Jump back and check if any other containers are ready for
		 * processing.
		 */
		container++;
		device_config_needed = NOTHING;
		goto retry_next_on_busy;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	if (device_config_needed != NOTHING)
#endif
	if (proc_scsi != (struct proc_dir_entry *)NULL) {
		struct proc_dir_entry * entry;

		adbg_aif(dev,KERN_INFO, "proc_scsi=%p ", proc_scsi);
		for (entry = proc_scsi->subdir;
		  entry != (struct proc_dir_entry *)NULL;
		  entry = entry->next) {
			adbg_aif(dev,KERN_INFO,"\"%.*s\"[%d]=%x ", entry->namelen,
			  entry->name, entry->namelen, entry->low_ino);
			if ((entry->low_ino != 0)
			 && (entry->namelen == 4)
			 && (memcmp ("scsi", entry->name, 4) == 0)) {
				adbg_aif(dev,KERN_INFO,"%p->write_proc=%p ", entry, entry->write_proc);
				if (entry->write_proc != (int (*)(struct file *, const char *, unsigned long, void *))NULL) {
					char buffer[80];
					int length;
					mm_segment_t fs;

					sprintf (buffer,
					  "scsi %s-single-device %d %d %d %d\n",
					  ((device_config_needed == DELETE)
					   ? "remove"
					   : "add"),
					  dev->scsi_host_ptr->host_no,
					  channel, id, lun);
					length = strlen (buffer);
					adbg_aif(dev,KERN_INFO,
					  "echo %.*s > /proc/scsi/scsi\n",
					  length-1, buffer);
					fs = get_fs();
					set_fs(get_ds());
					lock_kernel();
					length = entry->write_proc(
					  NULL, buffer, length, NULL);
					unlock_kernel();
					set_fs(fs);
				        adbg_aif(dev,KERN_INFO "returns %d\n",
					  length);
				}
				break;
			}
		}
	}
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
	if (channel == CONTAINER_CHANNEL) {
		container++;
		device_config_needed = NOTHING;
		goto retry_next;
	}
#endif
}

/*
 * send_wellness_command: this sends the wellness command to the firmware 
 *  	@dev  			- aacc_dev pointer
 *  	@wellness_str 	- the wellness string to be sent to firmware
 *  	@u32 datasize	- data size of the string
 */
static int send_wellness_command(struct aac_dev * dev, char *wellness_str, u32 datasize)
{
	struct aac_srb *srbcmd;
	struct aac_srb_unit srbu;
	char *dma_buf;
	int rcode = -ENOMEM;

	dma_buf = kzalloc(datasize, GFP_KERNEL);
	if (dma_buf == NULL) {
		aac_err(dev, "SEND WELLNESS buff allocation Failed\n");
		goto out;
	}

	memset(&srbu, 0, sizeof(struct aac_srb_unit));

	srbcmd = &srbu.srb;
	srbcmd->flags	= cpu_to_le32(SRB_DataOut);
	srbcmd->cdb[0]	= CISS_BMIC_DATA_OUT;
	srbcmd->cdb[6]	= CISS_WRITE_HOST_WELLNESS;

	/* move data to buffer*/
	memcpy(dma_buf, (char *)wellness_str, datasize);

	/* issue request to the controller */
	rcode = aac_send_safw_bmic_cmd(dev, &srbu, dma_buf, datasize);
	if(unlikely(rcode < 0)) {
		aac_err(dev, "SEND_WELLNESS failed-%d\n", rcode);
	}

out:
	kfree(dma_buf);
	return rcode;
}

void aac_schedule_bus_scan(struct aac_dev *aac)
{
    aac_info(aac, "Scheduling  bus rescan\n");
	if(aac->sa_firmware)
		aac_schedule_safw_scan_worker(aac);
	else
		aac_schedule_src_reinit_aif_worker(aac);
}
/**
 * Performs low level operations to perform controller
 * reset.
 * 	Return 0 if the controller is back in operational state.
 *  	else, the controller is DEAD and any further requests
 *  	should be blocked.
 */
static int _aac_reset_adapter(struct aac_dev *aac, int forced, u8 reset_type)
{
	int index, quirks;
	int retval;
	struct Scsi_Host *host;
	struct scsi_device *dev;
	struct scsi_cmnd *command;
	struct scsi_cmnd *command_list;
	int jafo = 0;
	int bled;

	/*
	 * Assumptions:
	 *	- host is locked, unless called by the aacraid thread.
	 *	  (a matter of convenience, due to legacy issues surrounding
	 *	  eh_host_adapter_reset).
	 *	- in_reset is asserted, so no new i/o is getting to the
	 *	  card.
	 *	- The card is dead, or will be very shortly ;-/ so no new
	 *	  commands are completing in the interrupt service.
	 */
	host = aac->scsi_host_ptr;
	scsi_block_requests(host);
	aac_cancel_workers(aac);
	aac_adapter_disable_int(aac);
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	if (aac->thread_pid != current->pid) {
		kill_proc(aac->thread_pid, SIGKILL, 0);
		/* Chance of sleeping in this context, must unlock */
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_unlock_irq(host->host_lock);
#else
		spin_unlock_irq(host->lock);
#endif
#else
		spin_unlock_irq(&io_request_lock);
#endif
		wait_for_completion(&aac->aif_completion);
		jafo = 1;
	}
#else
	if (aac->thread && aac->thread->pid != current->pid) {
        spin_unlock_irq(host->host_lock);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
        {
            struct task_struct *tsk=NULL;
            tsk = pid_task(find_vpid(aac->thread->pid), PIDTYPE_PID);
            if (tsk) {
                send_sig(SIGTERM, tsk, 1);
            }
        }
#endif
        retval = kthread_stop(aac->thread);
		if (retval != -EINTR) {
			aac_info (aac, "command thread terminated\n");
		}
		aac->thread = NULL;
	    jafo = 1;
	}
#endif

	/*
	 *	If a positive health, means in a known DEAD PANIC
	 * state and the adapter could be reset to `try again'.
	 */
	bled = forced ? 0 : aac_adapter_check_health(aac);
	retval = aac_adapter_restart(aac, bled, reset_type);
	
	if (retval)
		goto out;
	
	/*
	 *	Loop through the fibs, close the synchronous FIBS
	 */
	for (retval = 1, index = 0; index < (aac->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB); index++) {
		struct fib *fib = &aac->fibs[index];
		if ((!(fib->hw_fib_va->header.XferState & cpu_to_le32(NoResponseExpected | Async)) &&
		  (fib->hw_fib_va->header.XferState & cpu_to_le32(ResponseExpected))) || 
            fib->flags & FIB_CONTEXT_FLAG_WAIT) {
			unsigned long flagv;
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
			printk(KERN_INFO "returning FIB %p undone\n", fib);
#endif
			spin_lock_irqsave(&fib->event_lock, flagv);
			aac_complete(&fib->event_wait);
			spin_unlock_irqrestore(&fib->event_lock, flagv);
			schedule();
			retval = 0;
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
			printk(KERN_INFO "returned FIB %p undone\n", fib);
#endif
		}
	}
	/* Give some extra time for ioctls to complete. */
	if (retval == 0)
		ssleep(2);
	index = aac->cardtype;

	/*
	 * Re-initialize the adapter, first free resources, then carefully
	 * apply the initialization sequence to come back again. Only risk
	 * is a change in Firmware dropping cache, it is assumed the caller
	 * will ensure that i/o is queisced and the card is flushed in that
	 * case.
	 */
	aac_free_irq(aac);
	aac_fib_map_free(aac);
	aac_pci_free_consistent(aac->pdev, aac->comm_size, aac->comm_addr, aac->comm_phys);
	aac->comm_addr = NULL;
	aac->comm_phys = 0;
	kfree(aac->queues);
	aac->queues = NULL;
	kfree(aac->fsa_dev);
	aac->fsa_dev = NULL;
	quirks = aac_get_driver_ident(index)->quirks;
	/* Settup the dma and consistent dma mask */
	if (quirks & AAC_QUIRK_31BIT) {
		retval = pci_set_dma_mask(aac->pdev, DMA_BIT_MASK(32));
		if (!retval) {
			retval = pci_set_consistent_dma_mask(aac->pdev, DMA_BIT_MASK(31));
		}
	} else if (!(quirks & AAC_QUIRK_SRC)) {
		retval = pci_set_dma_mask(aac->pdev, DMA_BIT_MASK(32));
	} else {
		retval = pci_set_consistent_dma_mask(aac->pdev, DMA_BIT_MASK(32));
	}
	if (retval) 
		goto out;

#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Calling adapter init\n");
#endif
	if ((retval = (*(aac_get_driver_ident(index)->init))(aac)))
		goto out;

	if (jafo) {
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
		aac->thread_pid = retval = kernel_thread(
		  (int (*)(void *))aac_command_thread, aac, 0);
		if (retval < 0)
			goto out;
#else
		aac->thread = kthread_run(aac_command_thread, aac, aac->name);
		if (IS_ERR(aac->thread)) {
			retval = PTR_ERR(aac->thread);
			aac->thread = NULL;
			goto out;
		}
#endif
	}
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Acquiring adapter information\n");
#endif
	if(aac->sa_firmware) {
		char wellness_str[] = "<HW>PS\02\0\0\0ZZ";
		
		//copy the slot number
		wellness_str[8] = aac->physical_slot;
		//restore the slot number after a controller reset
		send_wellness_command(aac, wellness_str, sizeof(wellness_str));
	}
	(void)aac_get_adapter_info(aac);
	if ((quirks & AAC_QUIRK_34SG) && (host->sg_tablesize > 34)) {
		host->sg_tablesize = 34;
		host->max_sectors = (host->sg_tablesize * 8) + 112;
	}
	if ((quirks & AAC_QUIRK_17SG) && (host->sg_tablesize > 17)) {
		host->sg_tablesize = 17;
		host->max_sectors = (host->sg_tablesize * 8) + 112;
	}
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Determine the configuration status\n");
#endif
	aac_get_config_status(aac, 1);
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Probing all arrays to confirm status\n");
#endif
	aac_get_containers(aac);
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Completing all outstanding driver commands as BUSY\n");
#endif
	/*
	 * This is where the assumption that the Adapter is quiesced
	 * is important.
	 */
	command_list = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	__shost_for_each_device(dev, host) {
		unsigned long flags;
		spin_lock_irqsave(&dev->list_lock, flags);
		list_for_each_entry(command, &dev->cmd_list, list)
			if (command->SCp.phase == AAC_OWNER_FIRMWARE) {
				command->SCp.buffer = (struct scatterlist *)command_list;
				command_list = command;
			}
		spin_unlock_irqrestore(&dev->list_lock, flags);
	}
#else
#ifndef SAM_STAT_TASK_SET_FULL
# define SAM_STAT_TASK_SET_FULL (QUEUE_FULL << 1)
#endif
	for (dev = host->host_queue; dev != (struct scsi_device *)NULL; dev = dev->next)
		for(command = dev->device_queue; command; command = command->next)
			if (command->SCp.phase == AAC_OWNER_FIRMWARE) {
				command->SCp.buffer = (struct scatterlist *)command_list;
				command_list = command;
			}
#endif
	while ((command = command_list)) {
#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
		printk(KERN_INFO "returning %p for retry\n", command);
#endif
		command_list = (struct scsi_cmnd *)command->SCp.buffer;
		command->SCp.buffer = NULL;
		command->result = DID_OK << 16
		  | COMMAND_COMPLETE << 8
		  | SAM_STAT_TASK_SET_FULL;
		command->SCp.phase = AAC_OWNER_ERROR_HANDLER;
		command->scsi_done(command);
	}

/**
* Any device that was already marked offline needs to be marked running
*/
	shost_for_each_device(dev, host) {
		if(!scsi_device_online(dev)) {
			scsi_device_set_state(dev, SDEV_RUNNING);
		}
	}

#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	printk(KERN_INFO "Continue where we left off\n");
#endif
	retval = 0;

out:
	aac->in_reset = 0;

	scsi_unblock_requests(host);

	if(retval == 0)
		aac_schedule_bus_scan(aac);

	if (jafo) {
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
		spin_lock_irq(host->host_lock);
#else
		spin_lock_irq(host->lock);
#endif
#else
		spin_lock_irq(&io_request_lock);
#endif
	}
	return retval;
}



int aac_reset_adapter(struct aac_dev * aac, int forced, u8 reset_type)
{
	unsigned long flagv = 0;
	int retval;
	struct Scsi_Host * host;
	int bled;

	if (spin_trylock_irqsave(&aac->fib_lock, flagv) == 0)
		return -EBUSY;

	if (aac->in_reset) {
		spin_unlock_irqrestore(&aac->fib_lock, flagv);
		return -EBUSY;
	}
	aac->in_reset = 1;
	spin_unlock_irqrestore(&aac->fib_lock, flagv);

	/*
	 * Wait for all commands to complete to this specific
	 * target (block maximum 60 seconds). Although not necessary,
	 * it does make us a good storage citizen.
	 */
	host = aac->scsi_host_ptr;
	scsi_block_requests(host);
	aac_cancel_workers(aac);

	/* Quiesce build, flush cache, write through mode */
	if (forced < 2)
		aac_send_shutdown(aac);
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
	spin_lock_irqsave(host->host_lock, flagv);
#else
	spin_lock_irqsave(host->lock, flagv);
#endif
#else
	spin_lock_irqsave(&io_request_lock, flagv);
#endif
	bled = forced ? forced : aac_adapter_check_health(aac);
	retval = _aac_reset_adapter(aac, bled, reset_type);
#if (defined(SCSI_HAS_HOST_LOCK) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,21)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,4,21)) || !defined(CONFIG_CFGNAME))
	spin_unlock_irqrestore(host->host_lock, flagv);
#else
	spin_unlock_irqrestore(host->lock, flagv);
#endif
#else
	spin_unlock_irqrestore(&io_request_lock, flagv);
#endif

	if ((forced < 2) && (retval == -ENODEV)) {
		/* Unwind aac_send_shutdown() IOP_RESET unsupported/disabled */
		struct fib *fibctx = aac_fib_alloc(aac, NULL);
		if (fibctx) {
			struct aac_pause *cmd;
			int status;

			aac_fib_init(fibctx);

			cmd = (struct aac_pause *) fib_data(fibctx);

			cmd->command = cpu_to_le32(VM_ContainerConfig);
			cmd->type = cpu_to_le32(CT_PAUSE_IO);
			cmd->timeout = cpu_to_le32(1);
			cmd->min = cpu_to_le32(1);
			cmd->noRescan = cpu_to_le32(1);
			cmd->count = cpu_to_le32(0);

			status = aac_fib_send(ContainerCommand,
			  fibctx,
			  sizeof(struct aac_pause),
			  FsaNormal,
			  -2 /* Timeout silently */, 1,
			  NULL, NULL);

			if (status >= 0)
				aac_fib_complete(fibctx);
			/* FIB should be freed only after getting
			 * the response from the F/W */
			if (status != -ERESTARTSYS)
				aac_fib_free(fibctx);
		}
	}

	return retval;
}

int aac_check_health(struct aac_dev * aac)
{
	int BlinkLED;
#if (!defined(HAS_BOOT_CONFIG))
	unsigned long time_now, flagv = 0;
	struct list_head * entry;
#else
	unsigned long flagv = 0;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13))
	struct Scsi_Host * host;
#endif
	int bled;
	
	/* Extending the scope of fib_lock slightly to protect aac->in_reset */
	if (spin_trylock_irqsave(&aac->fib_lock, flagv) == 0)
		return 0;

	if (aac->in_reset || !(BlinkLED = aac_adapter_check_health(aac))) {
		spin_unlock_irqrestore(&aac->fib_lock, flagv);
		return 0; /* OK */
	}

	aac->in_reset = 1;

#if (!defined(HAS_BOOT_CONFIG))
	/* Fake up an AIF:
	 *	aac_aifcmd.command = AifCmdEventNotify = 1
	 *	aac_aifcmd.seqnum = 0xFFFFFFFF
	 *	aac_aifcmd.data[0] = AifEnExpEvent = 23
	 *	aac_aifcmd.data[1] = AifExeFirmwarePanic = 3
	 *	aac.aifcmd.data[2] = AifHighPriority = 3
	 *	aac.aifcmd.data[3] = BlinkLED
	 */

	time_now = jiffies/HZ;
	entry = aac->fib_list.next;

	/*
	 * For each Context that is on the
	 * fibctxList, make a copy of the
	 * fib, and then set the event to wake up the
	 * thread that is waiting for it.
	 */
	while (entry != &aac->fib_list) {
		/*
		 * Extract the fibctx
		 */
		struct aac_fib_context *fibctx = list_entry(entry, struct aac_fib_context, next);
		struct hw_fib * hw_fib;
		struct fib * fib;
		/*
		 * Check if the queue is getting
		 * backlogged
		 */
		if (fibctx->count > 20) {
			/*
			 * It's *not* jiffies folks,
			 * but jiffies / HZ, so do not
			 * panic ...
			 */
			u32 time_last = fibctx->jiffies;
			/*
			 * Has it been > 2 minutes
			 * since the last read off
			 * the queue?
			 */
			if ((time_now - time_last) > aif_timeout) {
				entry = entry->next;
				aac_close_fib_context(aac, fibctx);
				continue;
			}
		}
		/*
		 * Warning: no sleep allowed while
		 * holding spinlock
		 */
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
		hw_fib = kmalloc(sizeof(struct hw_fib), GFP_ATOMIC);
		fib = kmalloc(sizeof(struct fib), GFP_ATOMIC);
#else
		hw_fib = kzalloc(sizeof(struct hw_fib), GFP_ATOMIC);
		fib = kzalloc(sizeof(struct fib), GFP_ATOMIC);
#endif
		if (fib && hw_fib) {
			struct aac_aifcmd * aif;

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) && !defined(HAS_KZALLOC))
			memset(hw_fib, 0, sizeof(struct hw_fib));
			memset(fib, 0, sizeof(struct fib));
#endif
			fib->hw_fib_va = hw_fib;
			fib->dev = aac;
			aac_fib_init(fib);
			fib->type = FSAFS_NTC_FIB_CONTEXT;
			fib->size = sizeof (struct fib);
			fib->data = hw_fib->data;
			INIT_LIST_HEAD(&fib->fiblink);
			aif = (struct aac_aifcmd *)hw_fib->data;
			aif->command = cpu_to_le32(AifCmdEventNotify);
			aif->seqnum = cpu_to_le32(0xFFFFFFFF);
			((__le32 *)aif->data)[0] = cpu_to_le32(AifEnExpEvent);
			((__le32 *)aif->data)[1] = cpu_to_le32(AifExeFirmwarePanic);
			((__le32 *)aif->data)[2] = cpu_to_le32(AifHighPriority);
			((__le32 *)aif->data)[3] = cpu_to_le32(BlinkLED);

			/*
			 * Put the FIB onto the
			 * fibctx's fibs
			 */
			list_add_tail(&fib->fiblink, &fibctx->fib_list);
			fibctx->count++;
			/*
			 * Set the event to wake up the
			 * thread that will waiting.
			 */
			aac_complete(&fibctx->wait_completion);
		} else {
			printk(KERN_WARNING "aifd: didn't allocate NewFib.\n");
			kfree(fib);
			kfree(hw_fib);
		}
		entry = entry->next;
	}
#endif

	spin_unlock_irqrestore(&aac->fib_lock, flagv);

	if (BlinkLED < 0) {
		aac_err(aac," Host adapter dead %d\n", BlinkLED);
		goto out;
	}

        aac_err(aac,"Host adapter BLINK LED 0x%x\n", BlinkLED);

	if (!aac_check_reset || ((aac_check_reset == 1) &&
		(aac->supplement_adapter_info.supported_options2 &
			AAC_OPTION_IGNORE_RESET)))
		goto out;

	bled = aac_check_reset != 1 ? 1 : 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13))
	host = aac->scsi_host_ptr;
	if (aac->thread && (aac->thread->pid != current->pid))
		spin_lock_irqsave(host->host_lock, flagv);
	BlinkLED = _aac_reset_adapter(aac, bled, IOP_HWSOFT_RESET);
	if (aac->thread && (aac->thread->pid != current->pid))
		spin_unlock_irqrestore(host->host_lock, flagv);
	return BlinkLED;
#else
	return _aac_reset_adapter(aac, bled, IOP_HWSOFT_RESET);
#endif

out:
	aac->in_reset = 0;
	return BlinkLED;
}
static int get_fib_count(struct aac_dev *dev)
{
	unsigned int num = 0;
	struct list_head *entry;
	unsigned long flagv;

	/*
	 * Warning: no sleep allowed while
	 * holding spinlock. We take the estimate
	 * and pre-allocate a set of fibs outside the
	 * lock.
	 */
	num = le32_to_cpu(dev->init->r7.AdapterFibsSize)
			/ sizeof(struct hw_fib); /* some extra */
	spin_lock_irqsave(&dev->fib_lock, flagv);
	entry = dev->fib_list.next;
	while (entry != &dev->fib_list) {
		entry = entry->next;
		++num;
	}
    spin_unlock_irqrestore(&dev->fib_lock, flagv);

	return num;
}

static int fillup_pools(struct aac_dev *dev, struct hw_fib **hw_fib_pool,
						struct fib **fib_pool,
						unsigned int num)
{
	struct hw_fib **hw_fib_p;
	struct fib **fib_p;
	int rcode = 1;

	hw_fib_p = hw_fib_pool;
	fib_p = fib_pool;
	while (hw_fib_p < &hw_fib_pool[num]) {
		*(hw_fib_p) = kmalloc(sizeof(struct hw_fib), GFP_KERNEL);
		if (!(*(hw_fib_p++))) {
			--hw_fib_p;
			break;
		}

		*(fib_p) = kmalloc(sizeof(struct fib), GFP_KERNEL);
		if (!(*(fib_p++))) {
			kfree(*(--hw_fib_p));
			break;
		}
	}

	num = hw_fib_p - hw_fib_pool;
	if (!num)
		rcode = 0;

	return rcode;
}

static void wakeup_fibctx_threads(struct aac_dev *dev,
						struct hw_fib **hw_fib_pool,
						struct fib **fib_pool,
						struct fib *fib,
						struct hw_fib *hw_fib,
						unsigned int num)
{
	unsigned long flagv;
	struct list_head *entry;
	struct hw_fib **hw_fib_p;
	struct fib **fib_p;
	u32 time_now, time_last;
	struct hw_fib *hw_newfib;
	struct fib *newfib;
	struct aac_fib_context *fibctx;

	time_now = jiffies/HZ;
	spin_lock_irqsave(&dev->fib_lock, flagv);
	entry = dev->fib_list.next;
	/*
	 * For each Context that is on the
	 * fibctxList, make a copy of the
	 * fib, and then set the event to wake up the
	 * thread that is waiting for it.
	 */

	hw_fib_p = hw_fib_pool;
	fib_p = fib_pool;
	while (entry != &dev->fib_list) {
		/*
		 * Extract the fibctx
		 */
		fibctx = list_entry(entry, struct aac_fib_context,
				next);
		/*
		 * Check if the queue is getting
		 * backlogged
		 */
		if (fibctx->count > 20) {
			/*
			 * It's *not* jiffies folks,
			 * but jiffies / HZ so do not
			 * panic ...
			 */
			time_last = fibctx->jiffies;
			/*
			 * Has it been > 2 minutes
			 * since the last read off
			 * the queue?
			 */
			if ((time_now - time_last) > aif_timeout) {
				entry = entry->next;
				aac_close_fib_context(dev, fibctx);
				continue;
			}
		}
		/*
		 * Warning: no sleep allowed while
		 * holding spinlock
		 */
		if (hw_fib_p >= &hw_fib_pool[num]) {
			aac_warn(dev, "aifd: didn't allocate NewFib\n");
			entry = entry->next;
			continue;
		}

		hw_newfib = *hw_fib_p;
		*(hw_fib_p++) = NULL;
		newfib = *fib_p;
		*(fib_p++) = NULL;
		/*
		 * Make the copy of the FIB
		 */
		memcpy(hw_newfib, hw_fib, sizeof(struct hw_fib));
		memcpy(newfib, fib, sizeof(struct fib));
		newfib->hw_fib_va = hw_newfib;
		/*
		 * Put the FIB onto the
		 * fibctx's fibs
		 */
		list_add_tail(&newfib->fiblink, &fibctx->fib_list);
		fibctx->count++;
		/*
		 * Set the event to wake up the
		 * thread that is waiting.
		 */
		aac_complete(&fibctx->wait_completion);

		entry = entry->next;
	}
	/*
	 *	Set the status of this FIB
	 */
	*(__le32 *)hw_fib->data = cpu_to_le32(ST_OK);
	aac_fib_adapter_complete(fib, sizeof(u32));
	spin_unlock_irqrestore(&dev->fib_lock, flagv);

}

static void aac_process_events(struct aac_dev *dev)
{
	struct hw_fib *hw_fib;
	struct fib *fib;
	unsigned long flags;
	spinlock_t *t_lock;
	unsigned int rcode;

	t_lock = dev->queues->queue[HostNormCmdQueue].lock;
	spin_lock_irqsave(t_lock, flags);

	while (!list_empty(&(dev->queues->queue[HostNormCmdQueue].cmdq))) {
		struct list_head *entry;
		struct aac_aifcmd *aifcmd;
		unsigned int  num;
		struct hw_fib **hw_fib_pool, **hw_fib_p;
		struct fib **fib_pool, **fib_p;

		set_current_state(TASK_RUNNING);

		entry = dev->queues->queue[HostNormCmdQueue].cmdq.next;
		list_del(entry);

		t_lock = dev->queues->queue[HostNormCmdQueue].lock;
		spin_unlock_irqrestore(t_lock, flags);

		fib = list_entry(entry, struct fib, fiblink);
		hw_fib = fib->hw_fib_va;
		if (dev->sa_firmware) {
			/* Thor AIF */
			aac_handle_sa_aif(dev, fib);
			aac_fib_adapter_complete(fib, (u16)sizeof(u32));
			goto free_fib;
		}
		/*
		 *	We will process the FIB here or pass it to a
		 *	worker thread that is TBD. We Really can't
		 *	do anything at this point since we don't have
		 *	anything defined for this thread to do.
		 */
		memset(fib, 0, sizeof(struct fib));
		fib->type = FSAFS_NTC_FIB_CONTEXT;
		fib->size = sizeof(struct fib);
		fib->hw_fib_va = hw_fib;
		fib->data = hw_fib->data;
		fib->dev = dev;
		/*
		 *	We only handle AifRequest fibs from the adapter.
		 */

		aifcmd = (struct aac_aifcmd *) hw_fib->data;
		if (aifcmd->command == cpu_to_le32(AifCmdDriverNotify)) {
			/* Handle Driver Notify Events */
			aac_handle_aif(dev, fib);
			*(__le32 *)hw_fib->data = cpu_to_le32(ST_OK);
			aac_fib_adapter_complete(fib, (u16)sizeof(u32));
			goto free_fib;
		}
		/*
		 * The u32 here is important and intended. We are using
		 * 32bit wrapping time to fit the adapter field
		 */

		/* Sniff events */
		if (aifcmd->command == cpu_to_le32(AifCmdEventNotify)
		 || aifcmd->command == cpu_to_le32(AifCmdJobProgress)) {
			aac_handle_aif(dev, fib);
		}

		/*
		 * get number of fibs to process
		 */
		num = get_fib_count(dev);
		if (!num)
			goto free_fib;

		hw_fib_pool = kmalloc_array(num, sizeof(struct hw_fib *),
						GFP_KERNEL);
		if (!hw_fib_pool)
			goto free_fib;

		fib_pool = kmalloc_array(num, sizeof(struct fib *), GFP_KERNEL);
		if (!fib_pool)
			goto free_hw_fib_pool;

		/*
		 * Fill up fib pointer pools with actual fibs
		 * and hw_fibs
		 */
		rcode = fillup_pools(dev, hw_fib_pool, fib_pool, num);
		if (!rcode)
			goto free_mem;

		/*
		 * wakeup the thread that is waiting for
		 * the response from fw (ioctl)
		 */
		wakeup_fibctx_threads(dev, hw_fib_pool, fib_pool,
							    fib, hw_fib, num);

free_mem:
		/* Free up the remaining resources */
		hw_fib_p = hw_fib_pool;
		fib_p = fib_pool;
		while (hw_fib_p < &hw_fib_pool[num]) {
			kfree(*hw_fib_p);
			kfree(*fib_p);
			++fib_p;
			++hw_fib_p;
		}
		kfree(fib_pool);
free_hw_fib_pool:
		kfree(hw_fib_pool);
free_fib:
		kfree(fib);
		t_lock = dev->queues->queue[HostNormCmdQueue].lock;
		spin_lock_irqsave(t_lock, flags);
	}
	/*
	 *	There are no more AIF's
	 */
	t_lock = dev->queues->queue[HostNormCmdQueue].lock;
	spin_unlock_irqrestore(t_lock, flags);
}

int aac_send_safw_hostttime(struct aac_dev *dev, struct timespec64 *now)
{
	struct tm cur_tm;
	char wellness_str[] = "<HW>TD\010\0\0\0\0\0\0\0\0\0DW\0\0ZZ";
	u32 datasize = sizeof(wellness_str);
	int ret = -ENODEV;
#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	time64_t local_time;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,32) && !defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	extern struct timezone sys_tz;
#endif

	if (!dev->sa_firmware)
		goto out;

#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	local_time = (now->tv_sec - (sys_tz.tz_minuteswest * 60));
	time64_to_tm(local_time, 0, &cur_tm);
#else
	time_to_tm(now->tv_sec, 0, &cur_tm);
#endif

	cur_tm.tm_mon += 1;
	cur_tm.tm_year += 1900;
	wellness_str[8] = bin2bcd(cur_tm.tm_hour);
	wellness_str[9] = bin2bcd(cur_tm.tm_min);
	wellness_str[10] = bin2bcd(cur_tm.tm_sec);
	wellness_str[12] = bin2bcd(cur_tm.tm_mon);
	wellness_str[13] = bin2bcd(cur_tm.tm_mday);
	wellness_str[14] = bin2bcd(cur_tm.tm_year / 100);
	wellness_str[15] = bin2bcd(cur_tm.tm_year % 100);

	ret = send_wellness_command(dev, wellness_str, datasize);

out:
	return ret;
}

int aac_send_hosttime(struct aac_dev *dev, struct timespec64 *now)
{
	int ret = -ENOMEM;
	struct fib *fibptr;
	__le32 *info;

	fibptr = aac_fib_alloc(dev, NULL);
	if (!fibptr)
		goto out;

	aac_fib_init(fibptr);
	info = (__le32 *)fib_data(fibptr);
	*info = cpu_to_le32(now->tv_sec); /* overflow in y2106 */
	ret = aac_fib_send(SendHostTime, fibptr, sizeof(*info), FsaNormal,
					1, 1, NULL, NULL);

	/*
	 * Do not set XferState to zero unless
	 * receives a response from F/W
	 */
	if (ret >= 0)
		aac_fib_complete(fibptr);

	/*
	 * FIB should be freed only after
	 * getting the response from the F/W
	 */
	if (ret != -ERESTARTSYS)
		aac_fib_free(fibptr);

out:
	return ret;
}

/**
 *	aac_command_thread	-	command processing thread
 *	@dev: Adapter to monitor
 *
 *	Waits on the commandready event in it's queue. When the event gets set
 *	it will pull FIBs off it's queue. It will continue to pull FIBs off
 *	until the queue is empty. When the queue is empty it will wait for
 *	more FIBs.
 */

#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
int aac_command_thread(struct aac_dev * dev)
#else
int aac_command_thread(void *data)
#endif
{
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,5)) || defined(HAS_KTHREAD))
	struct aac_dev *dev = data;
#endif
#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
	DECLARE_WAITQUEUE(wait, current);
#endif
	unsigned long next_jiffies = jiffies + HZ;
	unsigned long next_check_jiffies = next_jiffies;
	long difference = HZ;

	adbg_reset(dev, KERN_ERR, "update_interval=%d:%02d check_interval=%ds\n",
		update_interval / 60, update_interval % 60, check_interval);

	/*
	 *	We can only have one thread per adapter for AIF's.
	 */
	if (dev->aif_thread)
		return -EINVAL;
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
	/*
	 *	Set up the name that will appear in 'ps'
	 *	stored in  task_struct.comm[16].
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	daemonize(dev->name);
	allow_signal(SIGKILL);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32))
	allow_signal(SIGTERM);
#endif
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
	snprintf(current->comm, sizeof(current->comm), dev->name);
	daemonize();
#else
	sprintf(current->comm, dev->name);
	daemonize();
#endif
#endif
#else

#endif
	/*
	 *	Let the DPC know it has a place to send the AIF's to.
	 */
	dev->aif_thread = 1;
#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
	add_wait_queue(&dev->queues->queue[HostNormCmdQueue].cmdready, &wait);
#endif
	set_current_state(TASK_INTERRUPTIBLE);
	dprintk ((KERN_INFO "aac_command_thread start\n"));
	while (1) {
		aac_process_events(dev);
		/*
		 *	Background activity
		 */
		if ((time_before(next_check_jiffies,next_jiffies))
		 && ((difference = next_check_jiffies - jiffies) <= 0)) {
			next_check_jiffies = next_jiffies;
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
			if (aac_check_health(dev) == 0) {
#else
			if (aac_adapter_check_health(dev) == 0) {
#endif
				difference = ((long)(unsigned)check_interval)
					   * HZ;
				next_check_jiffies = jiffies + difference;
			} else if (!dev->queues)
				break;
		}

		if (!time_before(next_check_jiffies,next_jiffies)
		 && ((difference = next_jiffies - jiffies) <= 0)) {
			struct timespec64 now;
			int ret;

		 /* Don't even try to talk to adapter if its sick */
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__))
			ret = aac_check_health(dev);
#else
			ret = aac_adapter_check_health(dev);
#endif
			if (!dev->queues) {
				// can't proceed further due to health check (or)
				// adapter reinit has failed.
				break;
			}
			next_check_jiffies = jiffies + ((long)(unsigned)check_interval) * HZ;
			ktime_get_real_ts64(&now);

			/* Synchronize our watches */
#if (defined(timespec64_undefined))
			if (((1000000 - (1000000 / HZ)) > now.tv_usec) && (now.tv_usec > (1000000 / HZ)))
				difference = (((1000000 - now.tv_usec) * HZ) + 500000) / 1000000;
#else
			if (((NSEC_PER_SEC - (NSEC_PER_SEC / HZ)) > now.tv_nsec) && (now.tv_nsec > (NSEC_PER_SEC / HZ)))
				difference = (((NSEC_PER_SEC - now.tv_nsec) * HZ) + NSEC_PER_SEC / 2) / NSEC_PER_SEC;
#endif
			else if (ret == 0) {
#if (defined(timespec64_undefined))
				if (now.tv_usec > 500000)
#else
				if (now.tv_nsec > NSEC_PER_SEC / 2)
#endif
					++now.tv_sec;

				if (dev->sa_firmware)
					ret = aac_send_safw_hostttime(dev, &now);
				else
					ret = aac_send_hosttime(dev, &now);

				difference = (long)(unsigned)update_interval*HZ;

			} else {
				/* retry shortly */
				difference = 10 * HZ;
			}

			next_jiffies = jiffies + difference;
			if (time_before(next_check_jiffies,next_jiffies))
				difference = next_check_jiffies - jiffies;
		}
		if (difference <= 0)
			difference = 1;
#if (!defined(CONFIG_COMMUNITY_KERNEL))
		if (nblank(fwprintf(x)) && (difference > HZ))
			difference = HZ;
#endif
		set_current_state(TASK_INTERRUPTIBLE);
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
		if (dev->thread_die)
			break;

		down(&dev->queues->queue[HostNormCmdQueue].cmdready);
#else

#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	if(signal_pending(current))
#else
	if(kthread_should_stop())
#endif
	break;
		schedule_timeout(difference);

#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
		if(signal_pending(current))
#else
		if (kthread_should_stop())
#endif
			break;
#endif
	}
	aac_info (dev, "command thread stopped\n");

#if ((!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__)) || defined(__VMKLNX__))
	if (dev->queues)
		remove_wait_queue(&dev->queues->queue[HostNormCmdQueue].cmdready, &wait);
#endif
	dev->aif_thread = 0;
	/* in case we are here due to controller failure, wait until termination
	   by kernel control paths.
	   */
	for(;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		if(kthread_should_stop())
			break;
		schedule();
	}
	set_current_state(TASK_RUNNING);
#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	complete_and_exit(&dev->aif_completion, 0);
#endif
	return 0;
}

int aac_setup_cpu_msix_tbl(struct aac_dev *dev)
{
	int ret = 0;
	int i = 0;
	int nr_cpu = 0;
	struct cpu_msix_tbl *tbl;

#if (defined(__VMKLNX__))
	/*
	 * According to vmware smp_num_cpus is held constant
	 * and should not change
	 */
	nr_cpu = smp_num_cpus;
#else
	nr_cpu = NR_CPUS;
#endif

	tbl = kmalloc(sizeof(struct cpu_msix_tbl)*nr_cpu, GFP_KERNEL);
	if(!tbl) {
		aac_err(dev, "msix_vector_tbl memory allocation failed\n");
		ret = -ENOMEM;
		goto out;
	}

	for(i=0; i < nr_cpu; i++) {
		tbl[i].is_valid = 0;
		tbl[i].msix = 0;
	}

	dev->cpu_msix_tbl = tbl;

out:
	return ret;
}

static inline void aac_src_link_cpu_msix(struct aac_dev *dev, int cpu, int msix)
{
	dev->cpu_msix_tbl[cpu].is_valid = 1;
	dev->cpu_msix_tbl[cpu].msix = msix;
}

#define HINT_RELEASE (0)
#define HINT_ACQUIRE (1)

static void aac_release_non_src_intx_irq(struct aac_dev *dev)
{
	adbg_msix(dev, KERN_INFO, "free_irq:intx:(irq %d)\n", dev->pdev->irq);
	free_irq(dev->pdev->irq, dev);
}

static void aac_release_src_intx_irq(struct aac_dev *dev)
{
	adbg_msix(dev, KERN_INFO, "free_irq:intx:(irq %d)\n", dev->pdev->irq);
	free_irq(dev->pdev->irq, &(dev->aac_msix[0]));
}

static int aac_request_src_intx_irq(struct aac_dev *dev)
{
	int ret;

	dev->aac_msix[0].vector_no = 0;
	dev->aac_msix[0].dev = dev;

	adbg_msix(dev, KERN_INFO, "request_irq:intx:(irq %d)\n", dev->pdev->irq);
	ret = request_irq(dev->pdev->irq, dev->a_ops.adapter_intr,
			IRQF_SHARED, "aacraid", &(dev->aac_msix[0]));
	if (ret)
		aac_err(dev, "Failed to register intx vecotr\n");

	return ret;
}

static void aac_release_src_msix_irq(struct aac_dev *dev, int num_of_msix)
{
	int i = 0;
	unsigned int irq = 0;

	for (i = 0 ; i < num_of_msix ; i++) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0))
		irq = dev->msixentry[i].vector;
#else
		irq = pci_irq_vector(dev->pdev, i);
#endif
		adbg_msix(dev, KERN_INFO, "free_irq:msix:(irq %d)\n", irq);
		free_irq(irq, &(dev->aac_msix[i]));
	}
}

static int aac_request_src_msix_irq(struct aac_dev *dev)
{
	int i = 0;
	int ret = 0;
	unsigned int irq = 0;

	for (i=0; i < dev->max_msix; i++) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0))
		irq = dev->msixentry[i].vector;
#else
		irq = pci_irq_vector(dev->pdev, i);
#endif
		dev->aac_msix[i].vector_no = i;
		dev->aac_msix[i].dev = dev;

		adbg_msix(dev, KERN_INFO, "request_irq:msix:(irq %d)\n", irq);
		ret = request_irq(irq, dev->a_ops.adapter_intr, 0, "aacraid", &(dev->aac_msix[i]));
		if (ret) {
			aac_err(dev, "Filed to register IRQ for vector %d\n",i);
			goto free_msix_irq;
		}
	}
out:
	return ret;

free_msix_irq:
	aac_release_src_msix_irq(dev, i);
	goto out;
}

static int aac_src_irq_affinity_hints(struct aac_dev *dev, int cmd)
{
	int ret = 0;
#if ((defined(RHEL_MAJOR) && RHEL_MAJOR == 6 && RHEL_MINOR >= 2) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35) && \
	 LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)))
	int cpu;
	int i;
	int vec;
	char *scmd = NULL;


	if(cmd == HINT_ACQUIRE)
		scmd = "HINT_ACQUIRE";
	else
		scmd = "HINT_RELEASE";

	(void)scmd;

	cpu = cpumask_first(cpu_online_mask);

	for (i = 0; i < dev->max_msix; i++) {
		const struct cpumask *cpu_mask = get_cpu_mask(cpu);

		if (cmd == HINT_ACQUIRE)
			aac_src_link_cpu_msix(dev, cpu, i);
		else if (cmd == HINT_RELEASE)
			cpu_mask = NULL;
		else
			aac_err(dev, "invalid argument\n");

		vec = dev->msixentry[i].vector;

		adbg_msix(dev, KERN_INFO, "irq_set_affinity_hint:msix:%s:(irq %d) (cpu %d)\n",
						scmd,vec, cpu);
		ret = irq_set_affinity_hint(vec, cpu_mask);
		if(ret) {
			aac_err(dev, "Failed to set IRQ affinity for cpu %d\n", cpu);
		}

		cpu = cpumask_next(cpu, cpu_online_mask);
	}
#endif
	return ret;
}

int aac_acquire_irq(struct aac_dev *dev)
{
	int ret = 0;

	if (!dev->sync_mode && dev->msix_enabled && dev->max_msix > 1) {
		ret = aac_request_src_msix_irq(dev);
		if (ret)
			goto disable_pci_msix;

		aac_src_irq_affinity_hints(dev, HINT_ACQUIRE);
	} else
		ret = aac_request_src_intx_irq(dev);
out:
	return ret;

disable_pci_msix:
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_DISABLE_MSI))
	pci_disable_msix(dev->pdev);
#endif
	goto out;
}

void aac_free_irq(struct aac_dev *dev)
{
	adbg_shut(dev, KERN_INFO, "Free IRQ\n");

	/*
	 * The non src irq is released here as well
	 */
	if(!aac_is_src(dev)) {
		aac_release_non_src_intx_irq(dev);
	}

	if (dev->max_msix > 1) {
		aac_src_irq_affinity_hints(dev, HINT_RELEASE);
		aac_release_src_msix_irq(dev, dev->max_msix);
	} else
		aac_release_src_intx_irq(dev);

#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_DISABLE_MSI))
	if (dev->max_msix > 1)
		pci_disable_msix(dev->pdev);
#endif
}

