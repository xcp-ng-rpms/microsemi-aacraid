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
 *   linit.c
 *
 * Abstract: Linux Driver entry module for Adaptec RAID Array Controller
 */


#include <linux/version.h> /* for the following test */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
#include <linux/compat.h>
#endif
#if (!defined(UTS_RELEASE) && ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)) && !defined(__VMKLNX__))
#include <linux/utsrelease.h>
#endif
#if (defined(HAS_COMPILE_H) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)) && !defined(CONFIG_COMMUNITY_KERNEL) && !defined(UTS_MACHINE))
#include <linux/compile.h>
#endif
#include <linux/blkdev.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,2))
#include <linux/completion.h>
#endif
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
#include <linux/moduleparam.h>
#else
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#endif
#include <linux/pci.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
#include <linux/aer.h>
#endif
#include <linux/slab.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
#include <linux/smp_lock.h>
#else
#include <linux/mutex.h>
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30) && LINUX_VERSION_CODE < KERNEL_VERSION(5,4,0))
#include <linux/pci-aspm.h>
#endif
#include <linux/spinlock.h>
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3))
#if (!defined(__VMKLNX30__) && !defined(__VMKLNX__))
#include <linux/syscalls.h>
#else
#if defined(__ESX5__)
#include "vmklinux_9/vmklinux_scsi.h"
#else
#include "vmklinux26/vmklinux26_scsi.h"
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
#include <linux/ioctl32.h>
#endif
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)) || defined(SCSI_HAS_SSLEEP)
#include <linux/delay.h>
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,5)) || defined(HAS_KTHREAD))
#include <linux/kthread.h>
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#endif

#include <scsi/scsi.h>
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,1)) && !defined(FAILED))
#define SUCCESS 0x2002
#define FAILED  0x2003
#endif
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && defined(DID_BUS_BUSY) && !defined(BLIST_NO_ULD_ATTACH))
#include <scsi/scsi_devinfo.h>	/* Pick up BLIST_NO_ULD_ATTACH? */
#endif
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_eh.h>
#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
#include <scsi/scsi_transport_sas.h>
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)) && !defined(BLIST_NO_ULD_ATTACH))
#define no_uld_attach inq_periph_qual
#elif ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)) && !defined(BLIST_NO_ULD_ATTACH))
#define no_uld_attach hostdata
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))
#if (((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT)) && !defined(HAS_BOOT_CONFIG))
#if ((KERNEL_VERSION(2,4,19) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,21)))
# include <asm-x86_64/ioctl32.h>
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
# include <asm/ioctl32.h>
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,3))
# include <linux/ioctl32.h>
#endif
  /* Cast the function, since sys_ioctl does not match */
# define aac_ioctl32(x,y) register_ioctl32_conversion((x), \
    (int(*)(unsigned int,unsigned int,unsigned long,struct file*))(y))
#endif
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
# include <asm/uaccess.h>
#endif
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)) || ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11)) && !defined(PCI_HAS_SHUTDOWN)))
#include <linux/reboot.h>
#endif

#include "aacraid.h"
#if (!defined(CONFIG_COMMUNITY_KERNEL))
#include "fwdebug.h"
#endif

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
spinlock_t io_request_lock;
#endif

MODULE_AUTHOR("Red Hat Inc and Adaptec");
MODULE_DESCRIPTION("Dell PERC2, 2/Si, 3/Si, 3/Di, "
		   "Adaptec Advanced Raid Products, "
		   "HP NetRAID-4M, IBM ServeRAID & ICP SCSI driver");
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,7))
MODULE_LICENSE("GPL");
#endif
#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,3)) || defined(MODULE_VERSION))
MODULE_VERSION(AAC_DRIVER_FULL_VERSION);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
static DEFINE_MUTEX(aac_mutex);
#endif
#if (defined(AAC_CSMI))
extern struct list_head aac_devices;
LIST_HEAD(aac_devices);
#else
#if (defined(CONFIG_COMMUNITY_KERNEL))
static LIST_HEAD(aac_devices);
#else
extern struct list_head aac_devices;
LIST_HEAD(aac_devices); /* fwprint */
#endif
#endif
#if (!defined(HAS_BOOT_CONFIG))
static int aac_cfg_major = AAC_CHARDEV_UNREGISTERED;
#endif
char aac_driver_version[] = AAC_DRIVER_FULL_VERSION;
extern int aac_disc_delay;
extern int aac_removable;
/*
 * Because of the way Linux names scsi devices, the order in this table has
 * become important.  Check for on-board Raid first, add-in cards second.
 *
 * Note: The last field is used to index into aac_drivers below.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
#ifdef DECLARE_PCI_DEVICE_TABLE
static DECLARE_PCI_DEVICE_TABLE(aac_pci_tbl) = {
#elif (defined(__devinitconst))
static const struct pci_device_id aac_pci_tbl[] __devinitconst = {
#else
static const struct pci_device_id aac_pci_tbl[] __devinitdata = {
#endif
#else
static const struct pci_device_id aac_pci_tbl[] = {
#endif
	{ 0x1028, 0x0001, 0x1028, 0x0001, 0, 0, 0 }, /* PERC 2/Si (Iguana/PERC2Si) */
	{ 0x1028, 0x0002, 0x1028, 0x0002, 0, 0, 1 }, /* PERC 3/Di (Opal/PERC3Di) */
	{ 0x1028, 0x0003, 0x1028, 0x0003, 0, 0, 2 }, /* PERC 3/Si (SlimFast/PERC3Si */
	{ 0x1028, 0x0004, 0x1028, 0x00d0, 0, 0, 3 }, /* PERC 3/Di (Iguana FlipChip/PERC3DiF */
	{ 0x1028, 0x0002, 0x1028, 0x00d1, 0, 0, 4 }, /* PERC 3/Di (Viper/PERC3DiV) */
	{ 0x1028, 0x0002, 0x1028, 0x00d9, 0, 0, 5 }, /* PERC 3/Di (Lexus/PERC3DiL) */
	{ 0x1028, 0x000a, 0x1028, 0x0106, 0, 0, 6 }, /* PERC 3/Di (Jaguar/PERC3DiJ) */
	{ 0x1028, 0x000a, 0x1028, 0x011b, 0, 0, 7 }, /* PERC 3/Di (Dagger/PERC3DiD) */
	{ 0x1028, 0x000a, 0x1028, 0x0121, 0, 0, 8 }, /* PERC 3/Di (Boxster/PERC3DiB) */
	{ 0x9005, 0x0283, 0x9005, 0x0283, 0, 0, 9 }, /* catapult */
	{ 0x9005, 0x0284, 0x9005, 0x0284, 0, 0, 10 }, /* tomcat */
	{ 0x9005, 0x0285, 0x9005, 0x0286, 0, 0, 11 }, /* Adaptec 2120S (Crusader) */
	{ 0x9005, 0x0285, 0x9005, 0x0285, 0, 0, 12 }, /* Adaptec 2200S (Vulcan) */
	{ 0x9005, 0x0285, 0x9005, 0x0287, 0, 0, 13 }, /* Adaptec 2200S (Vulcan-2m) */
	{ 0x9005, 0x0285, 0x17aa, 0x0286, 0, 0, 14 }, /* Legend S220 (Legend Crusader) */
	{ 0x9005, 0x0285, 0x17aa, 0x0287, 0, 0, 15 }, /* Legend S230 (Legend Vulcan) */

	{ 0x9005, 0x0285, 0x9005, 0x0288, 0, 0, 16 }, /* Adaptec 3230S (Harrier) */
	{ 0x9005, 0x0285, 0x9005, 0x0289, 0, 0, 17 }, /* Adaptec 3240S (Tornado) */
	{ 0x9005, 0x0285, 0x9005, 0x028a, 0, 0, 18 }, /* ASR-2020ZCR SCSI PCI-X ZCR (Skyhawk) */
	{ 0x9005, 0x0285, 0x9005, 0x028b, 0, 0, 19 }, /* ASR-2025ZCR SCSI SO-DIMM PCI-X ZCR (Terminator) */
	{ 0x9005, 0x0286, 0x9005, 0x028c, 0, 0, 20 }, /* ASR-2230S + ASR-2230SLP PCI-X (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x028d, 0, 0, 21 }, /* ASR-2130S (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x029b, 0, 0, 22 }, /* AAR-2820SA (Intruder) */
	{ 0x9005, 0x0286, 0x9005, 0x029c, 0, 0, 23 }, /* AAR-2620SA (Intruder) */
	{ 0x9005, 0x0286, 0x9005, 0x029d, 0, 0, 24 }, /* AAR-2420SA (Intruder) */
	{ 0x9005, 0x0286, 0x9005, 0x029e, 0, 0, 25 }, /* ICP9024RO (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x029f, 0, 0, 26 }, /* ICP9014RO (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x02a0, 0, 0, 27 }, /* ICP9047MA (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x02a1, 0, 0, 28 }, /* ICP9087MA (Lancer) */
	{ 0x9005, 0x0286, 0x9005, 0x02a3, 0, 0, 29 }, /* ICP5445AU (Hurricane44) */
	{ 0x9005, 0x0285, 0x9005, 0x02a4, 0, 0, 30 }, /* ICP9085LI (Marauder-X) */
	{ 0x9005, 0x0285, 0x9005, 0x02a5, 0, 0, 31 }, /* ICP5085BR (Marauder-E) */
	{ 0x9005, 0x0286, 0x9005, 0x02a6, 0, 0, 32 }, /* ICP9067MA (Intruder-6) */
	{ 0x9005, 0x0287, 0x9005, 0x0800, 0, 0, 33 }, /* Themisto Jupiter Platform */
	{ 0x9005, 0x0200, 0x9005, 0x0200, 0, 0, 33 }, /* Themisto Jupiter Platform */
	{ 0x9005, 0x0286, 0x9005, 0x0800, 0, 0, 34 }, /* Callisto Jupiter Platform */
	{ 0x9005, 0x0285, 0x9005, 0x028e, 0, 0, 35 }, /* ASR-2020SA SATA PCI-X ZCR (Skyhawk) */
	{ 0x9005, 0x0285, 0x9005, 0x028f, 0, 0, 36 }, /* ASR-2025SA SATA SO-DIMM PCI-X ZCR (Terminator) */
	{ 0x9005, 0x0285, 0x9005, 0x0290, 0, 0, 37 }, /* AAR-2410SA PCI SATA 4ch (Jaguar II) */
	{ 0x9005, 0x0285, 0x1028, 0x0291, 0, 0, 38 }, /* CERC SATA RAID 2 PCI SATA 6ch (DellCorsair) */
	{ 0x9005, 0x0285, 0x9005, 0x0292, 0, 0, 39 }, /* AAR-2810SA PCI SATA 8ch (Corsair-8) */
	{ 0x9005, 0x0285, 0x9005, 0x0293, 0, 0, 40 }, /* AAR-21610SA PCI SATA 16ch (Corsair-16) */
	{ 0x9005, 0x0285, 0x9005, 0x0294, 0, 0, 41 }, /* ESD SO-DIMM PCI-X SATA ZCR (Prowler) */
	{ 0x9005, 0x0285, 0x103C, 0x3227, 0, 0, 42 }, /* AAR-2610SA PCI SATA 6ch */
	{ 0x9005, 0x0285, 0x9005, 0x0296, 0, 0, 43 }, /* ASR-2240S (SabreExpress) */
	{ 0x9005, 0x0285, 0x9005, 0x0297, 0, 0, 44 }, /* ASR-4005 */
	{ 0x9005, 0x0285, 0x1014, 0x02F2, 0, 0, 45 }, /* IBM 8i (AvonPark) */
	{ 0x9005, 0x0285, 0x1014, 0x0312, 0, 0, 45 }, /* IBM 8i (AvonPark Lite) */
	{ 0x9005, 0x0286, 0x1014, 0x9580, 0, 0, 46 }, /* IBM 8k/8k-l8 (Aurora) */
	{ 0x9005, 0x0286, 0x1014, 0x9540, 0, 0, 47 }, /* IBM 8k/8k-l4 (Aurora Lite) */
	{ 0x9005, 0x0285, 0x9005, 0x0298, 0, 0, 48 }, /* ASR-4000 (BlackBird) */
	{ 0x9005, 0x0285, 0x9005, 0x0299, 0, 0, 49 }, /* ASR-4800SAS (Marauder-X) */
	{ 0x9005, 0x0285, 0x9005, 0x029a, 0, 0, 50 }, /* ASR-4805SAS (Marauder-E) */
	{ 0x9005, 0x0286, 0x9005, 0x02a2, 0, 0, 51 }, /* ASR-3800 (Hurricane44) */

	{ 0x9005, 0x0285, 0x1028, 0x0287, 0, 0, 52 }, /* Perc 320/DC*/
	{ 0x1011, 0x0046, 0x9005, 0x0365, 0, 0, 53 }, /* Adaptec 5400S (Mustang)*/
	{ 0x1011, 0x0046, 0x9005, 0x0364, 0, 0, 54 }, /* Adaptec 5400S (Mustang)*/
	{ 0x1011, 0x0046, 0x9005, 0x1364, 0, 0, 55 }, /* Dell PERC2/QC */
	{ 0x1011, 0x0046, 0x103c, 0x10c2, 0, 0, 56 }, /* HP NetRAID-4M */

	{ 0x9005, 0x0285, 0x1028, PCI_ANY_ID, 0, 0, 57 }, /* Dell Catchall */
	{ 0x9005, 0x0285, 0x17aa, PCI_ANY_ID, 0, 0, 58 }, /* Legend Catchall */
	{ 0x9005, 0x0285, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 59 }, /* Adaptec Catch All */
	{ 0x9005, 0x0286, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 60 }, /* Adaptec Rocket Catch All */
	{ 0x9005, 0x0288, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 61 }, /* Adaptec NEMER/ARK Catch All */
	{ 0x9005, 0x028b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 62 }, /* Adaptec PMC Series 6 (Tupelo) */
	{ 0x9005, 0x028c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 63 }, /* Adaptec PMC Series 7 (Denali) */
	{ 0x9005, 0x028d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 64 }, /* Adaptec PMC Series 8 */
	{ 0x9005, 0x028d, 0x9005, 0x0559, 0x04, 0, 65 }, /* Adaptec MSFT NAND addition - 81605ZQ */
	{ 0,}
};
MODULE_DEVICE_TABLE(pci, aac_pci_tbl);

/*
 * dmb - For now we add the number of channels to this structure.
 * In the future we should add a fib that reports the number of channels
 * for the card.  At that time we can remove the channels from here
 */
static struct aac_driver_ident aac_drivers[] = {
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 2/Si (Iguana/PERC2Si) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Opal/PERC3Di) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Si (SlimFast/PERC3Si */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Iguana FlipChip/PERC3DiF */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Viper/PERC3DiV) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Lexus/PERC3DiL) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 1, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Jaguar/PERC3DiJ) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Dagger/PERC3DiD) */
	{ aac_rx_init, "percraid", "DELL    ", "PERCRAID        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* PERC 3/Di (Boxster/PERC3DiB) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "catapult        ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* catapult */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "tomcat          ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* tomcat */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2120S   ", 1, AAC_QUIRK_31BIT | AAC_QUIRK_34SG },		       /* Adaptec 2120S (Crusader) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2200S   ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG },		       /* Adaptec 2200S (Vulcan) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 2200S   ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Adaptec 2200S (Vulcan-2m) */
	{ aac_rx_init, "aacraid",  "Legend  ", "Legend S220     ", 1, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Legend S220 (Legend Crusader) */
	{ aac_rx_init, "aacraid",  "Legend  ", "Legend S230     ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Legend S230 (Legend Vulcan) */

	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 3230S   ", 2 }, /* Adaptec 3230S (Harrier) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "Adaptec 3240S   ", 2 }, /* Adaptec 3240S (Tornado) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2020ZCR     ", 2 }, /* ASR-2020ZCR SCSI PCI-X ZCR (Skyhawk) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2025ZCR     ", 2 }, /* ASR-2025ZCR SCSI SO-DIMM PCI-X ZCR (Terminator) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "ASR-2230S PCI-X ", 2 }, /* ASR-2230S + ASR-2230SLP PCI-X (Lancer) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "ASR-2130S PCI-X ", 1 }, /* ASR-2130S (Lancer) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "AAR-2820SA      ", 1 }, /* AAR-2820SA (Intruder) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "AAR-2620SA      ", 1 }, /* AAR-2620SA (Intruder) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "AAR-2420SA      ", 1 }, /* AAR-2420SA (Intruder) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9024RO       ", 2 }, /* ICP9024RO (Lancer) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9014RO       ", 1 }, /* ICP9014RO (Lancer) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9047MA       ", 1 }, /* ICP9047MA (Lancer) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9087MA       ", 1 }, /* ICP9087MA (Lancer) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP5445AU       ", 1 }, /* ICP5445AU (Hurricane44) */
	{ aac_rx_init, "aacraid",  "ICP     ", "ICP9085LI       ", 1 }, /* ICP9085LI (Marauder-X) */
	{ aac_rx_init, "aacraid",  "ICP     ", "ICP5085BR       ", 1 }, /* ICP5085BR (Marauder-E) */
	{ aac_rkt_init, "aacraid",  "ICP     ", "ICP9067MA       ", 1 }, /* ICP9067MA (Intruder-6) */
	{ NULL        , "aacraid",  "ADAPTEC ", "Themisto        ", 0, AAC_QUIRK_SLAVE }, /* Jupiter Platform */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "Callisto        ", 2, AAC_QUIRK_MASTER }, /* Jupiter Platform */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2020SA       ", 1 }, /* ASR-2020SA SATA PCI-X ZCR (Skyhawk) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2025SA       ", 1 }, /* ASR-2025SA SATA SO-DIMM PCI-X ZCR (Terminator) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-2410SA SATA ", 1, AAC_QUIRK_17SG }, /* AAR-2410SA PCI SATA 4ch (Jaguar II) */
	{ aac_rx_init, "aacraid",  "DELL    ", "CERC SR2        ", 1, AAC_QUIRK_17SG }, /* CERC SATA RAID 2 PCI SATA 6ch (DellCorsair) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-2810SA SATA ", 1, AAC_QUIRK_17SG }, /* AAR-2810SA PCI SATA 8ch (Corsair-8) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-21610SA SATA", 1, AAC_QUIRK_17SG }, /* AAR-21610SA PCI SATA 16ch (Corsair-16) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2026ZCR     ", 1 }, /* ESD SO-DIMM PCI-X SATA ZCR (Prowler) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "AAR-2610SA      ", 1 }, /* SATA 6Ch (Bearcat) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-2240S       ", 1 }, /* ASR-2240S (SabreExpress) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-4005        ", 1 }, /* ASR-4005 */
	{ aac_rx_init, "ServeRAID","IBM     ", "ServeRAID 8i    ", 1 }, /* IBM 8i (AvonPark) */
	{ aac_rkt_init, "ServeRAID","IBM     ", "ServeRAID 8k-l8 ", 1 }, /* IBM 8k/8k-l8 (Aurora) */
	{ aac_rkt_init, "ServeRAID","IBM     ", "ServeRAID 8k-l4 ", 1 }, /* IBM 8k/8k-l4 (Aurora Lite) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-4000        ", 1 }, /* ASR-4000 (BlackBird & AvonPark) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-4800SAS     ", 1 }, /* ASR-4800SAS (Marauder-X) */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "ASR-4805SAS     ", 1 }, /* ASR-4805SAS (Marauder-E) */
	{ aac_rkt_init, "aacraid",  "ADAPTEC ", "ASR-3800        ", 1 }, /* ASR-3800 (Hurricane44) */

	{ aac_rx_init, "percraid", "DELL    ", "PERC 320/DC     ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG }, /* Perc 320/DC*/
	{ aac_sa_init, "aacraid",  "ADAPTEC ", "Adaptec 5400S   ", 4, AAC_QUIRK_34SG }, /* Adaptec 5400S (Mustang)*/
	{ aac_sa_init, "aacraid",  "ADAPTEC ", "AAC-364         ", 4, AAC_QUIRK_34SG }, /* Adaptec 5400S (Mustang)*/
	{ aac_sa_init, "percraid", "DELL    ", "PERCRAID        ", 4, AAC_QUIRK_34SG }, /* Dell PERC2/QC */
	{ aac_sa_init, "hpnraid",  "HP      ", "NetRAID         ", 4, AAC_QUIRK_34SG }, /* HP NetRAID-4M */

	{ aac_rx_init, "aacraid",  "DELL    ", "RAID            ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Dell Catchall */
	{ aac_rx_init, "aacraid",  "Legend  ", "RAID            ", 2, AAC_QUIRK_31BIT | AAC_QUIRK_34SG | AAC_QUIRK_SCSI_32 }, /* Legend Catchall */
	{ aac_rx_init, "aacraid",  "ADAPTEC ", "RAID            ", 2 }, /* Adaptec Catch All */
	{ aac_rkt_init, "aacraid", "ADAPTEC ", "RAID            ", 2 }, /* Adaptec Rocket Catch All */
	{ aac_nark_init, "aacraid", "ADAPTEC ", "RAID            ", 2 }, /* Adaptec NEMER/ARK Catch All */
	{ aac_src_init, "aacraid", "ADAPTEC ", "RAID            ", 2, AAC_QUIRK_SRC }, /* Adaptec PMC Series 6 (Tupelo) */
	{ aac_srcv_init, "aacraid", "ADAPTEC ", "RAID            ", 2, AAC_QUIRK_SRC }, /* Adaptec PMC Series 7 (Denali) */
	{ aac_srcv_init, "aacraid", "ADAPTEC ", "RAID            ", 2, AAC_QUIRK_SRC }, /* Adaptec PMC Series 8 */
	{ aac_srcv_init, "aacraid", "ADAPTEC ", "RAID            ", 2, AAC_QUIRK_SRC } /* Adaptec PMC Series 9 */
};

#ifdef AAC_SAS_TRANSPORT
struct scsi_transport_template	*aac_sas_transport_template;
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11))

#if (((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) ? defined(__x86_64__) : defined(CONFIG_COMPAT)) && !defined(HAS_BOOT_CONFIG))
/*
 * Promote 32 bit apps that call get_next_adapter_fib_ioctl to 64 bit version
 */
static int aac_get_next_adapter_fib_ioctl(unsigned int fd, unsigned int cmd,
		unsigned long arg, struct file *file)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	struct fib_ioctl f;
	mm_segment_t fs;
	int retval;

	memset (&f, 0, sizeof(f));
	if (copy_from_user(&f, (void __user *)arg, sizeof(f) - sizeof(u32)))
		return -EFAULT;
	fs = get_fs();
	set_fs(get_ds());
	retval = sys_ioctl(fd, cmd, (unsigned long)&f);
	set_fs(fs);
	return retval;
#else
	struct fib_ioctl __user *f;

	f = compat_alloc_user_space(sizeof(*f));
	if (!access_ok(VERIFY_WRITE, f, sizeof(*f)))
		return -EFAULT;

	clear_user(f, sizeof(*f));
	if (copy_in_user(f, (void __user *)arg, sizeof(*f) - sizeof(u32)))
		return -EFAULT;

	return sys_ioctl(fd, cmd, (unsigned long)f);
#endif
}
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0))
#define sys_ioctl NULL	/* register_ioctl32_conversion defaults to this when NULL passed in as a handler */
#endif
#endif

#endif

/**
 *	aac_queuecommand	-	queue a SCSI command
 *	@cmd:		SCSI command to queue
 *	@done:		Function to call on command completion
 *
 *	Queues a command for execution by the associated Host Adapter.
 *
 *	TODO: unify with aac_scsi_cmd().
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))
static int aac_queuecommand(struct scsi_cmnd *cmd, void (*done)(struct scsi_cmnd *))
#else
static int aac_queuecommand(struct Scsi_Host *shost, struct scsi_cmnd *cmd)
#endif
{
	struct scsi_device *sdev = cmd->device;
	struct aac_dev *dev = shost_priv(sdev->host);

	if (dev->adapter_shutdown)
		adbg_shut(dev, KERN_INFO, "aac_queuecommand(%p={.cmnd[0]=%x}) post-shutdown\n",
						cmd, cmd->cmnd[0]);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37))
	cmd->scsi_done = done;
#endif

	 if ( dev->adapter_panic == 1) {
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
		cmd->result = DID_NO_CONNECT << 16;
#else
		set_host_byte(cmd, DID_NO_CONNECT);
#endif
        cmd->scsi_done(cmd);
		return 0;
	} 
	cmd->SCp.phase = AAC_OWNER_LOWLEVEL;
	return (aac_scsi_cmd(cmd) ? FAILED : 0);
}


/**
 *	aac_info		-	Returns the host adapter name
 *	@shost:		Scsi host to report on
 *
 *	Returns a static string describing the device in question
 */

static const char *aac_get_info(struct Scsi_Host *shost)
{
	struct aac_dev *dev = shost_priv(shost);
	return aac_drivers[dev->cardtype].name;
}

/**
 *	aac_get_driver_ident
 *	@devtype: index into lookup table
 *
 *	Returns a pointer to the entry in the driver lookup table.
 */

struct aac_driver_ident* aac_get_driver_ident(int devtype)
{
	return &aac_drivers[devtype];
}


/**
 *	aac_biosparm	-	return BIOS parameters for disk
 *	@sdev: The scsi device corresponding to the disk
 *	@bdev: the block device corresponding to the disk
 *	@capacity: the sector capacity of the disk
 *	@geom: geometry block to fill in
 *
 *	Return the Heads/Sectors/Cylinders BIOS Disk Parameters for Disk.
 *	The default disk geometry is 64 heads, 32 sectors, and the appropriate
 *	number of cylinders so as not to exceed drive capacity.  In order for
 *	disks equal to or larger than 1 GB to be addressable by the BIOS
 *	without exceeding the BIOS limitation of 1024 cylinders, Extended
 *	Translation should be enabled.   With Extended Translation enabled,
 *	drives between 1 GB inclusive and 2 GB exclusive are given a disk
 *	geometry of 128 heads and 32 sectors, and drives above 2 GB inclusive
 *	are given a disk geometry of 255 heads and 63 sectors.  However, if
 *	the BIOS detects that the Extended Translation setting does not match
 *	the geometry in the partition table, then the translation inferred
 *	from the partition table will be used by the BIOS, and a warning may
 *	be displayed.
 */

static int aac_biosparm(struct scsi_device *sdev, struct block_device *bdev,
			sector_t capacity, int *geom)
{
	struct diskparm *param = (struct diskparm *)geom;
	unsigned char *buf;

	dprintk((KERN_DEBUG "aac_biosparm.\n"));

	/*
	 *	Assuming extended translation is enabled - #REVISIT#
	 */
	if (capacity >= 2 * 1024 * 1024) { /* 1 GB in 512 byte sectors */
		if(capacity >= 4 * 1024 * 1024) { /* 2 GB in 512 byte sectors */
			param->heads = 255;
			param->sectors = 63;
		} else {
			param->heads = 128;
			param->sectors = 32;
		}
	} else {
		param->heads = 64;
		param->sectors = 32;
	}

	param->cylinders = cap_to_cyls(capacity, param->heads * param->sectors);

	/*
	 *	Read the first 1024 bytes from the disk device, if the boot
	 *	sector partition table is valid, search for a partition table
	 *	entry whose end_head matches one of the standard geometry
	 *	translations ( 64/32, 128/32, 255/63 ).
	 */
	buf = scsi_bios_ptable(bdev);
	if (!buf)
		return 0;

	if(*(__le16 *)(buf + 0x40) == cpu_to_le16(0xaa55)) {
		struct partition *first = (struct partition * )buf;
		struct partition *entry = first;
		int saved_cylinders = param->cylinders;
		int num;
		unsigned char end_head, end_sec;

		for(num = 0; num < 4; num++) {
			end_head = entry->end_head;
			end_sec = entry->end_sector & 0x3f;

			if(end_head == 63) {
				param->heads = 64;
				param->sectors = 32;
				break;
			} else if(end_head == 127) {
				param->heads = 128;
				param->sectors = 32;
				break;
			} else if(end_head == 254) {
				param->heads = 255;
				param->sectors = 63;
				break;
			}
			entry++;
		}

		if (num == 4) {
			end_head = first->end_head;
			end_sec = first->end_sector & 0x3f;
		}

		param->cylinders = cap_to_cyls(capacity, param->heads * param->sectors);
		if (num < 4 && end_sec == param->sectors) {
			if (param->cylinders != saved_cylinders)
				dprintk((KERN_DEBUG "Adopting geometry: heads=%d, sectors=%d from partition table %d.\n",
					param->heads, param->sectors, num));
		} else if (end_head > 0 || end_sec > 0) {
			dprintk((KERN_DEBUG "Strange geometry: heads=%d, sectors=%d in partition table %d.\n",
				end_head + 1, end_sec, num));
			dprintk((KERN_DEBUG "Using geometry: heads=%d, sectors=%d.\n",
					param->heads, param->sectors));
		}
	}
	kfree(buf);
	return 0;
}

static int aac_slave_alloc(struct scsi_device *sdev)
{
	int rt = 0;
	struct aac_dev *aac;

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)))
	sdev->tagged_supported = 1;
	scsi_activate_tcq(sdev, sdev->host->can_queue);
#endif
#if defined(AAC_SAS_TRANSPORT)
	aac = shost_priv(sdev->host);
	if (aac_transport_enabled(aac)) {
		u32 bus, cid;

		rt = aac_get_safw_internal_bus_cid(aac, sdev, &bus, &cid);
		if (rt) {
			sdev->hostdata = NULL;
			goto out;
		}
		aac->hba_map[bus][cid].host_bus_num = sdev_channel(sdev);
		aac->hba_map[bus][cid].host_target_num = sdev_id(sdev);
		sdev->hostdata = &aac->hba_map[bus][cid];
	}
out:
#endif
	return rt;
}

static void aac_slave_destroy(struct scsi_device *sdev)
{
#if defined(AAC_SAS_TRANSPORT)
	struct aac_dev *aac = shost_priv(sdev->host);
	if (aac_transport_enabled(aac)) {
		u32 bus, cid;

		if(aac_get_bus_cid(aac, sdev, &bus, &cid))
			return;

		aac->hba_map[bus][cid].host_bus_num = INVALID;
		aac->hba_map[bus][cid].host_target_num = INVALID;
	}
#endif

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)) && (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)))
	scsi_deactivate_tcq(sdev, 1);
#endif
}

/**
 *	aac_slave_configure		-	compute queue depths
 *	@sdev:	SCSI device we are considering
 *
 *	Selects queue depths for each target device based on the host adapter's
 *	total capacity and the queue depth supported by the target device.
 *	A queue depth of one automatically disables tagged queueing.
 */

static int aac_slave_configure(struct scsi_device *sdev)
{
	struct aac_dev *aac = shost_priv(sdev->host);
	int chn, tid, is_native_device = 0, is_raw_device = 0;

	if (aac_get_bus_cid(aac, sdev, &chn, &tid))
		return -ENODEV;

	if (chn < AAC_MAX_BUSES && tid < AAC_MAX_TARGETS && aac->sa_firmware){
		if(aac->hba_map[chn][tid].devtype == AAC_DEVTYPE_NATIVE_RAW)
			is_native_device = 1;
		else if(aac->hba_map[chn][tid].devtype == AAC_DEVTYPE_ARC_RAW)
			is_raw_device = 1;
	}
	dprintk((KERN_DEBUG "aac_slave_configure: is_native_device %d is_raw_device %d",is_native_device, is_raw_device));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
#ifdef AAC_SAS_TRANSPORT
	if(aac_transport_enabled(aac)) {
		if (sa_raid_volume(aac,sdev_id(sdev)))
			sdev->no_write_same = 1;
	} else
#endif
	if (sdev_channel(sdev) == CONTAINER_CHANNEL) {
		sdev->no_write_same = 1;
	}
#endif

	if (!is_native_device) {
		if (aac->jbod && (sdev->type == TYPE_DISK))
			sdev->removable = aac_removable;
		if ((sdev->type == TYPE_DISK) &&
			(sdev_channel(sdev) != CONTAINER_CHANNEL) &&
			(!aac->jbod || sdev->inq_periph_qual) &&
			(!aac->raid_scsi_mode || (sdev_channel(sdev) != 2))) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,14))
			if (expose_physicals == 0)
				return -ENXIO;
#endif
			if (expose_physicals < 0)
				sdev->no_uld_attach = 1;
		}
	}

	if (is_native_device ||
		(sdev->tagged_supported && (sdev->type == TYPE_DISK) &&
			(!aac->raid_scsi_mode || (sdev_channel(sdev) != 2)) &&
			!sdev->no_uld_attach)) {
		struct scsi_device * dev;
		struct Scsi_Host *host = sdev->host;
		unsigned num_lsu = 0;
		unsigned num_one = 0;
		unsigned depth;
		unsigned cid;

		/*
		 * Firmware has an individual device recovery time typically
		 * of 35 seconds, give us a margin. Thor can take longer in error recovery 
		 * hence different values 
		 */
		int timeout = (aac->sa_firmware ? AAC_SA_TIMEOUT : AAC_ARC_TIMEOUT);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27))
		if (sdev->timeout < (timeout * HZ))
			sdev->timeout = timeout * HZ;
#else
		if (sdev->request_queue->rq_timeout < (timeout * HZ))
			blk_queue_rq_timeout(sdev->request_queue, timeout * HZ);
#endif
		if (!is_native_device) {
			for (cid = 0; cid < aac->maximum_num_containers; ++cid)
				if (aac->fsa_dev[cid].valid)
					++num_lsu;
			__shost_for_each_device(dev, host) {
				if (dev->tagged_supported &&
					(dev->type == TYPE_DISK) &&
					(!aac->raid_scsi_mode ||
						(sdev_channel(sdev) != 2)) &&
					!dev->no_uld_attach) {
				 if ((sdev_channel(dev) != CONTAINER_CHANNEL)
				  || !aac->fsa_dev[sdev_id(dev)].valid)
					++num_lsu;
				} else
				 ++num_one;
			}
			if (num_lsu == 0)
				++num_lsu;
			depth = (host->can_queue - num_one) / num_lsu;
			if(strncmp(sdev->vendor,"ATA",3) == 0){
				dprintk((KERN_DEBUG "SATA Device"));
				if(sdev_channel(sdev) == 1 || is_raw_device){
					dprintk((KERN_DEBUG "aac_slave_configure:RAW Device QD = 32"));
					depth = 32;
					}
			}else {
				dprintk((KERN_DEBUG "SAS Device"));
				if(sdev_channel(sdev) == 1 || is_raw_device){
					dprintk((KERN_DEBUG "aac_slave_configure:RAW Device QD = 64 "));
					depth = 64;
					}
			}
			if (depth > 256)
				depth = 256;
			else if (depth < 2)
				depth = 2;
			dprintk((KERN_DEBUG "aac_slave_configure: queue depth = %d",depth));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
			scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, depth);
#else
			scsi_change_queue_depth(sdev, depth);
#endif
		} else {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
			dprintk((KERN_DEBUG "aac_slave_configure: HBA Device queue depth %d", aac->hba_map[chn][tid].qd_limit));
			scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, aac->hba_map[chn][tid].qd_limit); 
#else
			scsi_change_queue_depth(sdev, aac->hba_map[chn][tid].qd_limit);
#endif
		}

#if (!defined(__ESX5__) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,24)) && !defined(PCI_HAS_SET_DMA_MAX_SEG_SIZE))
		if (sdev->request_queue) {
			if (!(((struct aac_dev *)host->hostdata)->adapter_info.options &
				AAC_OPT_NEW_COMM))
				blk_queue_max_segment_size(sdev->request_queue, 65536);
			else
				blk_queue_max_segment_size(sdev->request_queue,
					host->max_sectors << 9);
		}
#endif
	} else
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
		scsi_adjust_queue_depth(sdev, 0, 1);
#else
		scsi_change_queue_depth(sdev, 1);
#endif

	sdev->tagged_supported = 1;

	return 0;
}

/**
 *	aac_change_queue_depth		-	alter queue depths
 *	@sdev:	SCSI device we are considering
 *	@depth:	desired queue depth
 *
 *	Alters queue depths for target device based on the host adapter's
 *	total capacity and the queue depth supported by the target device.
 */

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33) && LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)) || (defined(RHEL_MAJOR) && (RHEL_MAJOR == 6 && RHEL_MINOR >= 2)))
static int aac_change_queue_depth(struct scsi_device *sdev, int depth,
				  int reason)
#else
static int aac_change_queue_depth(struct scsi_device *sdev, int depth)
#endif
{
	struct aac_dev *aac = shost_priv(sdev->host);
	int chn, tid, is_native_device = 0;

	if (aac_get_bus_cid(aac, sdev, &chn, &tid))
		return -ENODEV;

	if (chn < AAC_MAX_BUSES && tid < AAC_MAX_TARGETS &&
		aac->hba_map[chn][tid].devtype == AAC_DEVTYPE_NATIVE_RAW)
		is_native_device = 1;

	if (!is_native_device && sdev->tagged_supported &&
	   (sdev->type == TYPE_DISK) && (sdev_channel(sdev) == CONTAINER_CHANNEL)) {
		struct scsi_device * dev;
		struct Scsi_Host *host = sdev->host;
		unsigned num = 0;

		__shost_for_each_device(dev, host) {
			if (dev->tagged_supported && (dev->type == TYPE_DISK) &&
				(sdev_channel(dev) == CONTAINER_CHANNEL))
				++num;
			++num;
		}
		if (num >= host->can_queue)
			num = host->can_queue - 1;
		if (depth > (host->can_queue - num))
			depth = host->can_queue - num;
		if (depth > 256)
			depth = 256;
		else if (depth < 2)
			depth = 2;
		dprintk((KERN_DEBUG "aac_change_queue_depth: Container %d", depth));
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
		scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, depth);
#else
		scsi_change_queue_depth(sdev, depth);
#endif
	} else if(is_native_device) {
		if(depth > aac->hba_map[chn][tid].qd_limit)
			depth = aac->hba_map[chn][tid].qd_limit;
		else if(depth < 2)
			depth = 2;
		dprintk((KERN_DEBUG "aac_change_queue depth: native device queue depth ch:%d tid:%d qd:%d",chn,tid,aac->hba_map[chn][tid].qd_limit));
		dprintk((KERN_DEBUG "aac_change_queue_depth: user input depth %d", depth)); 
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
			scsi_adjust_queue_depth(sdev, MSG_ORDERED_TAG, depth);
#else
			scsi_change_queue_depth(sdev, depth);
#endif
	} else {
		if(depth > 256)
			depth = 256;
		else if (depth < 2)
			depth = 2;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0))
		scsi_adjust_queue_depth(sdev, 0, depth);
#else
		scsi_change_queue_depth(sdev, depth);
#endif
	}
	return sdev->queue_depth;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
static ssize_t aac_show_raid_level(struct device *dev, char *buf)
#else
static ssize_t aac_show_raid_level(struct device *dev, struct device_attribute *attr, char *buf)
#endif
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct aac_dev *aac = shost_priv(sdev->host);

	if (sdev_channel(sdev) != CONTAINER_CHANNEL)
		return snprintf(buf, PAGE_SIZE, sdev->no_uld_attach
		  ? "Hidden\n" :
		  ((aac->jbod && (sdev->type == TYPE_DISK)) ? "JBOD\n" : ""));
	return snprintf(buf, PAGE_SIZE, "%s\n",
	  get_container_type(aac->fsa_dev[sdev_id(sdev)].type));
}

static struct device_attribute aac_raid_level_attr = {
	.attr = {
		.name = "level",
		.mode = S_IRUGO,
	},
	.show = aac_show_raid_level
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13))
static ssize_t aac_show_unique_id(struct device *dev, char *buf)
#else
static ssize_t aac_show_unique_id(struct device *dev,
	     struct device_attribute *attr, char *buf)
#endif
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct aac_dev *aac = shost_priv(sdev->host);
	unsigned char sn[16];

	memset(sn, 0, sizeof(sn));

	if (sdev_channel(sdev) == CONTAINER_CHANNEL)
		memcpy(sn, aac->fsa_dev[sdev_id(sdev)].identifier, sizeof(sn));

	return snprintf(buf, 16 * 2 + 2,
		"%02X%02X%02X%02X%02X%02X%02X%02X"
		"%02X%02X%02X%02X%02X%02X%02X%02X\n",
		sn[0], sn[1], sn[2], sn[3],
		sn[4], sn[5], sn[6], sn[7],
		sn[8], sn[9], sn[10], sn[11],
		sn[12], sn[13], sn[14], sn[15]);
}

static struct device_attribute aac_unique_id_attr = {
	.attr = {
		.name = "unique_id",
		.mode = S_IRUGO,
	},
	.show = aac_show_unique_id
};

static struct device_attribute *aac_dev_attrs[] = {
	&aac_raid_level_attr,
	&aac_unique_id_attr,
	NULL,
};

#if (!defined(HAS_BOOT_CONFIG))
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5,2,0))
static int aac_ioctl(struct scsi_device *sdev, int cmd, void __user * arg)
#else
static int aac_ioctl(struct scsi_device *sdev, unsigned int cmd, void __user * arg)
#endif
{
	struct aac_dev *dev = shost_priv(sdev->host);
	int retval;

	adbg_ioctl(dev, KERN_DEBUG, "aac_ioctl(%p, %x, %p)\n", sdev, cmd, arg);
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;
	retval = aac_adapter_check_health(dev);
	if(retval) {
		adbg_ioctl(dev, KERN_DEBUG, "aac_ioctl: aac_adapter_check_health failed.\n");
		return -EBUSY;
	}
	retval = aac_do_ioctl(dev, cmd, arg);
	adbg_ioctl(dev, KERN_DEBUG, "aac_ioctl returns %d\n", retval);
	return retval;
}
#endif

extern void aac_hba_callback(void *context, struct fib * fibptr);

struct fib *aac_get_matching_fib(struct aac_dev *aac, void *data)
{
	struct fib *fib;

	for_each_fib(fib, aac) {
		if(fib->callback_data == data)
			return fib;
	}

	return NULL;
}

static void aac_fib_debug_print(struct fib *fib)
{
#if (defined(FIB_COMPLETION_TIMING))
	unsigned long wait_time;

	/* Calculating the time difference between current time
	 * and FIB allocation time stamp to derive the wait time
	 * of aborted command
	 */
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4,19,84))
        struct timeval now;
        do_gettimeofday(&now);
	wait_time = (now.tv_sec - fib->DriverTimeStartS) * 1000000L + (now.tv_usec - fib->DriverTimeStartuS);
#else
        struct timespec64 now;
        ktime_get_ts64(&now);
	wait_time = (now.tv_sec - fib->DriverTimeStartS) * 1000000L + ((now.tv_nsec / NSEC_PER_USEC) - fib->DriverTimeStartuS);
#endif

	aac_err(fib->dev, "FIB(%d) = %p : %llx Command = %d XferState = %x Wait Time = %lu Sec\n",
			fib->index, fib, fib->hw_fib_pa,
			le32_to_cpu(fib->hw_fib_va->header.Command),
			le32_to_cpu(fib->hw_fib_va->header.XferState),
			wait_time/1000000L);
#else
	aac_err(fib->dev, "FIB(%d) = %p : %llx Command = %d XferState = %x\n",
			fib->index, fib, fib->hw_fib_pa,
			le32_to_cpu(fib->hw_fib_va->header.Command),
			le32_to_cpu(fib->hw_fib_va->header.XferState));
#endif

}

/*
 *	aac_eh_tmf_lun_reset_fib 	- fill tmf lun reset fib
 *	@struct fib	fib structure to fill up
 *	@ bus,cid, lun	bus traget & lun
 *
 */
static u8 aac_eh_tmf_abort_fib(struct aac_hba_map_info *info, struct fib *fib,
					u64 tmf_lun, __le32 managed_request_id)
{
	struct aac_hba_tm_req *tmf;
	u64 address;

	tmf = (struct aac_hba_tm_req *)fib->hw_fib_va;
	memset(tmf, 0, sizeof(*tmf));
	tmf->tmf = HBA_TMF_ABORT_TASK;
	tmf->it_nexus = info->rmw_nexus;
	tmf->lun[1] = tmf_lun;
	tmf->managed_request_id = managed_request_id;

	address = (u64)fib->hw_error_pa;
	tmf->error_ptr_hi = cpu_to_le32((u32)(address >> 32));
	tmf->error_ptr_lo = cpu_to_le32((u32)(address & 0xffffffff));
	tmf->error_length = cpu_to_le32(FW_ERROR_BUFFER_SIZE);

	fib->hbacmd_size = sizeof(*tmf);

	return HBA_IU_TYPE_SCSI_TM_REQ;
}

static int aac_eh_abort(struct scsi_cmnd* cmd)
{
	struct scsi_device *dev = cmd->device;
	struct Scsi_Host *host = dev->host;
	struct aac_dev *aac = shost_priv(host);
	struct fib *fib;
	int count, found=0;
	u32 bus, cid;
	int ret = FAILED;

	if (aac->in_reset)
		return ret;

	if (aac_adapter_check_health(aac))
		return ret;

	if (aac_get_bus_cid(aac, cmd->device, &bus, &cid))
		return ret;

	aac_err(aac, "Host adapter abort request ("SCSI_ADDR_FORMAT")\n",
			host->host_no, sdev_channel(dev), sdev_id(dev),
			dev->lun);

	aac_err(aac, "Timed out Command: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			cmd->cmnd[0],  cmd->cmnd[1],  cmd->cmnd[2],   cmd->cmnd[3],  cmd->cmnd[4],  cmd->cmnd[5],
			cmd->cmnd[6],  cmd->cmnd[7],  cmd->cmnd[8],   cmd->cmnd[9],  cmd->cmnd[10], cmd->cmnd[11],
			cmd->cmnd[12], cmd->cmnd[13], cmd->cmnd[14],  cmd->cmnd[15]);

	for_each_fib(fib, aac) {
		/* On abort, if the command and FIB matches,
		 * print the FIB details
		 **/
		if (fib->callback_data == cmd &&
			fib->hw_fib_va->header.XferState)
			aac_fib_debug_print(fib);
	}

	adbg_dump_command_queue(cmd);

	if (aac->hba_map[bus][cid].devtype == AAC_DEVTYPE_NATIVE_RAW) {
		int status;
		__le32 managed_request_id = 0;
		u8 command;

		for_each_fib(fib, aac) {
			if (*(u8 *)fib->hw_fib_va != 0 &&
				(fib->flags & FIB_CONTEXT_FLAG_NATIVE_HBA) &&
				(fib->callback_data == cmd)) {

				found = 1;
				managed_request_id = ((struct aac_hba_cmd_req *)
					fib->hw_fib_va)->request_id;
				break;
			}
		}

		if (!found)
			return ret;

		/* start a HBA_TMF_ABORT_TASK TMF request */
		fib = aac_fib_alloc(aac, NULL);
		if (!fib)
			return ret;

		command = aac_eh_tmf_abort_fib(&aac->hba_map[bus][cid], fib,
				cmd->device->lun, managed_request_id);

		cmd->SCp.sent_command = 0;

		status = aac_hba_send(command, fib,
				  (fib_callback) aac_hba_callback,
				  (void *) cmd);

		/* Wait up to 15 seconds for completion */
		for (count = 0; count < 15; ++count) {
			if (cmd->SCp.sent_command) {
				ret = SUCCESS;
				break;
			}
			msleep_interruptible(1000);
		}

		if (ret != SUCCESS)
			aac_err(aac, "Host adapter abort request timed out\n");

	} else {

		switch (cmd->cmnd[0]) {
		case SERVICE_ACTION_IN:
			if (!(aac->raw_io_interface) ||
				!(aac->raw_io_64) ||
				((cmd->cmnd[1] & 0x1f) != SAI_READ_CAPACITY_16))
				break;
			/* fall through */
		case INQUIRY:
		case READ_CAPACITY:
			/* Mark associated FIB to not complete, eh handler does this */
			for_each_fib(fib, aac) {
				if (fib->hw_fib_va->header.XferState &&
					(fib->flags & FIB_CONTEXT_FLAG) &&
					(fib->callback_data == cmd)) {

						fib->flags |= FIB_CONTEXT_FLAG_TIMED_OUT;
						cmd->SCp.phase = AAC_OWNER_ERROR_HANDLER;
						ret = SUCCESS;
				}
			}
		break;
		case TEST_UNIT_READY:
			/* Mark associated FIB to not complete, eh handler does this */
			for_each_fib(fib, aac) {
				struct scsi_cmnd * command;

				if ((fib->hw_fib_va->header.XferState & cpu_to_le32(Async | NoResponseExpected)) &&
					(fib->flags & FIB_CONTEXT_FLAG) &&
					((command = fib->callback_data)) &&
					(command->device == cmd->device)) {

						fib->flags |= FIB_CONTEXT_FLAG_TIMED_OUT;
						command->SCp.phase = AAC_OWNER_ERROR_HANDLER;
						if (command == cmd)
							ret = SUCCESS;
				}
			}
		break;
		}
	}

	return ret;
}
/*
 *	aac_eh_tmf_lun_reset_fib 	- fill tmf lun reset fib
 *	@struct fib	fib structure to fill up
 *	@ bus,cid, lun	bus traget & lun
 *
 */
static u8 aac_eh_tmf_lun_reset_fib(struct aac_hba_map_info *info, 
                                   struct fib *fib, u64 tmf_lun)
{
	struct aac_hba_tm_req *tmf;
	u64 address;

	/* start a HBA_TMF_LUN_RESET TMF request */
	tmf = (struct aac_hba_tm_req *)fib->hw_fib_va;
	memset(tmf, 0, sizeof(*tmf));
	tmf->tmf = HBA_TMF_LUN_RESET;
	tmf->it_nexus = info->rmw_nexus;
	int_to_scsilun(tmf_lun, (struct scsi_lun *)tmf->lun);

	address = (u64)fib->hw_error_pa;
	tmf->error_ptr_hi = cpu_to_le32
		((u32)(address >> 32));
	tmf->error_ptr_lo = cpu_to_le32
		((u32)(address & 0xffffffff));
	tmf->error_length = cpu_to_le32(FW_ERROR_BUFFER_SIZE);
	fib->hbacmd_size = sizeof(*tmf);

	return HBA_IU_TYPE_SCSI_TM_REQ;
}
/*
 *	aac_eh_tmf_hard_reset_fib	- fill hard reset fib
 *	@ struct fib	Fib structure
 *	@ bus cid	bus & target id
 *
 */
static u8 aac_eh_tmf_hard_reset_fib(struct aac_hba_map_info *info, 
                                    struct fib *fib)
{
	struct aac_hba_reset_req *rst;
	u64 address;

	/* already tried, start a hard reset now */
	rst = (struct aac_hba_reset_req *)fib->hw_fib_va;
	memset(rst, 0, sizeof(*rst));
	/* reset_type is already zero... */
	rst->it_nexus = info->rmw_nexus;

	address = (u64)fib->hw_error_pa;
	rst->error_ptr_hi = cpu_to_le32((u32)(address >> 32));
	rst->error_ptr_lo = cpu_to_le32
		((u32)(address & 0xffffffff));
	rst->error_length = cpu_to_le32(FW_ERROR_BUFFER_SIZE);
	fib->hbacmd_size = sizeof(*rst);

    return HBA_IU_TYPE_SATA_REQ;
}

/*
 * aac_tmf_callback   -  callback to check tmf response and complete the fib
 * &fibptr:     Fib to check the response
 */
void aac_tmf_callback(void *context, struct fib *fibptr)
{
	struct aac_hba_resp *err =
		&((struct aac_native_hba *)fibptr->hw_fib_va)->resp.err;
	struct aac_hba_map_info *info = context;
	int res;

	switch (err->service_response) {
	case HBA_RESP_SVCRES_TASK_COMPLETE:
		switch (err->status) {
		case SAM_STAT_GOOD:
			res = 0;
			break;
		default:
			res = -3;
			break;
		}
		break;
	case HBA_RESP_SVCRES_TMF_REJECTED:
		res = -1;
		break;
	case HBA_RESP_SVCRES_TMF_LUN_INVALID:
		res = 0;
		break;
	case HBA_RESP_SVCRES_TMF_COMPLETE:
	case HBA_RESP_SVCRES_TMF_SUCCEEDED:
		res = 0;
		break;
	default:
		res = -2;
		break;
	}

	aac_fib_complete(fibptr);
	aac_fib_free(fibptr);

	info->reset_state = res;
}

/*
 *  @aac_target_reset - helper function for target reset
 *  scs_cmnd  - scsi command which causes reset
 *
 */
int aac_target_reset(struct scsi_cmnd *cmd)
{
	struct scsi_device *dev = cmd->device;
	struct Scsi_Host *host = dev->host;
	struct aac_dev *aac = shost_priv(host);
	struct aac_hba_map_info *info;
	int count;
	u32 bus=0, cid=0;
	int ret = FAILED;
	struct fib *fib;
	int status;
	u8 command;

	aac_err(aac ,"Host target  reset request ("SCSI_ADDR_FORMAT")\n",         
                        host->host_no, sdev_channel(dev), sdev_id(dev),
                        dev->lun); 
	if(aac_get_bus_cid(aac, dev, &bus, &cid))
		return ret;

	if (unlikely(aac_pci_offline(aac)))
		return SUCCESS;

	info = &aac->hba_map[bus][cid];

	if(info->devtype != AAC_DEVTYPE_NATIVE_RAW)
		return ret;

	if(info->reset_state > 0)
		return ret;

	fib = aac_fib_alloc(aac, NULL);
	if (!fib)
		return ret;

	/* already tried, start a hard reset now */
	command = aac_eh_tmf_hard_reset_fib(info, fib);

	info->reset_state = 2;

	status = aac_hba_send(command, fib,
				(fib_callback) aac_tmf_callback, (void *) info);

	/* Wait up to 15 seconds for completion */
	for (count = 0; count < 15; ++count) {
		if (info->reset_state <= 0) {
			ret = info->reset_state == 0 ? SUCCESS : FAILED;
			break;
		}
		msleep(1000);
	}
	
	return ret;
}

static int aac_print_command_queue_states(struct aac_dev *aac)
{
	struct Scsi_Host *host = aac->scsi_host_ptr;
	struct scsi_device *dev;
	struct scsi_cmnd * command;
	unsigned long flags;
	/* middle level cmd cnt  */
	int mlcnt = 0;
	/* low level cmd cnt */
	int llcnt = 0;
	/* error handler cmd cnt */
	int ehcnt = 0;
	/* firmware cmd cnt  */
	int fwcnt = 0;
	/* cmd not reached driver yet  */
	int krlcnt =0;

	/* check how many fib out there, and print the data  */
	__shost_for_each_device(dev, host) {
		spin_lock_irqsave(&dev->list_lock, flags);
		list_for_each_entry(command, &dev->cmd_list, list) {
			switch (command->SCp.phase) {
			case AAC_OWNER_FIRMWARE:
				fwcnt++;
				break;
			case AAC_OWNER_ERROR_HANDLER:
				ehcnt++;
				break;
			case AAC_OWNER_LOWLEVEL:
				llcnt++;
				break;
			case AAC_OWNER_MIDLEVEL:
				mlcnt++;
				break;
			default:
				krlcnt++;
				break;
			}
		}
		spin_unlock_irqrestore(&dev->list_lock, flags);
	}
	aac_err(aac,"outstanding cmnd: midlevel %d, lowlevel %d, error handler %d, firmware %d, kernel %d\n",
					mlcnt, llcnt, ehcnt, fwcnt, krlcnt);
	/*
	 *  If no pending cmnd, return from here
	 */
	return mlcnt + llcnt + fwcnt + ehcnt;
}

/*
 *	aac_dev_reset	- Device reset command handling
 *	@scsi_cmd:	SCSI command block causing the reset
 *
 */
static int aac_dev_reset(struct scsi_cmnd *cmd)
{
	struct scsi_device *dev = cmd->device;
	struct Scsi_Host *host = dev->host;
	struct aac_dev *aac = shost_priv(host);
	struct aac_hba_map_info *info;
	int count;
	u32 bus=0, cid=0;
	int ret = FAILED;
	struct fib *fib;
	int status;

	adbg_dump_pending_fibs(aac, cmd);
	aac_err(aac ,"Host device reset request ("SCSI_ADDR_FORMAT")\n",
			host->host_no, sdev_channel(dev), sdev_id(dev),
			dev->lun);

	count = aac_check_health(aac);
	if (count)
		aac_err(aac, " BlinkLED detected: 0x%x \n", count);
	
	if (count == 0xef)
		goto return_status;

	adbg_dump_command_queue(cmd);

	if (aac_get_bus_cid(aac, dev, &bus, &cid))
		goto return_status;

	if (unlikely(aac_pci_offline(aac))) {
		ret = SUCCESS;
		goto return_status;
	}

	info = &aac->hba_map[bus][cid];
	if(info->devtype == AAC_DEVTYPE_NATIVE_RAW) {
		u8 command;

		if(info->reset_state > 0)
			goto return_status;

		fib = aac_fib_alloc(aac, NULL);
		if (!fib)
			goto return_status;

		/* start a HBA_TMF_LUN_RESET TMF request */
		command = aac_eh_tmf_lun_reset_fib(info, fib,dev->lun);
		info->reset_state = 1;
		status = aac_hba_send(command, fib,
					(fib_callback) aac_tmf_callback,
					(void *) info);

		/* Wait up to 15 seconds for completion */
		for (count = 0; count < 15; ++count) {
			if (info->reset_state <= 0) {
				ret = info->reset_state == 0 ? SUCCESS : FAILED;
				break;
			}
			msleep(1000);
		}
	} else {
		/* Mark the assoc. FIB to not complete, eh handler does this */
		for_each_fib(fib, aac) {
			if (fib->hw_fib_va->header.XferState &&
				(fib->flags & FIB_CONTEXT_FLAG) &&
				(fib->callback_data == cmd)) {
					fib->flags |= FIB_CONTEXT_FLAG_EH_RESET;
					cmd->SCp.phase = AAC_OWNER_ERROR_HANDLER;
			}
		}
	}

	count = aac_print_command_queue_states(aac);
	if (!aac->sa_firmware && count == 0)
		ret = SUCCESS;

	if (ret != SUCCESS)
		aac_err(aac,"SCSI bus appears hung\n");

return_status:
#if (defined(__VMKLNX__))
	/*
	 * call target reset
	 */
	if (ret != SUCCESS)
		ret = aac_target_reset(cmd);
#endif
	return ret;
}

/*
 *	aac_eh_bus_reset	- Bus reset command handling
 *	@scsi_cmd:	SCSI command block causing the reset
 *
 */
static int aac_eh_bus_reset(struct scsi_cmnd* cmd)
{
#if (defined(__VMKLNX__))
	struct fib *fib = aac_get_matching_fib(shost_priv(cmd->device->host),
		cmd);

	if (fib && fib->hw_fib_va->header.XferState)
		goto ret_fail;

	return SUCCESS;
ret_fail:
#endif
	return FAILED;
}

/*
 *	aac_eh_target_reset	- Target reset command handling
 *	@scsi_cmd:	SCSI command block causing the reset
 *
 */
static int aac_eh_target_reset(struct scsi_cmnd *cmd)
{
	return aac_target_reset(cmd);
}

/*
 *	aac_eh_dev_reset	- Device reset command handling
 *	@scsi_cmd:	SCSI command block causing the reset
 *
 */
static int aac_eh_dev_reset(struct scsi_cmnd *cmd)
{
#if (defined(__VMKLNX__))
	struct fib *fib = aac_get_matching_fib(shost_priv(cmd->device->host),
		cmd);

	if (fib && fib->hw_fib_va->header.XferState)
		goto ret_fail;

	return SUCCESS;
ret_fail:
#endif
	return aac_dev_reset(cmd);
}

/*
 *  aac_eh_host_reset - host/adapter reset
 *  @scsi_cmd: SCSI command block causing reset
 *
 */
int aac_eh_host_reset(struct scsi_cmnd *cmd)
{
	struct scsi_device *dev = cmd->device;
	struct Scsi_Host *host = dev->host;
	struct aac_dev *aac = shost_priv(host);
	int ret = FAILED;

	if (unlikely(aac_pci_offline(aac)))
		return SUCCESS;

	/*
	* Check if reset is supported by the firmware
	*/
	if (((aac->supplement_adapter_info.supported_options2 &
		AAC_OPTION_MU_RESET) ||
		(aac->supplement_adapter_info.supported_options2 &
		AAC_OPTION_DOORBELL_RESET)) &&
			aac_check_reset &&
			((aac_check_reset != 1) ||
			!(aac->supplement_adapter_info.supported_options2 &
		AAC_OPTION_IGNORE_RESET))) {
		    /* Bypass wait for command quiesce */
		if(aac_reset_adapter(aac, 2, IOP_HWSOFT_RESET) == 0)
			ret = SUCCESS;
	}
	/*
	 * Reset EH state
	 */
	if (ret == SUCCESS) {
		int bus, cid;
		struct aac_hba_map_info *info;

		for (bus = 0; bus < AAC_MAX_BUSES; bus++) {
			for (cid = 0; cid < AAC_MAX_TARGETS; cid++) {
				info = &aac->hba_map[bus][cid];
				if (info->devtype == AAC_DEVTYPE_NATIVE_RAW)
					info->reset_state = 0;
			}
		}
	}

	return ret;
}

#if (!defined(HAS_BOOT_CONFIG))

/**
 *	aac_cfg_open		-	open a configuration file
 *	@inode: inode being opened
 *	@file: file handle attached
 *
 *	Called when the configuration device is opened. Does the needed
 *	set up on the handle and then returns
 *
 *	Bugs: This needs extending to check a given adapter is present
 *	so we can support hot plugging, and to ref count adapters.
 */

static int aac_cfg_open(struct inode *inode, struct file *file)
{
	struct aac_dev *aac;
	int err = -ENODEV;

#if (defined(__ESXi4__))
	unsigned major_number = imajor(inode); //ESXi4 support
#else
	unsigned minor_number = iminor(inode);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
	lock_kernel();  /* BKL pushdown: nothing else protects this list */
#else
	mutex_lock(&aac_mutex);	/* BKL pushdown: nothing else protects this list */
#endif
	list_for_each_entry(aac, &aac_devices, entry) {
#if (defined(__ESXi4__))
		if (aac->major_number == major_number) {
#else
		if (aac->id == minor_number) {
#endif
			file->private_data = aac;
			err = 0;
			break;
		}
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
	unlock_kernel();
#else
	mutex_unlock(&aac_mutex);
#endif

	return err;
}

/**
 *	aac_cfg_ioctl		-	AAC configuration request
 *	@inode: inode of device
 *	@file: file handle
 *	@cmd: ioctl command code
 *	@arg: argument
 *
 *	Handles a configuration ioctl. Currently this involves wrapping it
 *	up and feeding it into the nasty windowsalike glue layer.
 *
 *	Bugs: Needs locking against parallel ioctls lower down
 *	Bugs: Needs to handle hot plugging
 */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int aac_cfg_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
#else
static long aac_cfg_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
#endif
{
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	struct aac_dev * aac;
	list_for_each_entry(aac, &aac_devices, entry) {
#if (defined(__ESXi4__))
		if (aac->major_number == imajor(inode)) { //ESXi4 support
#else
		if (aac->id == iminor(inode)) {
#endif
			file->private_data = aac;
			break;
		}
	}
	if (file->private_data == NULL)
		return -ENODEV;
#endif

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	return aac_do_ioctl(file->private_data, cmd, (void __user *)arg);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
#ifdef CONFIG_COMPAT
static long aac_compat_do_ioctl(struct aac_dev *dev, unsigned cmd, unsigned long arg)
{
	long ret;
#if (LINUX_VERSION_CODE <  KERNEL_VERSION(3,0,0))
	lock_kernel();
#else
	mutex_lock(&aac_mutex);
#endif
	switch (cmd) {
	case FSACTL_MINIPORT_REV_CHECK:
	case FSACTL_SENDFIB:
	case FSACTL_OPEN_GET_ADAPTER_FIB:
	case FSACTL_CLOSE_GET_ADAPTER_FIB:
	case FSACTL_SEND_RAW_SRB:
	case FSACTL_GET_PCI_INFO:
	case FSACTL_QUERY_DISK:
	case FSACTL_DELETE_DISK:
	case FSACTL_FORCE_DELETE_DISK:
	case FSACTL_GET_CONTAINERS:
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	case FSACTL_GET_VERSION_MATCHING:
#endif
	case FSACTL_SEND_LARGE_FIB:
#if (defined(FSACTL_REGISTER_FIB_SEND) && !defined(CONFIG_COMMUNITY_KERNEL))
	case FSACTL_REGISTER_FIB_SEND:
#endif
		ret = aac_do_ioctl(dev, cmd, (void __user *)arg);
		break;

	case FSACTL_GET_NEXT_ADAPTER_FIB: {
		struct fib_ioctl __user *f;

		f = compat_alloc_user_space(sizeof(*f));

		adbg_ioctl(dev, KERN_INFO, "FSACTL_GET_NEXT_ADAPTER_FIB:"
		  " compat_alloc_user_space(%lu)=%p\n", sizeof(*f), f);

		ret = 0;
		if (clear_user(f, sizeof(*f)))
		{
			adbg_ioctl(dev, KERN_INFO, "clear_user(%p,%lu)\n", f, sizeof(*f));
			ret = -EFAULT;
		}

		if (copy_in_user(f, (void __user *)arg, sizeof(struct fib_ioctl) - sizeof(u32)))
		{
			adbg_ioctl(dev,KERN_INFO,"copy_in_user(%p,%p,%lu)\n", f,
			  (void __user *)arg,
			  sizeof(struct fib_ioctl) - sizeof(u32));
			ret = -EFAULT;
		}

		if (!ret)
			ret = aac_do_ioctl(dev, cmd, f);
		break;
	}

	default:
#if (defined(AAC_CSMI))
		ret = aac_csmi_ioctl(dev, cmd, (void __user *)arg);
		if (ret == -ENOTTY)
#endif
		ret = -ENOIOCTLCMD;
		break;
	}
#if (LINUX_VERSION_CODE <  KERNEL_VERSION(3,0,0))
	unlock_kernel();
#else
	mutex_unlock(&aac_mutex);
#endif
	return ret;
}

#if (LINUX_VERSION_CODE <  KERNEL_VERSION(5,2,0))
static int aac_compat_ioctl(struct scsi_device *sdev, int cmd, void __user *arg)
#else
static int aac_compat_ioctl(struct scsi_device *sdev, unsigned int cmd, void __user *arg)
#endif
{
	struct aac_dev *dev = shost_priv(sdev->host);
	return aac_compat_do_ioctl(dev, cmd, (unsigned long)arg);
}

static long aac_compat_cfg_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;
	return aac_compat_do_ioctl((struct aac_dev *)file->private_data, cmd, arg);
}
#endif
#endif
#endif

static ssize_t aac_show_model(
	struct device *device, struct device_attribute *attr,
	char *buf)
{
	struct aac_dev *dev = shost_priv(class_to_shost(device));
	int len;

#if (defined(CONFIG_COMMUNITY_KERNEL))

	if (dev->supplement_adapter_info.adapter_type_text[0]) {
		char * cp = dev->supplement_adapter_info.adapter_type_text;
		while (*cp && *cp != ' ')
			++cp;
		while (*cp == ' ')
			++cp;
		len = snprintf(buf, PAGE_SIZE, "%s\n", cp);
	} else
		len = snprintf(buf, PAGE_SIZE, "%s\n",
		  aac_drivers[dev->cardtype].model);
#else
	struct scsi_inq scsi_inq;
	char *cp;

	setinqstr(dev, &scsi_inq, 255);
	cp = &scsi_inq.pid[sizeof(scsi_inq.pid)-1];
	while (*cp && *cp == ' ' && cp >= scsi_inq.vid)
	--cp;
	len = snprintf(buf, PAGE_SIZE,
	  "%.*s\n", (int)(cp - scsi_inq.vid) + 1, scsi_inq.vid);
#endif
	return len;
}

static ssize_t aac_show_vendor(
	struct device *device, struct device_attribute *attr,
		char *buf)
{
	int len;
	char vendor_string[]="Adaptec";
/*
	struct aac_dev *dev = shost_priv(class_to_shost(device));
	struct aac_supplement_adapter_info *sup_adap_info;
	int len;
	struct scsi_inq scsi_inq;
	char *cp;

	sup_adap_info = &dev->supplement_adapter_info;
#if (defined(CONFIG_COMMUNITY_KERNEL))

	if (sup_adap_info->adapter_type_text[0]) {
		char * cp = sup_adap_info->adapter_type_text;
		while (*cp && *cp != ' ')
			++cp;
		len = snprintf(buf, PAGE_SIZE, "%.*s\n",
		  (int)(cp - (char *)sup_adap_info->adapter_type_text),
		  sup_adap_info->adapter_type_text);
	} else
	len = snprintf(buf, PAGE_SIZE, "%s\n",
	  aac_drivers[dev->cardtype].vname);
#else
	setinqstr(dev, &scsi_inq, 255);
	cp = &scsi_inq.vid[sizeof(scsi_inq.vid)-1];
	while (*cp && *cp == ' ' && cp > scsi_inq.vid)
		--cp;
	len = snprintf(buf, PAGE_SIZE,
	  "%.*s\n", (int)(cp - scsi_inq.vid) + 1, scsi_inq.vid);
#endif
*/
	len = snprintf(buf, strlen(vendor_string)+1, vendor_string);
	return len;
}

static ssize_t aac_show_flags(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct class_device *device,
#else
	struct device *device, struct device_attribute *attr,
#endif
	char *buf)
{
	int len = 0;
	uint64_t flags = 0;
	struct aac_dev *dev = shost_priv(class_to_shost(device));

	if (nblank(dprintk(x)))
		flags |= AAC_DPRINTK;

#if (!defined(CONFIG_COMMUNITY_KERNEL))
	if (nblank(fwprintf(x)))
		flags |= AAC_FWPRINTF;
#endif

#if (defined(AAC_DETAILED_STATUS_INFO))
	flags |=  AAC_STATUS_INFO;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_INIT))
	flags |= AAC_DEBUG_INIT;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_SETUP))
	flags |= AAC_DEBUG_SETUP;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_AAC_CONFIG))
	flags |= AAC_DEBUG_AAC_CONFIG;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_AIF))
	flags |= AAC_DEBUG_AIF;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_IOCTL))
	flags |= AAC_DEBUG_IOCTL;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_TIMING))
	flags |= AAC_DEBUG_TIMING;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_RESET))
	flags |= AAC_DEBUG_RESET;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_FIB))
	flags |= AAC_DEBUG_FIB;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_CONTEXT))
	flags |= AAC_DEBUG_CONTEXT;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_2TB))
	flags |= AAC_DEBUG_2TB;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_IO))
	flags |= AAC_DEBUG_IO;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_SG))
	flags |= AAC_DEBUG_SG;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_VM_NAMESERVE))
	flags |= AAC_DEBUG_VM_NAMESERVE;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_SYNCHRONIZE))
	flags |= AAC_DEBUG_SYNCHRONIZE;
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_SHUTDOWN))
	flags |= AAC_DEBUG_SHUTDOWN;
#endif

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) || (defined(SERVICE_ACTION_IN) && defined(SAI_READ_CAPACITY_16)))
	if (dev->raw_io_interface && dev->raw_io_64)
		flags |= AAC_SAI_READ_CAPACITY_16;
#endif

#if (defined(SCSI_HAS_VARY_IO))
	flags |= AAC_SCSI_HAS_VARY_IO;
#endif

#if (defined(BOOTCD))
	flags |= AAC_BOOTCD;
#endif

	if (dev->jbod)
		flags |= AAC_SUPPORTED_JBOD;

	if (dev->supplement_adapter_info.supported_options2 & AAC_OPTION_POWER_MANAGEMENT)
		flags |= AAC_SUPPORTED_POWER_MANAGEMENT;

#if ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)) || defined(PCI_HAS_ENABLE_MSI) || defined(PCI_HAS_DISABLE_MSI))
	if (dev->msi)
		flags |= AAC_PCI_HAS_MSI;
#endif

	len = snprintf(buf, PAGE_SIZE, "0x%llx", flags);

	return len;
}

static ssize_t aac_show_kernel_version(
	struct device *device, struct device_attribute *attr,
	char *buf)
{
	struct aac_dev *dev = shost_priv(class_to_shost(device));
	int len, tmp;

	tmp = le32_to_cpu(dev->adapter_info.kernelrev);
	len = snprintf(buf, PAGE_SIZE, "%d.%d-%d[%d]\n",
	  tmp >> 24, (tmp >> 16) & 0xff, tmp & 0xff,
	  le32_to_cpu(dev->adapter_info.kernelbuild));
	return len;
}

static ssize_t aac_show_driver_version(struct device *device,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", aac_driver_version);
}

ssize_t aac_show_serial_number(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
	struct class_device *device,
#else
	struct device *device, struct device_attribute *attr,
#endif
	char *buf)
{
	struct aac_dev *dev = shost_priv(class_to_shost(device));
	int len = 0;

	if (le32_to_cpu(dev->adapter_info.serial[0]) != 0xBAD0)
		len = snprintf(buf, PAGE_SIZE, "%06X\n",
			le32_to_cpu(dev->adapter_info.serial[0]));
	/*
	 * "DDTS# 11875: vmware 4.0 : Shows some junk value in serial number field"
	 *		 Added this fix to copy serial number into buffer
	 */

	if (len)
		len = snprintf(buf, PAGE_SIZE, "%.*s\n",
			(int)sizeof(dev->supplement_adapter_info.
			mfg_pcba_serial_no),
			dev->supplement_adapter_info.mfg_pcba_serial_no);
	return len;
}

static ssize_t aac_show_max_channel(
	struct device *device, struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
		class_to_shost(device)->max_channel);
}

static ssize_t aac_show_hba_max_channel(
	struct device *device, struct device_attribute *attr,
	char *buf)
{
	struct aac_dev *aac = shost_priv(class_to_shost(device));

	return snprintf(buf, PAGE_SIZE, "%d\n", aac->maximum_num_channels);
}

static ssize_t aac_show_hba_max_physical(
	struct device *device, struct device_attribute *attr,
	char *buf)
{
	struct aac_dev *aac = shost_priv(class_to_shost(device));

	return snprintf(buf, PAGE_SIZE, "%d\n", aac->maximum_num_physicals);
}

static ssize_t aac_show_hba_max_array(
	struct device *device, struct device_attribute *attr,
	char *buf)
{
	struct aac_dev *aac = shost_priv(class_to_shost(device));

	return snprintf(buf, PAGE_SIZE, "%d\n", aac->maximum_num_containers);
}

static ssize_t aac_show_max_id(struct device *device,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
	  class_to_shost(device)->max_id);
}

static ssize_t aac_store_reset_adapter(struct device *device,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int retval = -EACCES;

	if (!capable(CAP_SYS_ADMIN))
		return retval;

	retval = aac_reset_adapter(shost_priv(class_to_shost(device)), buf[0] == '!', IOP_HWSOFT_RESET);

	if (retval >= 0)
		retval = count;

	return retval;
}

static ssize_t aac_show_reset_adapter(
	struct device *device, struct device_attribute *attr,
	char *buf)
{
	struct aac_dev *dev = shost_priv(class_to_shost(device));
	int len, tmp;

	tmp = aac_adapter_check_health(dev);
	if ((tmp == 0) && dev->in_reset)
		tmp = -EBUSY;
	len = snprintf(buf, PAGE_SIZE, "0x%x\n", tmp);
	return len;
}

static ssize_t aac_store_uart_adapter(
	struct device *device, struct device_attribute *attr,
	const char *buf, size_t count)
{
	if (nblank(fwprintf(x))) {
		struct aac_dev *dev = shost_priv(class_to_shost(device));
		unsigned len = count;
		unsigned long seconds = get_seconds();

		/* Trim off trailing space */
		while ((len > 0) && ((buf[len-1] == '\n') ||
		  (buf[len-1] == '\r') || (buf[len-1] == '\t') ||
		  (buf[len-1] == ' ')))
			--len;
		if (len > (dev->FwDebugBufferSize - 10))
			len = dev->FwDebugBufferSize - 10;
		seconds = seconds;
		fwprintf((dev, HBA_FLAGS_DBG_FW_PRINT_B, "%02u:%02u:%02u: %.*s",
		  (int)((seconds / 3600) % 24), (int)((seconds / 60) % 60),
		  (int)(seconds % 60), len, buf));
	}
	return count;
}

static ssize_t aac_show_uart_adapter(
	struct device *device, struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, nblank(fwprintf(x)) ? "YES\n" : "NO\n");
}

static struct device_attribute aac_model = {
	.attr = {
		.name = "model",
		.mode = S_IRUGO,
	},
	.show = aac_show_model,
};
static struct device_attribute aac_vendor = {
	.attr = {
		.name = "vendor",
		.mode = S_IRUGO,
	},
	.show = aac_show_vendor,
};
static struct device_attribute aac_flags = {
	.attr = {
		.name = "flags",
		.mode = S_IRUGO,
	},
	.show = aac_show_flags,
};
static struct device_attribute aac_kernel_version = {
	.attr = {
		.name = "firmware_version",
		.mode = S_IRUGO,
	},
	.show = aac_show_kernel_version,
};
static struct device_attribute aac_lld_version = {
	.attr = {
		.name = "driver_version",
		.mode = S_IRUGO,
	},
	.show = aac_show_driver_version,
};

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
static struct class_device_attribute aac_serial_number = {
#else
static struct device_attribute aac_serial_number = {
#endif

	.attr = {
		.name = "serial_number",
		.mode = S_IRUGO,
	},
	.show = aac_show_serial_number,
};
static struct device_attribute aac_max_channel = {
	.attr = {
		.name = "max_channel",
		.mode = S_IRUGO,
	},
	.show = aac_show_max_channel,
};
static struct device_attribute aac_hba_max_channel = {
	.attr = {
		.name = "hba_max_channel",
		.mode = S_IRUGO,
	},
	.show = aac_show_hba_max_channel,
};
static struct device_attribute aac_hba_max_physical = {
	.attr = {
		.name = "hba_max_physical",
		.mode = S_IRUGO,
	},
	.show = aac_show_hba_max_physical,
};
static struct device_attribute aac_hba_max_array = {
	.attr = {
		.name = "hba_max_array",
		.mode = S_IRUGO,
	},
	.show = aac_show_hba_max_array,
};
static struct device_attribute aac_max_id = {
	.attr = {
		.name = "max_id",
		.mode = S_IRUGO,
	},
	.show = aac_show_max_id,
};
static struct device_attribute aac_reset = {
	.attr = {
		.name = "reset_host",
		.mode = S_IWUSR|S_IRUGO,
	},
	.store = aac_store_reset_adapter,
	.show = aac_show_reset_adapter,
};
#if (!defined(CONFIG_COMMUNITY_KERNEL))
static struct device_attribute aac_uart = {
	.attr = {
		.name = "uart",
		.mode = S_IWUSR|S_IRUGO,
	},
	.store = aac_store_uart_adapter,
	.show = aac_show_uart_adapter,
};
#endif

static struct device_attribute *aac_attrs[] = {
	&aac_model,
	&aac_vendor,
	&aac_flags,
	&aac_kernel_version,
	&aac_lld_version,
	&aac_serial_number,
	&aac_max_channel,
	&aac_hba_max_channel,
	&aac_hba_max_physical,
	&aac_hba_max_array,
	&aac_max_id,
	&aac_reset,
#if (!defined(CONFIG_COMMUNITY_KERNEL))
	&aac_uart,
#endif
	NULL
};

ssize_t aac_get_serial_number(struct device *device, char *buf)
{
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	return aac_show_serial_number(device, buf);
#else
	return aac_show_serial_number(device, &aac_serial_number, buf);
#endif
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(CONFIG_SCSI_PROC_FS))

/**
 *	aac_procinfo	-	Implement /proc/scsi/<drivername>/<n>
 *	@proc_buffer: memory buffer for I/O
 *	@start_ptr: pointer to first valid data
 *	@offset: offset into file
 *	@bytes_available: space left
 *	@host_no: scsi host ident
 *	@write: direction of I/O
 *
 *	Used to export driver statistics and other infos to the world outside
 *	the kernel using the proc file system. Also provides an interface to
 *	feed the driver with information.
 *
 *		For reads
 *			- if offset > 0 return -EINVAL
 *			- if offset == 0 write data to proc_buffer and set the start_ptr to
 *			beginning of proc_buffer, return the number of characters written.
 *		For writes
 *			- writes currently not supported, return -EINVAL
 *
 *	Bugs:	Only offset zero is handled
 */
static int aac_procinfo(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	struct Scsi_Host * shost,
#endif
	char *proc_buffer, char **start_ptr,off_t offset,
	int bytes_available,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	int host_no,
#endif
	int write)
{
	struct aac_dev * dev = (struct aac_dev *)NULL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	struct Scsi_Host * shost = (struct Scsi_Host *)NULL;
#endif
	char *buf;
	int len;
	int total_len = 0;

	*start_ptr = proc_buffer;
#if (defined(AAC_LM_SENSOR) || defined(IOP_RESET))
	if(offset > 0)
#else
	if ((!nblank(fwprintf(x)) && write) || offset > 0)
#endif
		return 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
	list_for_each_entry(dev, &aac_devices, entry) {
		shost = dev->scsi_host_ptr;
		if (shost->host_no == host_no)
			break;
	}
	if (shost == (struct Scsi_Host *)NULL)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		return 0;
#else
		return -ENODEV;
#endif
#endif
	dev = shost_priv(shost);
	if (dev == (struct aac_dev *)NULL)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
		return 0;
#else
		return -ENODEV;
#endif
	if (!write) {
		buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!buf)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
			return 0;
#else
			return -ENOMEM;
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
		len = snprintf(proc_buffer, bytes_available,
		  "Driver version: Adaptec Raid Controller: (v %s)\n", aac_driver_version);
#else
		len = sprintf(proc_buffer,
		  "Driver version: Adaptec Raid Controller: (v %s)\n", aac_driver_version);
#endif
		total_len = len;
		proc_buffer += len;
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "Board ID: 0x0%x%x\n", dev->pdev->subsystem_device, dev->pdev->subsystem_vendor);
#else
			len = sprintf(proc_buffer, "Board ID: 0x0%x%x\n", dev->pdev->subsystem_device, dev->pdev->subsystem_vendor);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "PCI ID: 0x0%x%x\n", dev->pdev->device, dev->pdev->vendor);
#else
			len = sprintf(proc_buffer, "PCI ID: 0x0%x%x\n", dev->pdev->device, dev->pdev->vendor);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
			len = aac_show_vendor(shost_to_class(shost), &aac_vendor, buf);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "Vendor: %.*s\n", len - 1, buf);
#else
			len = sprintf(proc_buffer, "Vendor: %.*s\n", len - 1, buf);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
			len = aac_show_model(shost_to_class(shost), &aac_model, buf);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "Model: %.*s", len, buf);
#else
			len = sprintf(proc_buffer, "Model: %.*s", len, buf);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
			len = aac_show_flags(shost_to_class(shost), buf);
#else
			len = aac_show_flags(shost_to_class(shost), &aac_flags, buf);
#endif
			if (len) {
				char *cp = proc_buffer;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
				len = snprintf(cp, bytes_available - total_len,
				  "flags=%.*s", len, buf);
#else
				len = sprintf(cp, "flags=%.*s", len, buf);
#endif
				total_len += len;
				proc_buffer += len;
				while (--len > 0) {
					if (*cp == '\n')
						*cp = '+';
					++cp;
				}
			}
		}
		if (bytes_available > total_len) {
			len = aac_show_kernel_version(shost_to_class(shost), &aac_kernel_version, buf);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "kernel: %.*s", len, buf);
#else
			len = sprintf(proc_buffer, "kernel: %.*s", len, buf);
			total_len += len;
			proc_buffer += len;
#endif
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "monitor: %.*s", len, buf);
#else
			len = sprintf(proc_buffer, "monitor: %.*s", len, buf);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
			len = snprintf(proc_buffer, bytes_available - total_len,
			  "bios: %.*s", len, buf);
#else
			len = sprintf(proc_buffer, "bios: %.*s", len, buf);
#endif
			total_len += len;
			proc_buffer += len;
		}
		if (bytes_available > total_len) {
			len = aac_get_serial_number(shost_to_class(shost), buf);
			if (len) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,4))
				len = snprintf(proc_buffer, bytes_available - total_len,
				  "serial: %.*s", len, buf);
#else
				len = sprintf(proc_buffer, "serial: %.*s", len, buf);
#endif
				total_len += len;
			}
		}
		kfree(buf);
		return total_len;
	}
#if (defined(IOP_RESET))
	{
		static char reset[] = "reset_host";
		if (strnicmp (proc_buffer, reset, sizeof(reset) - 1) == 0) {
			if (!capable(CAP_SYS_ADMIN))
				return -EPERM;
			(void)aac_reset_adapter(dev,
			    proc_buffer[sizeof(reset) - 1] == '!', IOP_HWSOFT_RESET);
			return bytes_available;
		}
	}
#endif
	if (nblank(fwprintf(x))) {
		static char uart[] = "uart=";
		if (strnicmp (proc_buffer, uart, sizeof(uart) - 1) == 0) {
			(void)aac_store_uart_adapter(shost_to_class(shost),
			  &aac_uart,
			  &proc_buffer[sizeof(uart) - 1],
			  bytes_available - (sizeof(uart) - 1));
			return bytes_available;
		}
	}
#if (defined(AAC_LM_SENSOR))
	{
		int ret, tmp, index;
		s32 temp[5];
		static char temperature[] = "temperature=";
		if (strnicmp (proc_buffer, temperature, sizeof(temperature) - 1))
			return bytes_available;
		for (index = 0;
		  index < (sizeof(temp)/sizeof(temp[0]));
		  ++index)
			temp[index] = 0x80000000;
		ret = sizeof(temperature) - 1;
		for (index = 0;
		  index < (sizeof(temp)/sizeof(temp[0]));
		  ++index) {
			int sign, mult, c;
			if (ret >= bytes_available)
				break;
			c = proc_buffer[ret];
			if (c == '\n') {
				++ret;
				break;
			}
			if (c == ',') {
				++ret;
				continue;
			}
			sign = 1;
			mult = 0;
			tmp = 0;
			if (c == '-') {
				sign = -1;
				++ret;
			}
			for (;
			  (ret < bytes_available) && ((c = proc_buffer[ret]));
			  ++ret) {
				if (('0' <= c) && (c <= '9')) {
					tmp *= 10;
					tmp += c - '0';
					mult *= 10;
				} else if ((c == '.') && (mult == 0))
					mult = 1;
				else
					break;
			}
			if ((ret < bytes_available)
			 && ((c == ',') || (c == '\n')))
				++ret;
			if (!mult)
				mult = 1;
			if (sign < 0)
				tmp = -tmp;
			temp[index] = ((tmp << 8) + (mult >> 1)) / mult;
			if (c == '\n')
				break;
		}
		ret = index;
		if (nblank(dprintk(x))) {
			for (index = 0; index < ret; ++index) {
				int sign;
				tmp = temp[index];
				sign = tmp < 0;
				if (sign)
					tmp = -tmp;
				dprintk((KERN_DEBUG "%s%s%d.%08doC",
				  (index ? "," : ""),
				  (sign ? "-" : ""),
				  tmp >> 8, (tmp % 256) * 390625));
			}
		}
		/* Send temperature message to Firmware */
		(void)aac_adapter_sync_cmd(dev, RCV_TEMP_READINGS,
		  ret, temp[0], temp[1], temp[2], temp[3], temp[4],
		  NULL, NULL, NULL, NULL, NULL);
		return bytes_available;
	}
#endif
	return -EINVAL;
}
#endif
#endif
#if (!defined(HAS_BOOT_CONFIG))

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
static struct file_operations aac_cfg_fops = {
#else
static const struct file_operations aac_cfg_fops = {
#endif
	.owner		= THIS_MODULE,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
	.ioctl		= aac_cfg_ioctl,
#else
	.unlocked_ioctl = aac_cfg_ioctl,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
#ifdef CONFIG_COMPAT
	.compat_ioctl   = aac_compat_cfg_ioctl,
#endif
#endif
	.open		= aac_cfg_open,
};
#endif

static struct scsi_host_template aac_driver_template = {
	.module				= THIS_MODULE,
#if (defined(__VMKLNX30__) || defined(__VMKLNX__))
	.name				= "aacraid",
#else
	.name				= "AAC",
#endif
	.proc_name			= AAC_DRIVERNAME,
	.info				= aac_get_info,
#if (!defined(HAS_BOOT_CONFIG))
	.ioctl				= aac_ioctl,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
#if (defined(CONFIG_COMPAT) && !defined(HAS_BOOT_CONFIG))
	.compat_ioctl			= aac_compat_ioctl,
#endif
#endif
	.queuecommand			= aac_queuecommand,
	.bios_param			= aac_biosparm,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)) || defined(CONFIG_SCSI_PROC_FS))
	.proc_info			= aac_procinfo,
#endif
#endif
	.shost_attrs			= aac_attrs,
	.slave_configure		= aac_slave_configure,
	.slave_alloc			= aac_slave_alloc,
	.slave_destroy			= aac_slave_destroy,

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
	.change_queue_depth		= aac_change_queue_depth,
#endif
#ifdef RHEL_MAJOR
#if (RHEL_MAJOR == 6 && RHEL_MINOR >= 2)
	.lockless			= 1,
#endif
#endif
	.sdev_attrs			= aac_dev_attrs,

	.eh_device_reset_handler	= aac_eh_dev_reset,
	.eh_bus_reset_handler		= aac_eh_bus_reset,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26) && !defined(__VMKLNX__))
	.eh_target_reset_handler	= aac_eh_target_reset,
#endif
	.eh_abort_handler		= aac_eh_abort,
	.eh_host_reset_handler		= aac_eh_host_reset,
	.can_queue			= AAC_NUM_IO_FIB,
	.this_id			= MAXIMUM_NUM_CONTAINERS,
	.sg_tablesize			= 16,
	.max_sectors			= 128,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0) && LINUX_VERSION_CODE < KERNEL_VERSION(4,4,0))
	.use_blk_tags			= 1,
#endif
	.cmd_per_lun			= AAC_MAX_CMD_PER_LUN,
#if (((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,23)) || defined(ENABLE_SG_CHAINING)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)))
	.use_sg_chaining		= ENABLE_SG_CHAINING,
#endif
	.emulated			= 1,
#if (defined(SCSI_HAS_VARY_IO))
	.vary_io			= 1,
#endif
};

#ifdef AAC_SAS_TRANSPORT
static int aac_sas_get_linkerrors(struct sas_phy *aac_phy)
{
	return 0;
}

static int aac_sas_get_enclosure_identifier(struct sas_rphy *aac_rphy, u64 *addr)
{
	return 0;
}

static int aac_sas_get_bay_identifier(struct sas_rphy *aac_rphy)
{
	return -ENXIO;
}

static int aac_sas_phy_reset(struct sas_phy *aac_phy, int reset_type)
{
	return 0;
}

#if (defined(RHEL_MAJOR) && (RHEL_MAJOR >= 5 && RHEL_MINOR > 0))
static int aac_sas_phy_enable(struct sas_phy *aac_phy, int enable_type)
{
	return 0;
}


static int aac_sas_set_phy_speed(struct sas_phy *aac_phy, struct sas_phy_linkrates* link)
{
	return -EINVAL;
}
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,3,5)) || (defined(RHEL_MAJOR) && (RHEL_MAJOR >= 6 && RHEL_MINOR > 2))
static int aac_sas_phy_setup(struct sas_phy *aac_phy)
{
	return 0;
}

static void aac_sas_phy_release(struct sas_phy *aac_phy)
{
}
#endif


#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,3,5)) || (defined(RHEL_MAJOR) && (RHEL_MAJOR >= 5 && RHEL_MINOR > 1))
static int aac_can_ctrl_do_commands(struct aac_dev *aac)
{
	int err = 0;

	if (aac->in_reset || aac->pci_error_state) {
		aac_err (aac, "Controller in reset\n");
		err = -EFAULT;
		goto out;
	}

	if (aac->adapter_shutdown) {
		aac_err(aac, "Adapter is shutdown\n");
		err = -EFAULT;
		goto out;
	}

	err = aac_adapter_check_health(aac);
	if (err) {
		err = -EFAULT;
		goto out;
	}
out:
	return err;
}

static struct aac_csmi_smp_cmd *aac_build_safw_csmi_smp_req(struct aac_dev *aac,
					struct sas_rphy *aac_rphy,
					struct aac_compat_bsg_job *smp_job)
{
	struct aac_csmi_smp_cmd *smp_cmd = NULL;
	struct aac_csmi_header  *ioctl_header = NULL;
	struct aac_csmi_smp_passthru *passthru_cmd = NULL;
	struct aac_smp_request *smp_request = NULL;

	size_t smp_cmd_size = sizeof(struct aac_csmi_smp_cmd);
	u32 smp_req_cmd_size = 0;
	u32 smp_resp_cmd_size = 0;
	__be64 sas_address = 0;

	smp_cmd = kzalloc(smp_cmd_size, GFP_KERNEL);
	if (!smp_cmd) {
		aac_err(aac, "Memory allocation failed\n");
		goto out;
	}

	smp_req_cmd_size = aac_compat_bsg_job_req_bytes(smp_job);
	smp_resp_cmd_size = aac_compat_bsg_job_resp_bytes(smp_job);

	ioctl_header = &smp_cmd->ioctl_header;
	ioctl_header->header_length	= sizeof(smp_cmd->ioctl_header);
	ioctl_header->timeout		= 60;
	ioctl_header->control_code	= CSMI_CC_SAS_SMP_PASSTHRU;
	ioctl_header->length		= smp_cmd_size - sizeof(smp_cmd->ioctl_header);

	passthru_cmd = &smp_cmd->params;
	passthru_cmd->phy_identifier	= aac_rphy->identify.phy_identifier;
	passthru_cmd->port_identifier	= 0;
	passthru_cmd->connection_rate	= 0;
	sas_address = cpu_to_be64(aac_rphy->identify.sas_address);
	memcpy(passthru_cmd->destination_sas_address, &sas_address, 8);
	passthru_cmd->request_length	= smp_req_cmd_size - 4;

	smp_request = &smp_cmd->params.smp_request;
	aac_compat_build_bsg_smp_request(smp_request, smp_job);

	passthru_cmd->response_bytes	= smp_resp_cmd_size;
out:
	return smp_cmd;

}

static void aac_build_safw_csmi_smp_resp(struct aac_dev *aac,
				struct aac_csmi_smp_cmd *smp_cmd,
				struct aac_compat_bsg_job *smp_job)
{
	struct aac_smp_response *smp_response = NULL;

	smp_response = &smp_cmd->params.smp_response;
	aac_compat_build_bsg_smp_response(smp_response, smp_job);
}

static void aac_build_safw_request_reply(struct aac_dev *aac,
					struct aac_srb_reply *smp_reply,
					struct aac_compat_bsg_job *smp_job)
{
	aac_compat_build_bsg_job_reply(smp_reply, smp_job);
}

static int aac_sas_smp_handler_common(struct aac_dev *aac, struct sas_rphy *aac_rphy,
					struct aac_compat_bsg_job *smp_job)
{
	int err = 0;
	struct aac_csmi_smp_cmd *smp_cmd = NULL;
	struct aac_srb *smp_srb = NULL;
	struct aac_srb_unit srbu;
	size_t smp_cmd_size = sizeof(struct aac_csmi_smp_cmd);
	u8 *bmic_cdb = NULL;

	mutex_lock(&aac->ioctl_mutex);

	if (!aac_compat_bsg_job_response_space(aac, smp_job)) {
		aac_err(aac, "No space to respond!\n");
		err = -EINVAL;
		goto out;
	}

	err = aac_can_ctrl_do_commands(aac);
	if (err)
		goto out;

	if (!aac_rphy) {
		aac_err(aac, "Does not support smp commands to controller\n");
		err = -EINVAL;
		goto out;
	}

	if (aac_rphy->identify.device_type != SAS_FANOUT_EXPANDER_DEVICE) {
		err = -EINVAL;
		goto out;
	}

	if (aac_compat_bsg_job_multiple_segments(aac, smp_job)) {
		aac_err(aac, "multiple segments request %u response %u\n",
			aac_compat_bsg_job_req_bytes(smp_job),
			aac_compat_bsg_job_resp_bytes(smp_job));
		err = -EINVAL;
		goto out;
	}

	smp_cmd = aac_build_safw_csmi_smp_req(aac, aac_rphy, smp_job);
	if(!smp_cmd) {
		err = -ENOMEM;
		goto out;
	}
	adbg_smp_dump_smp_srb_request(aac, NULL, smp_cmd, NULL);

	memset(&srbu, 0, sizeof(struct aac_srb_unit));
	smp_srb = &srbu.srb;

	smp_srb->flags    = cpu_to_le32(SRB_DataIn | SRB_DataOut);
	bmic_cdb = &smp_srb->cdb[0];
	bmic_cdb[0] = CISS_BMIC_DATA_OUT;
	bmic_cdb[5] = CSMI_CC_SAS_SMP_PASSTHRU;
	bmic_cdb[6] = CISS_CSMI_PASS_THROUGH;
	bmic_cdb[7] = (u8)(smp_cmd_size>>8);
	bmic_cdb[8] = (u8)(smp_cmd_size);
	smp_srb->cdb_size =16;


	err = aac_send_safw_bmic_cmd(aac, &srbu, (void *)smp_cmd, smp_cmd_size);
	if (err) {
		aac_err(aac, "Smp Handler failed\n");
		goto out;
	}

	aac_build_safw_csmi_smp_resp(aac, smp_cmd, smp_job);
	aac_build_safw_request_reply(aac, &srbu.srb_reply, smp_job);

	adbg_smp_dump_smp_srb_request(aac, &srbu, smp_cmd, smp_job);
out:
	kfree(smp_cmd);
	mutex_unlock(&aac->ioctl_mutex);
	return err;
}

#if defined(AAC_SAS_SMP_BSG_JOB)
static void aac_sas_smp_handler(struct bsg_job *aac_job, struct Scsi_Host *aac_host, struct sas_rphy *aac_rphy)
{
	struct aac_dev *aac = shost_priv(aac_host);
	struct aac_compat_bsg_job job;
	int result = 0;

	job.bsg_job = aac_job;
	job.reslen = 0;

	result = aac_sas_smp_handler_common(aac, aac_rphy, &job);

	bsg_job_done(aac_job, result, job.reslen);
}
#else
static int aac_sas_smp_handler(struct Scsi_Host *aac_host, struct sas_rphy *aac_rphy, struct request *smp_req)
{
	struct aac_dev *aac = shost_priv(aac_host);
	struct aac_compat_bsg_job job;

	job.smp_req = smp_req;

	return aac_sas_smp_handler_common(aac, aac_rphy, &job);
}
#endif
#endif

static struct sas_function_template aac_sas_transport_functions = {
	.get_linkerrors			= aac_sas_get_linkerrors,
	.get_enclosure_identifier	= aac_sas_get_enclosure_identifier,
	.get_bay_identifier		= aac_sas_get_bay_identifier,
	.phy_reset			= aac_sas_phy_reset,
#if (defined(RHEL_MAJOR) && (RHEL_MAJOR >= 5 && RHEL_MINOR > 0))
	.phy_enable			= aac_sas_phy_enable,
	.set_phy_speed			= aac_sas_set_phy_speed,
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,3,5)) || (defined(RHEL_MAJOR) && (RHEL_MAJOR >= 6 && RHEL_MINOR > 2))
	.phy_setup			= aac_sas_phy_setup,
	.phy_release			= aac_sas_phy_release,
#endif
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,3,5)) || (defined(RHEL_MAJOR) && (RHEL_MAJOR >= 5 && RHEL_MINOR > 1))
	.smp_handler			= aac_sas_smp_handler,
#endif
};

#endif

void aac_cancel_workers(struct aac_dev *aac)
{
	cancel_delayed_work_sync(&aac->safw_rescan_worker);
	cancel_delayed_work_sync(&aac->src_reinit_aif_worker);
}

static void __aac_shutdown(struct aac_dev * aac)
{
#ifdef AAC_SAS_TRANSPORT
	struct aac_host_map *tmp = NULL;
	struct list_head *p,*q;
	u32 key;
#endif

#if ((LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,5)) && !defined(HAS_KTHREAD))
	adbg_shut(aac, KERN_INFO, "(%p={.aif_thread=%d,.thread_pid=%d,.shutdown=%d) - ENTER\n"
				aac, aac->aif_thread, aac->thread_pid, aac->adapter_shutdown);
	if (aac->aif_thread && (aac->thread_pid > 0)) {
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
		aac->thread_die = 1;
#else
		adbg_shut(aac, KERN_INFO "kill_proc(%d,SIGKILL,0)\n", aac->thread_pid);
		kill_proc(aac->thread_pid, SIGKILL, 0);
#endif
		adbg_shut(aac, KERN_INFO, "wait_for_completion(%p)\n",
							&aac->aif_completion);
		wait_for_completion(&aac->aif_completion);
	}
#else
	adbg_shut(aac, KERN_INFO, "(%p={.aif_thread=%d,.thread=%p,.shutdown=%d) - ENTER\n",
				aac, aac->aif_thread, aac->thread, aac->adapter_shutdown);
	if (aac->aif_thread) {
		int i;

		adbg_shut(aac, KERN_INFO, "kthread_stop(%p)\n", aac->thread);
		for (i = 0; i < (aac->scsi_host_ptr->can_queue + AAC_NUM_MGT_FIB); i++) {
			struct fib *fib = &aac->fibs[i];
			if (!(fib->hw_fib_va->header.XferState & cpu_to_le32(NoResponseExpected | Async)) &&
			    (fib->hw_fib_va->header.XferState & cpu_to_le32(ResponseExpected)))
				aac_complete(&fib->event_wait);
		}
		kthread_stop(aac->thread);
		aac->thread = NULL;
	}
#endif

#ifdef AAC_SAS_TRANSPORT
	/* go through the link list */
	for (key = 0; key < AAC_MAX_TARGETS; key++) {
		list_for_each_safe (p,q,&aac->host_map_hash[key]){
			tmp = list_entry(p, struct aac_host_map, list);
			list_del(p);
			kfree(tmp);
			}
	}
#endif


	aac->adapter_shutdown = 1;

	aac_send_shutdown(aac);

	aac_adapter_disable_int(aac);

	aac_free_irq(aac);

	adbg_shut(aac, KERN_INFO, "EXIT\n");
}

void aac_init_char(void)
{
#if (!defined(HAS_BOOT_CONFIG))
	/*
	 * ESXi4 do not support character device with multiple minor numbers
	 * So we will have to create interfaces with different major numbers
	 * for each such interface
	 */
#if (defined(__ESXi4__))
	struct aac_dev		*aac;
	char			name[5];

	list_for_each_entry(aac, &aac_devices, entry) {
		sprintf(name, "aac%d", aac->id);
		aac_cfg_major = register_chrdev( 0, name, &aac_cfg_fops);
		if (aac_cfg_major < 0) {
			printk(KERN_WARNING
				"aacraid: unable to register \"%s\" device.\n", name);
		} else
			aac->major_number = aac_cfg_major;
	}
#else
	aac_cfg_major = register_chrdev( 0, "aac", &aac_cfg_fops);
	if (aac_cfg_major < 0) {
		printk(KERN_WARNING
			"aacraid: unable to register \"aac\" device.\n");
	}
#endif
#endif
}


static void aac_enable_sas_host(struct aac_dev *aac)
{
#if defined(AAC_SAS_TRANSPORT)
	if (aac->sa_firmware)
		aac->scsi_host_ptr->transportt = aac_sas_transport_template;
#endif
}

static void aac_set_sas_target_id(struct aac_dev *aac)
{
#if defined( AAC_SAS_TRANSPORT)
	struct sas_host_attrs *sas_host;

	if(!aac_transport_enabled(aac))
		return;

	sas_host = to_sas_host_attrs(aac->scsi_host_ptr);
	sas_host->next_target_id = aac->maximum_num_containers;
#endif
}

static int aac_add_host(struct aac_dev *aac)
{
	int error = 0;

	aac_enable_sas_host(aac);

	error = scsi_add_host(aac->scsi_host_ptr, &aac->pdev->dev);
	if (error) {
		aac_err(aac, "scsi_add_host failed-%d\n", error);
		goto out;
	}

	aac_set_sas_target_id(aac);
out:
	return error;
}

void aac_remove_host(struct aac_dev *dev)
{
	aac_cancel_workers(dev);

	mutex_lock(&dev->scan_mutex);
#if defined(AAC_SAS_TRANSPORT)
	aac_remove_all_safw_sas_devices(dev);

	if (aac_transport_enabled(dev))
		sas_remove_host(dev->scsi_host_ptr);
#endif
	scsi_remove_host(dev->scsi_host_ptr);
	mutex_unlock(&dev->scan_mutex);
}

void aac_reinit_aif(struct aac_dev *aac, unsigned int index)
{
    /*
     * Firmware may send a AIF messages very early and the Driver may had ignored as it is not
     * fully ready to process the messages. so send AIF to firmware so that if there is any unprocessed
     * events then it can be processed now.
     */
     if (aac_drivers[index].quirks & AAC_QUIRK_SRC)
            aac_intr_normal(aac, 0, 2, 0, NULL);
}

static int __devinit aac_probe_one(struct pci_dev *pdev,
		const struct pci_device_id *id)
{
	unsigned index = id->driver_data;
	struct Scsi_Host *shost;
	struct aac_dev *aac;
	struct list_head *insert = &aac_devices;
	int error = -ENODEV;
	int unique_id = 0;
#if (defined(__arm__) || defined(CONFIG_EXTERNAL))
	static struct pci_dev * slave = NULL;
	static int nslave = 0;
#endif
	extern int aac_sync_mode;

#if defined(__powerpc__) || defined(__PPC__) || defined(__ppc__)
	/* EEH support: FW takes time to complete the PCI hot reset.
	 * EEH will perform a hot plug activity to unload and load
	 * the driver. Delay lets the controller complete the reset
	 * and load the driver.
	 */
	msleep(1000);
#endif

#if (defined(__arm__) || defined(CONFIG_EXTERNAL))
	if (aac_drivers[index].quirks & AAC_QUIRK_SLAVE) {
		/* detect adjoining slaves */
		if (slave) {
			if ((pci_resource_start(pdev, 0)
			  + pci_resource_len(pdev, 0))
			  == pci_resource_start(slave, 0))
				slave = pdev;
			else if ((pci_resource_start(slave, 0)
			  + (pci_resource_len(slave, 0) * nslave))
			  != pci_resource_start(pdev, 0)) {
				printk(KERN_WARNING AAC_DRIVERNAME
				  ": multiple sets of slave controllers discovered\n");
				nslave = 0;
				slave = pdev;
			}
		} else
			slave = pdev;
		if (pci_resource_start(slave,0)) {
			error = pci_enable_device(pdev);
			if (error) {
				printk(KERN_WARNING AAC_DRIVERNAME
				  ": failed to enable slave\n");
				nslave = 0;
				slave = NULL;
				return error;
			}
			++nslave;
			pci_set_master(pdev);
		} else {
			printk(KERN_WARNING AAC_DRIVERNAME
			  ": slave BAR0 is not set\n");
			nslave = 0;
			slave = NULL;
			return error;
		}
		return 1;
	}
#endif
	list_for_each_entry(aac, &aac_devices, entry) {
		if (aac->id > unique_id)
			break;
		insert = &aac->entry;
		unique_id++;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30))
	pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 |
					PCIE_LINK_STATE_CLKPM);
#endif
	error = pci_enable_device(pdev);
	if (error) {
		dev_err(&pdev->dev,"%s:PCI device not enabled",__FUNCTION__);
		goto out;
	}
	error = -ENODEV;
#if (defined(__arm__) || defined(CONFIG_EXTERNAL))
	if ((aac_drivers[index].quirks & AAC_QUIRK_MASTER) && (slave)) {
		unsigned long base = pci_resource_start(pdev, 0);
		struct master_registers {
			u32 x[51];
			u32	E_CONFIG1;
			u32 y[3];
			u32	E_CONFIG2;
		} __iomem * map = ioremap(base, AAC_MIN_FOOTPRINT_SIZE);
		if (!map) {
			printk(KERN_WARNING AAC_DRIVERNAME
			  ": unable to map master adapter to configure slaves.\n");
		} else {
			((struct master_registers *)map)->E_CONFIG2
			  = cpu_to_le32(pci_resource_start(slave, 0));
			((struct master_registers *)map)->E_CONFIG1
			  = cpu_to_le32(0x5A000000 + nslave);
			iounmap(map);
		}
		nslave = 0;
		slave = NULL;
	}
#endif

	if (!(aac_drivers[index].quirks & AAC_QUIRK_SRC)) {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
			dev_err(&pdev->dev,"%s: PCI 32 BIT dma mask set failed\n",__FUNCTION__);
			goto out_disable_pdev;
		}
	}
	/*
	 * If the quirk31 bit is set, the adapter needs adapter
	 * to driver communication memory to be allocated below 2gig
	 */
	if (aac_drivers[index].quirks & AAC_QUIRK_31BIT){
		if (pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(31))) {
			dev_err(&pdev->dev,"%s: PCI 31 BIT consistent dma mask set failed\n",__FUNCTION__);
			goto out_disable_pdev;
		}
	} else {
		if (pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32))) {
			dev_err(&pdev->dev,"%s: PCI 32 BIT consistent dma mask set failed\n",__FUNCTION__);
			goto out_disable_pdev;
		}
	}
	pci_set_master(pdev);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14))
	aac_driver_template.name = aac_drivers[index].name;
#endif
#if ((defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__)) && !defined(__VMKLNX__))
	shost = vmk_scsi_register(&aac_driver_template, sizeof(struct aac_dev), pdev->bus->number, pdev->devfn);
#else
	shost = scsi_host_alloc(&aac_driver_template, sizeof(struct aac_dev));
#endif
	adbg_init(aac,KERN_INFO,"scsi_host_alloc(%p,%zu)=%p\n",
			&aac_driver_template, sizeof(struct aac_dev), shost);
	if (!shost) {
		dev_err(&pdev->dev, "%s: scsi_host_alloc failed\n",__FUNCTION__);
		goto out_disable_pdev;
	}

	shost->irq = pdev->irq;
	shost->base = pci_resource_start(pdev, 0);
	aac_scsi_set_device(shost, &pdev->dev);
	shost->unique_id = unique_id;

	shost->max_cmd_len = 16;

	if (aac_cfg_major == AAC_CHARDEV_NEEDS_REINIT)
		aac_init_char();

	aac = shost_priv(shost);
	aac->scsi_host_ptr = shost;
	aac->pdev = pdev;
	aac->name = aac_driver_template.name;
	aac->id = shost->unique_id;
	aac->cardtype = index;
#ifdef AAC_SAS_TRANSPORT
	memset(aac->hba_map, 0, sizeof(aac->hba_map));
	memset(aac->host_map_hash, 0, sizeof(aac->host_map_hash));
#endif

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)) && !defined(HAS_RESET_DEVICES))
#define	reset_devices aac_reset_devices
#endif

	INIT_LIST_HEAD(&aac->entry);
	if (aac_reset_devices || reset_devices)
		aac->init_reset = true;

	do {
		aac->fibs = kmalloc(sizeof(struct fib) * (shost->can_queue + AAC_NUM_MGT_FIB), GFP_KERNEL);
	} while (!aac->fibs && (shost->can_queue -= 16) >= (64 - AAC_NUM_MGT_FIB));
	if (!aac->fibs) {
		aac_err(aac, "Fib memory allocation failed\n");
		goto out_free_host;
	}
	memset(aac->fibs, 0, sizeof(struct fib) * (shost->can_queue + AAC_NUM_MGT_FIB));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
	aac->reply_map = kzalloc(sizeof(unsigned int) * nr_cpu_ids,
                        GFP_KERNEL);
	if(!aac->reply_map)
        {
		aac_err(aac, "reply_map allocation failed\n");
                return -ENOMEM;
        }
#endif

	spin_lock_init(&aac->fib_lock);

	mutex_init(&aac->ioctl_mutex);
	mutex_init(&aac->scan_mutex);

	INIT_DELAYED_WORK(&aac->safw_rescan_worker, aac_safw_rescan_worker);
	INIT_DELAYED_WORK(&aac->src_reinit_aif_worker, aac_src_reinit_aif_worker);

	if(reset_devices && (num_online_cpus() == 1)) {
		aac->kdump_msix = 1;
		aac_err(aac,"Kdump MSIX mode since in kdump and single CPU");
	}

	error = aac_setup_cpu_msix_tbl(aac);
	if(error)
		goto out_free_host;

        /*
	 *	Map in the registers from the adapter.
	 */
	aac->base_size = AAC_MIN_FOOTPRINT_SIZE;
	if ((*aac_drivers[index].init)(aac)){
		aac_err(aac, "PCI device init failed, driver_index-%d,pci-vendor-%x,pci-device-%x\n",\
			index,pdev->vendor, pdev->device);
        error = -ENODEV;
        goto out_unmap;
	}

	if (aac->sync_mode) {
		if (aac_sync_mode)
			aac_info(aac,"Sync. mode enforced by driver parameter. This will cause a significant performance decrease!\n");
		else
			aac_info(aac,"Async. mode not supported by current driver, sync. mode enforced.\nPlease update driver to get full performance.\n");
	}

	aac_get_fw_debug_buffer(aac);

	/*
	 *	Start any kernel threads needed
	 */
	aac->thread = kthread_run(aac_command_thread, aac, AAC_DRIVERNAME);
	if (IS_ERR(aac->thread)) {
		aac_err(aac,"Unable to create command thread.\n");
		error = PTR_ERR(aac->thread);
		aac->thread = NULL;
#if (0 && defined(BOOTCD))
		fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
		  "aacraid: Unable to create command thread."));
#endif
		goto out_deinit;
	}

	aac->maximum_num_channels = aac_drivers[index].channels;
	error = aac_get_adapter_info(aac);
	if (error < 0) {
		aac_err(aac, "aac_get_adapter_info failed-%d",error);
		goto out_deinit;
	}
	//store the physical slot info from the fiirmware so that it can be reapplied after a reset
	aac->physical_slot = aac->supplement_adapter_info.slot_number;
	/*
	 * Lets override negotiations and drop the maximum SG limit to 34
	 */
	if ((aac_drivers[index].quirks & AAC_QUIRK_34SG) &&
			(shost->sg_tablesize > 34)) {
		shost->sg_tablesize = 34;
		shost->max_sectors = (shost->sg_tablesize * 8) + 112;
	}

	if ((aac_drivers[index].quirks & AAC_QUIRK_17SG) &&
			(shost->sg_tablesize > 17)) {
		shost->sg_tablesize = 17;
		shost->max_sectors = (shost->sg_tablesize * 8) + 112;
	}

	error = aac_pci_set_dma_max_seg_size(pdev,
		(aac->adapter_info.options & AAC_OPT_NEW_COMM) ?
			(shost->max_sectors << 9) : 65536);
	if (error) {
		aac_err(aac, "pci_set_dma_max_seg_size failed-%d\n",error);
		goto out_deinit;
	}

	/*
	 * Firmware printf works only with older firmware.
	 */
	if (aac_drivers[index].quirks & AAC_QUIRK_34SG)
		aac->printf_enabled = 1;
	else
		aac->printf_enabled = 0;

	/*
	 * max channel will be the physical channels plus 1 virtual channel
	 * all containers are on the virtual channel 0 (CONTAINER_CHANNEL)
	 * physical channels are address by their actual physical number+1
	 */
	if (aac->nondasd_support || expose_physicals || aac->jbod)
		shost->max_channel = aac->maximum_num_channels;
	else
		shost->max_channel = 0;
#if defined(__powerpc__) || defined(__PPC__) || defined(__ppc__)
	error = aac_get_config_status(aac, 1);
#else
	error = aac_get_config_status(aac, 0);
#endif
	if (error < 0)
		aac_err(aac, "aac_get_config_status failed-%d\n",error);

	error = aac_get_containers(aac);
	if (error < 0)
		aac_err(aac, "aac_get_containers failed-%d\n",error);

	list_add(&aac->entry, insert);

	shost->max_id = aac->maximum_num_containers;
	if (shost->max_id < aac->maximum_num_physicals)
		shost->max_id = aac->maximum_num_physicals;
	if (shost->max_id < MAXIMUM_NUM_CONTAINERS)
		shost->max_id = MAXIMUM_NUM_CONTAINERS;
	else
		shost->this_id = shost->max_id;
#ifdef AAC_SAS_TRANSPORT
	if(aac_transport_enabled(aac))
		shost->this_id = -1;
#endif

	/*
	 * Firmware may send a AIF messages very early and the Driver may had ignored as it is not
	 *  fully ready to process the messages. so send AIF to firmware so that if there is any unprocessed
	 *  events then it can be processed now.
	 */
	if ((!aac->sa_firmware) && (aac_drivers[index].quirks & AAC_QUIRK_SRC))
		aac_intr_normal(aac, 0, 2, 0, NULL);
	/*
	 * dmb - we may need to move the setting of these parms somewhere else once
	 * we get a fib that can report the actual numbers
	 */
	shost->max_lun = AAC_MAX_LUN;

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	shost->xportFlags = VMKLNX_SCSI_TRANSPORT_TYPE_PSCSI;
#endif
	pci_set_drvdata(pdev, shost);

	error = aac_scsi_init_shared_tag_map(shost, shost->can_queue);
	if (error)
		goto out_deinit;

#ifdef AAC_SAS_TRANSPORT
	{
		int i = 0;
		for (i=0; i<AAC_MAX_TARGETS; i++) {
			INIT_LIST_HEAD(&aac->host_map_hash[i]);
		}
	}
#endif

	error = aac_add_host(aac);
	if(error)
		goto out_deinit;

#ifdef AAC_SAS_TRANSPORT
	if (aac_disc_delay) {
		aac_info(aac, "delaying for %d seconds\n", aac_disc_delay);
		ssleep(aac_disc_delay);
	}

#endif
	aac_scan_host(aac, AAC_INIT);

#if defined(__ESX5__)
	if (aac_is_src(aac)) {
		if (aac->max_msix > 1)
			vmklnx_scsi_register_poll_handler(shost, aac->msixentry[0].vector,
				aac->a_ops.adapter_intr_poll, &(aac->aac_msix[0]));
		else
			vmklnx_scsi_register_poll_handler(shost, aac->pdev->irq,
				aac->a_ops.adapter_intr, &(aac->aac_msix[0]));

	} else {
		vmklnx_scsi_register_poll_handler(shost, aac->pdev->irq,
			aac->a_ops.adapter_intr, aac);
	}
#endif

	fdbg_init(aac, HBA_FLAGS_DBG_FW_PRINT_B,
	  "aac_probe_one() returns success");

	if (aac->pdev->device == PMC_DEVICE_S6) {
		if (aac->msi)
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This Tupelo driver is MSI enabled", aac->name, aac->id));
		else
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This Tupelo driver is INTx enabled", aac->name, aac->id));
	} else if(aac_is_src(aac)) {
		if (aac->max_msix > 1)
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This driver is MSI-x enabled", aac->name, aac->id));
		else if (aac->msi)
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This driver is MSI enabled", aac->name, aac->id));
		else
			fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B, "%s%d: This driver is INTx enabled", aac->name, aac->id));
	}

	aac_pci_enable_pcie_error_reporting_and_save_state(pdev);

	return 0;

 out_deinit:
	__aac_shutdown(aac);
 out_unmap:
	aac_fib_map_free(aac);
	if (aac->comm_addr)
		aac_pci_free_consistent(aac->pdev, aac->comm_size, aac->comm_addr,
		  aac->comm_phys);
	kfree(aac->queues);
	aac_adapter_ioremap(aac, 0);
	kfree(aac->fibs);
	kfree(aac->fsa_dev);
	kfree(aac->cpu_msix_tbl);
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	spin_lock_destroy(&aac->fib_lock);
#endif
 out_free_host:
	scsi_host_put(shost);
 out_disable_pdev:
	pci_disable_device(pdev);
 out:
	return error;
}


extern void aac_define_int_mode(struct aac_dev *dev);
void aac_release_resources(struct aac_dev *aac)
{
	aac_adapter_disable_int(aac);
	aac_free_irq(aac);
}

#if (defined(CONFIG_PM))
static int aac_acquire_resources(struct aac_dev *dev)
{
	unsigned long status;

	/*
	 *	First clear out all interrupts.  Then enable the one's that we
	 *	can handle.
	 */
	while (!((status=src_readl(dev, MUnit.OMR)) & KERNEL_UP_AND_RUNNING) ||
		status == 0xffffffff)
			msleep(1);

	aac_adapter_disable_int(dev);
	aac_adapter_enable_int(dev);

	if (aac_is_srcv(dev))
		aac_define_int_mode(dev);

	if (dev->msix_enabled)
		aac_src_access_devreg(dev, AAC_ENABLE_MSIX);

	if(aac_acquire_irq(dev))
		goto error_iounmap;

	aac_adapter_enable_int(dev);

	/*
	 * max msix may change after EEH
	 * re-assign the vectors to FIBs
	 */
	aac_fib_vector_assign(dev);

	/*
	 * After EEH recovery or suspend resume max_msix count may change
	 * this is will take care of the same
	 */
	if (!dev->sync_mode) {
		dev->init->r7.NoOfMSIXVectors = cpu_to_le32(dev->max_msix);
		aac_adapter_start(dev);
	}

#if defined(__powerpc__) || defined(__PPC__) || defined(__ppc__)
	aac_get_config_status(dev, 1);
#endif

	return 0;

error_iounmap:
	return -1;
}

static int aac_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct aac_dev *aac = shost_priv(shost);

	scsi_block_requests(shost);
	aac_cancel_workers(aac);

	aac_send_shutdown(aac);

	aac_release_resources(aac);

	pci_set_drvdata(pdev, shost);
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

static int aac_resume(struct pci_dev *pdev) {
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct aac_dev *aac = shost_priv(shost);
	int r;

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake(pdev, PCI_D0, 0);
	pci_restore_state(pdev);
	r = pci_enable_device(pdev);
	if (r)
		goto fail_device;

	pci_set_master(pdev);
	if (aac_acquire_resources(aac))
		goto fail_device;
	/*
	* reset this flag to unblock ioctl() as it was set at aac_send_shutdown() to block ioctls from upperlayer
	*/
	aac->adapter_shutdown = 0;
	scsi_unblock_requests(shost);

        return 0;

fail_device:
	printk(KERN_INFO "%s%d: resume failed.\n", aac->name, aac->id);
	scsi_host_put(shost);
	pci_disable_device(pdev);
	return -ENODEV;
}
#endif

static void aac_shutdown(struct pci_dev *dev)
{
	struct Scsi_Host *shost = pci_get_drvdata(dev);

	scsi_block_requests(shost);
	__aac_shutdown(shost_priv(shost));
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
static void __devexit aac_remove_one(struct pci_dev *pdev)
#else
static void aac_remove_one(struct pci_dev *pdev)
#endif
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct aac_dev *aac = shost_priv(shost);

	aac_remove_host(aac);

	__aac_shutdown(aac);
	aac_fib_map_free(aac);
	aac_pci_free_consistent(aac->pdev, aac->comm_size, aac->comm_addr,
			aac->comm_phys);
	kfree(aac->queues);

	aac_adapter_ioremap(aac, 0);
	
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
	kfree(aac->reply_map);
#endif

	kfree(aac->fibs);
	kfree(aac->fsa_dev);

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	spin_lock_destroy(&aac->fib_lock);
#endif
	list_del(&aac->entry);
	scsi_host_put(shost);
	pci_disable_device(pdev);
#if (!defined(HAS_BOOT_CONFIG))
	if (list_empty(&aac_devices)) {
		unregister_chrdev(aac_cfg_major, "aac");
		aac_cfg_major = AAC_CHARDEV_NEEDS_REINIT;
	}
#endif
	return;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
void aac_flush_ios(struct aac_dev *aac)
{
	struct fib *fib;
	struct scsi_cmnd *cmd;

	for_each_fib(fib, aac) {
		cmd = (struct scsi_cmnd *)fib->callback_data;

		if (cmd && (cmd->SCp.phase == AAC_OWNER_FIRMWARE)) {
			scsi_dma_unmap(cmd);
			aac_fib_free(fib);
			if ((aac->pci_error_state)||(aac->adapter_panic == 1))
				cmd->result = DID_NO_CONNECT << 16;
			else
				cmd->result = DID_RESET << 16;
			cmd->scsi_done(cmd);
		}
	}
}

static pci_ers_result_t aac_pci_error_detected(struct pci_dev *pdev,
					enum pci_channel_state error)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct aac_dev *aac = shost_priv(shost);

	aac->pci_error_state = error;
	aac->adapter_shutdown = 1;

	aac_info(aac, "PCI error detected %x\n", error);

	switch (error) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:

		scsi_block_requests(aac->scsi_host_ptr);
		aac_cancel_workers(aac);
		aac_flush_ios(aac);
		aac_release_resources(aac);

		pci_disable_pcie_error_reporting(pdev);
		aac_adapter_ioremap(aac, 0);

		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		aac_flush_ios(aac);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t aac_pci_mmio_enabled(struct pci_dev *pdev)
{
	struct aac_dev *aac = shost_priv(pci_get_drvdata(pdev));

	aac_info(aac, "PCI error - mmio enabled\n");
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t aac_pci_slot_reset(struct pci_dev *pdev)
{
	struct aac_dev *aac = shost_priv(pci_get_drvdata(pdev));

	aac_info(aac, "PCI error - slot reset\n");

	pci_restore_state(pdev);
	if (pci_enable_device(pdev)) {
		aac_warn(aac, "PCI error - failed to enable slave\n");
		goto fail_device;
	}

	pci_set_master(pdev);

	if (pci_enable_device_mem(pdev)) {
		aac_err(aac, "pci_enable_device_mem failed\n");
		goto fail_device;
	}

	return PCI_ERS_RESULT_RECOVERED;

fail_device:
	aac_err(aac, "PCI error - slot reset failed\n");
	return PCI_ERS_RESULT_DISCONNECT;
}

static void aac_pci_resume(struct pci_dev *pdev)
{
	struct Scsi_Host *shost = pci_get_drvdata(pdev);
	struct scsi_device *sdev = NULL;
	struct aac_dev *aac = shost_priv(shost);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4,17,19))
	pci_cleanup_aer_uncorrect_error_status(pdev);
#endif

	switch (aac->pci_error_state) {
	case pci_channel_io_normal:
		break;
	case pci_channel_io_frozen:
		if (aac_adapter_ioremap(aac, aac->base_size)) {
			aac_err(aac, "ioremap failed\n");
			/* remap failed, go back ... */
			aac->comm_interface = AAC_COMM_PRODUCER;
			if (aac_adapter_ioremap(aac, AAC_MIN_FOOTPRINT_SIZE)) {
				aac_warn(aac, "Unable to map adapter.\n");
				return;
			}
		}

		msleep(10000);

		aac_acquire_resources(aac);
		break;
	case pci_channel_io_perm_failure:
		break;
	}

	/*
	 * reset this flag to unblock ioctl() as it was set at aac_send_shutdown() to block ioctls from upperlayer
	 */
	aac->adapter_shutdown = 0;
	aac->pci_error_state = 0;

	shost_for_each_device(sdev, shost) {
		if (!scsi_device_online(sdev))
			scsi_device_set_state(sdev, SDEV_RUNNING);
	}

	scsi_unblock_requests(shost);
	aac_scan_host(aac, AAC_REINIT);
	pci_save_state(pdev);

	aac_info(aac, "PCI error - resume\n");
}

static struct pci_error_handlers aac_pci_err_handler = {
	.error_detected 	= aac_pci_error_detected,
	.mmio_enabled 		= aac_pci_mmio_enabled,
	.slot_reset 		= aac_pci_slot_reset,
	.resume 		= aac_pci_resume,
};
#endif

static struct pci_driver aac_pci_driver = {
	.name		= AAC_DRIVERNAME,
	.id_table	= aac_pci_tbl,
	.probe		= aac_probe_one,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
	.remove		= __devexit_p(aac_remove_one),
#else
	.remove		= aac_remove_one,
#endif
#if (defined(CONFIG_PM))
	.suspend	= aac_suspend,
	.resume		= aac_resume,
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)) && ((LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)) || defined(PCI_HAS_SHUTDOWN)))
	.shutdown	= aac_shutdown,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
	.err_handler    = &aac_pci_err_handler,
#endif
};

static int __init aac_init(void)
{
	int error;

#if defined(AAC_SAS_TRANSPORT)
	aac_sas_transport_template = sas_attach_transport(&aac_sas_transport_functions);
	if (!aac_sas_transport_template)
		return -ENODEV;
#endif

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
#if (!defined(__VMKLNX__))
	if (!vmk_set_module_version(AAC_DRIVERNAME " (" AAC_DRIVER_BUILD_DATE ")"))
		return 0;
#endif
	spin_lock_init(&io_request_lock);
#if (!defined(__VMKLNX__))
	aac_driver_template.driverLock = &io_request_lock;
#endif
#endif
	printk(KERN_INFO "Adaptec %s driver %s\n",
	  AAC_DRIVERNAME, aac_driver_version);
#if (defined(__VMKLNX30__))
	if (!vmklnx_set_module_version("%s", aac_driver_version))
		return -ENODEV;
#endif

	error = pci_register_driver(&aac_pci_driver);

	if (error < 0 || list_empty(&aac_devices)) {
		if (error >= 0) {
			pci_unregister_driver(&aac_pci_driver);
			error = -ENODEV;
		}
#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
		spin_lock_destroy(&io_request_lock);
#endif
		return error;
	}

	aac_init_char();

	return 0;
}

static void __exit aac_exit(void)
{
	if (aac_cfg_major > -1) {
		unregister_chrdev(aac_cfg_major, "aac");
		aac_cfg_major = -1;
	}
	pci_unregister_driver(&aac_pci_driver);

#if defined(AAC_SAS_TRANSPORT)
	sas_release_transport(aac_sas_transport_template);
#endif

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX30__) || defined(__VMKLNX__))
	spin_lock_destroy(&io_request_lock);
#endif
}

module_init(aac_init);
module_exit(aac_exit);
