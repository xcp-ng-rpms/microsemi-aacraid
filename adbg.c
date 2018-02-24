
/*
 * Adaptec AAC series RAID controller driver
 * (c) Copyright 2001 Red Hat Inc.<alan@redhat.com>
 *
 * based on the old aacraid driver that is..
 * Adaptec aacraid device driver for Linux.
 *
 * Copyright (c) 2000-2007 Adaptec, Inc. (aacraid@adaptec.com)
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
 *  adbg.c
 *
 * Abstract: Contains debug routines
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/blkdev.h>

#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>

#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX30__) && !defined(__VMKLNX__))
#include <scsi/scsi_transport_sas.h>
#endif

#include "aacraid.h"
#include "adbg.h"
#include "fwdebug.h"

#if defined(AAC_SAS_TRANSPORT)
unsigned int aac_compat_bsg_job_req_bytes(struct aac_compat_bsg_job *smp_job)
{
#if defined(AAC_SAS_SMP_BSG_JOB)
	return smp_job->bsg_job->request_payload.payload_len;
#else
	return blk_rq_bytes(smp_job->smp_req);
#endif
}

unsigned int aac_compat_bsg_job_resp_bytes(struct aac_compat_bsg_job *smp_job)
{
#if defined(AAC_SAS_SMP_BSG_JOB)
	return smp_job->bsg_job->reply_payload.payload_len;
#else
	return blk_rq_bytes(smp_job->smp_req->next_rq);
#endif
}

void aac_compat_bsg_job_req_data(struct aac_compat_bsg_job *smp_job,
					void **buf)
{
#if defined(AAC_SAS_SMP_BSG_JOB)
	sg_copy_to_buffer(smp_job->bsg_job->request_payload.sg_list, 1, *buf,
		smp_job->bsg_job->request_payload.payload_len);
#else
	*buf = bio_data(smp_job->smp_req->bio);
#endif
}

void aac_compat_bsg_job_resp_data(struct aac_compat_bsg_job *smp_job,
					void **buf)
{
#if defined(AAC_SAS_SMP_BSG_JOB)
	sg_copy_to_buffer(smp_job->bsg_job->reply_payload.sg_list, 1, *buf,
		smp_job->bsg_job->reply_payload.payload_len);
#else
	*buf = bio_data(smp_job->smp_req->next_rq->bio);
#endif
}

int aac_compat_bsg_job_response_space(struct aac_dev *dev,
					struct aac_compat_bsg_job *smp_job)
{
#if defined(AAC_SAS_SMP_BSG_JOB)
	return aac_compat_bsg_job_resp_bytes(smp_job) != 0;
#else
	return smp_job->smp_req->next_rq != NULL;

#endif
}

int aac_compat_bsg_job_multiple_segments(struct aac_dev *dev,
					struct aac_compat_bsg_job *smp_job)
{
#if defined(AAC_SAS_SMP_BSG_JOB)
	struct bsg_job *aac_job = smp_job->bsg_job;

	return (aac_job->request_payload.sg_cnt > 1 ||
			aac_job->reply_payload.sg_cnt > 1);
#else
	struct request *smp_req = smp_job->smp_req;
	struct request *smp_resp = smp_req->next_rq;

	return (bio_multiple_segments(smp_req->bio) ||
			bio_multiple_segments(smp_resp->bio));
#endif
}

void aac_compat_build_bsg_smp_request(struct aac_smp_request *smp_request,
					struct aac_compat_bsg_job *smp_job)
{
#if defined(AAC_SAS_SMP_BSG_JOB)
	sg_copy_to_buffer(smp_job->bsg_job->request_payload.sg_list,
			1, smp_request, smp_job->bsg_job->request_payload.payload_len);
#else
	void *smp_req_cmd = NULL;
	u32 smp_req_cmd_size = 0;

	smp_req_cmd_size = aac_compat_bsg_job_req_bytes(smp_job);
	aac_compat_bsg_job_req_data(smp_job, &smp_req_cmd);

	memcpy(smp_request, smp_req_cmd, smp_req_cmd_size);
#endif
}

void aac_compat_build_bsg_smp_response(struct aac_smp_response *smp_response,
					struct aac_compat_bsg_job *smp_job)
{
#if defined(AAC_SAS_SMP_BSG_JOB)
	sg_copy_from_buffer(smp_job->bsg_job->reply_payload.sg_list, 1, smp_response,
		smp_job->bsg_job->reply_payload.payload_len);
#else
	void *smp_resp_cmd = NULL;
	u32 smp_resp_cmd_size = 0;

	smp_resp_cmd_size = aac_compat_bsg_job_resp_bytes(smp_job);
	aac_compat_bsg_job_resp_data(smp_job, &smp_resp_cmd);

	memcpy(smp_resp_cmd, smp_response, smp_resp_cmd_size);
#endif
}

void aac_compat_build_bsg_job_reply(struct aac_srb_reply *smp_reply,
					struct aac_compat_bsg_job *smp_job)
{
	u32 len = min_t(u32, le32_to_cpu(smp_reply->sense_data_size), SCSI_SENSE_BUFFERSIZE);

	/*
	 * unfortunatly for smp commands we do not support residual information
	 */
#if defined(AAC_SAS_SMP_BSG_JOB)
	struct bsg_job *bsg_job = smp_job->bsg_job;

	bsg_job->reply_len = len;
	memcpy(bsg_job->reply, smp_reply->sense_data, len);

	smp_job->reslen = aac_compat_bsg_job_resp_bytes(smp_job);
#else

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0))
	struct request *smp_req = smp_job->smp_req;
	struct request *smp_resp = smp_req->next_rq;
#else
	struct scsi_request *smp_req = scsi_req(smp_job->smp_req);
	struct scsi_request *smp_resp = scsi_req(smp_job->smp_req->next_rq);
#endif

	smp_req->sense_len = len;
	memcpy(smp_req->sense, smp_reply->sense_data, len);

	smp_req->resid_len = 0;
	smp_resp->resid_len = 0;
#endif
}
#endif

void print_raw_buffer(struct aac_dev *aac, u8 *buf, u32 buf_size, char  *name)
{
	unsigned len = buf_size;
	char buffer[80];
	char * cp = buffer;

	strncpy(cp, name, 4);
	cp += 4;
	while (len > 0) {
		if (cp >= &buffer[sizeof(buffer)-4]) {
			printk(KERN_INFO "%s\n", buffer);
			strncpy(cp = buffer, "    ", 4);
			cp += 4;
		}
		sprintf (cp, "%02x ", *(buf++));
		cp += strlen(cp);
		--len;
	}
	if (cp > &buffer[4])
		printk(KERN_INFO "%s\n", buffer);
}

void print_sg_info64(struct aac_dev *dev, struct sgmap64 *psg)
{
	int i, nseg = le32_to_cpu(psg->count);
	for (i = 0; i < nseg; i++) {
		int count = le32_to_cpu(psg->sg[i].count);
		u32 addr0 = le32_to_cpu(psg->sg[i].addr[0]);
		u32 addr1 = le32_to_cpu(psg->sg[i].addr[1]);
		if (addr1 == 0)
			printk(KERN_INFO "%x[%d]", addr0, count);
		else
			printk(KERN_INFO " %x%7x[%d]", addr1, addr0, count);
	}
	printk(KERN_INFO "\n");
}

void print_sg_info32(struct aac_dev *dev, struct sgmap *psg)
{
	int i, nseg = le32_to_cpu(psg->count);
	for (i = 0; i < nseg; i++) {
		int count = le32_to_cpu(psg->sg[i].count);
		u32 addr = le32_to_cpu(psg->sg[i].addr);
		printk(KERN_INFO " %x[%d]", addr, count);
	}
	printk(KERN_INFO "\n");
}

void dump_srb(struct aac_dev *aac, void *buf, u32 buf_size)
{
	struct user_aac_srb *usrb = NULL;

	usrb = (struct user_aac_srb *)buf;

	aac_info(aac, "function      :%d", usrb->function);
	aac_info(aac, "channel       :%d", usrb->channel);
	aac_info(aac, "id            :%d", usrb->id);
	aac_info(aac, "lun           :%d", usrb->lun);
	aac_info(aac, "timeout       :%d", usrb->timeout);
	aac_info(aac, "flags         :%x", usrb->flags);
	aac_info(aac, "count         :%d", usrb->count);
	aac_info(aac, "retry_limit   :%d", usrb->retry_limit);
	aac_info(aac, "cdb_size      :%d", usrb->cdb_size);
	print_raw_buffer(aac, usrb->cdb, usrb->cdb_size, "cdb= ");

	//Print sg_info here
}

void dump_srb_reply(struct aac_dev *aac, void *buf, u32 buf_size)
{
	struct user_aac_srb_reply *usrb_rep = NULL;

	usrb_rep = (struct user_aac_srb_reply *)buf;

	aac_info(aac, "status            :%d", usrb_rep->status);
	aac_info(aac, "srb_status        :%d", usrb_rep->srb_status);
	aac_info(aac, "scsi_status       :%d", usrb_rep->scsi_status);
	aac_info(aac, "data_xfer_length  :%d", usrb_rep->data_xfer_length);
	aac_info(aac, "sense_data_size   :%d", usrb_rep->sense_data_size);
	print_raw_buffer(aac, usrb_rep->sense_data,
					usrb_rep->sense_data_size, "SNS= ");
}

void dump_srb_unit(struct aac_dev *aac, void *buf, u32 buf_size)
{
	struct user_aac_srb_unit *usrb_unit = NULL;

	usrb_unit = (struct user_aac_srb_unit *)buf;

	aac_info(aac, "****** SRB ******\n");
	dump_srb(aac, (void *)&usrb_unit->usrb, sizeof(struct user_aac_srb));
	aac_info(aac, "****** SRB REPLY ******\n");
	dump_srb_reply(aac, (void *)&usrb_unit->usrb_reply, sizeof(struct user_aac_srb_reply));
}

#if defined(AAC_SAS_TRANSPORT)
void ioctl_dump_srb_csmi_reply(struct aac_dev *aac, struct user_aac_srb *usrb,
				const char *name, void *buf, u32 buf_size)
{
	if (usrb->cdb[5] != CSMI_CC_SAS_SMP_PASSTHRU)
		return;

	adbg_smp(aac, KERN_INFO, "%s\n", name);
	dump_srb_reply(aac, (u8 *)buf, buf_size);
}

void dump_srb_csmi(struct aac_dev *aac, void *buf, u32 buf_size)
{
	struct aac_csmi_smp_cmd *csmid = NULL;
	struct aac_csmi_header  *ioh = NULL;
	struct aac_csmi_smp_passthru *pt = NULL;
	struct aac_smp_request  *srq = NULL;
	struct aac_smp_response *srsp = NULL;

	csmid = (struct aac_csmi_smp_cmd *)buf;
	ioh = &csmid->ioctl_header;
	pt = &csmid->params;
	srq  = &pt->smp_request;
	srsp = &pt->smp_response;

	aac_info(aac, "ioctl   :header_length : %x\n", ioh->header_length);
	print_raw_buffer(aac, ioh->signature, 8, "SIG= ");
	aac_info(aac, "ioctl   :timeout       : %x\n", ioh->timeout);
	aac_info(aac, "ioctl   :control_code  : %x\n", ioh->control_code);
	aac_info(aac, "ioctl   :return_code   : %x\n", ioh->return_code);
	aac_info(aac, "ioctl   :length        : %x\n", ioh->length);

	aac_info(aac, "passthru:phy_identifier   : %x\n", pt->phy_identifier);
	aac_info(aac, "passthru:port_identifier  : %x\n", pt->port_identifier);
	aac_info(aac, "passthru:connection_rate  : %x\n", pt->connection_rate);
	aac_info(aac, "passthru:reserved         : %x\n", pt->reserved);
	print_raw_buffer(aac, pt->destination_sas_address, 8, "SAS= ");
	aac_info(aac, "passthru:request_length   : %x\n", pt->request_length);

	aac_info(aac, "request :frame_type       : %x\n", srq->frame_type);
	aac_info(aac, "request :function         : %x\n", srq->function);
	aac_info(aac, "request :allocated_response_length : %x\n", srq->allocated_response_length);
	aac_info(aac, "request :request_length   : %x\n", srq->request_length);
	print_raw_buffer(aac, srq->additional_request_bytes, 16, "REQ= ");

	aac_info(aac, "passthru:connection_status: %x\n", pt->connection_status);
	print_raw_buffer(aac, pt->reserved2, 3, "RSR= ");
	aac_info(aac, "passthru:response_bytes   : %x\n", pt->response_bytes);

	aac_info(aac, "response:frame_type       : %x\n", srsp->frame_type);
	aac_info(aac, "response:function         : %x\n", srsp->function);
	aac_info(aac, "response:function_result  : %x\n", srsp->function_result);
	aac_info(aac, "response:response_length  : %x\n", srsp->response_length);
	print_raw_buffer(aac, srsp->additional_response_bytes, 16, "RSP= ");
}

void ioctl_dump_srb_csmi(struct aac_dev *aac, struct user_aac_srb *usrb,
				const char *name, void *buf, u32 buf_size)
{
	if (usrb->cdb[5] != CSMI_CC_SAS_SMP_PASSTHRU)
		return;

	adbg_smp(aac, KERN_INFO, "%s\n", "CSMI REQUEST SRB");
	dump_srb(aac, (u8 *)usrb, sizeof(struct user_aac_srb));
	adbg_smp(aac, KERN_INFO, "%s\n", name);
	dump_srb_csmi(aac, (u8 *)buf, buf_size);
}

#define adbg_smp_print_cmd(AAC, BUF, SIZE) print_raw_buffer(AAC, BUF, SIZE, "SMP= ")
void print_smp_request(struct aac_dev *aac, struct aac_compat_bsg_job *smp_job, char *header)
{
	void *req_cmd = NULL;
	u32 req_size = 0;
	void *resp_cmd = NULL;
	u32 resp_size = 0;

	req_size = aac_compat_bsg_job_req_bytes(smp_job);
	resp_size = aac_compat_bsg_job_resp_bytes(smp_job);

#if defined(AAC_SAS_SMP_BSG_JOB)
	req_cmd = kzalloc(req_size, GFP_ATOMIC);
	if(!req_cmd)
		return;

	resp_cmd = kzalloc(resp_size, GFP_ATOMIC);
	if(!resp_cmd)
		return;
#endif
	aac_compat_bsg_job_req_data(smp_job, &req_cmd);
	aac_compat_bsg_job_resp_data(smp_job, &resp_cmd);

	adbg_smp(aac, KERN_INFO, "********* %s BEGIN ********* \n", header);

	adbg_smp(aac, KERN_INFO, "request_len:%x", req_size);
	adbg_smp_print_cmd(aac, req_cmd, req_size);

	adbg_smp(aac, KERN_INFO, "respond_len:%x", resp_size);
	adbg_smp_print_cmd(aac, resp_cmd, resp_size);

	adbg_smp(aac, KERN_INFO, "********* %s END *********\n", header);


#if defined(AAC_SAS_SMP_BSG_JOB)
	kfree(req_cmd);
	kfree(resp_cmd);
#endif
}

void dump_smp_srb_requst(struct aac_dev *aac, struct aac_srb_unit *srbu,
				struct aac_csmi_smp_cmd *smp_cmd,
				struct aac_compat_bsg_job *smp_job)
{
	if(smp_job)
		print_smp_request(aac, smp_job, "SMP REQUEST/RESPONSE");

	if(smp_cmd) {
		adbg_smp(aac, KERN_INFO, "CSMI SMP BUFFER\n");
		dump_srb_csmi(aac, smp_cmd, sizeof(struct aac_csmi_smp_cmd));
	}
	if(srbu) {
		dump_srb_unit(aac, srbu, sizeof(struct aac_srb_unit));
	}
}
#endif

#ifdef AAC_DEBUG_INSTRUMENT_RESET
void dump_pending_fibs(struct aac_dev *aac, struct scsi_cmnd *cmd)
{
	int i;
	u32 handle=0, isFastResponse=0;
	char	strstate[9][20] = {	"FREE",
					"FREED_AIF",
					"FREED_IO",
					"ALLOCATED_AIF",
					"ALLOCATED_IO",
					"INITIALIZED",
					"IO_SUBMITTED",
					"PRE_INT",
					"POST_INT"
				};

	adbg_reset(aac, KERN_ERR, " \n********* Pending FIBs ********* \n");
	for (i=0; i<1024; i++)
	{
		if ((atomic_read(&aac->fibs[i].state) > DBG_STATE_FREED_IO)
			&& (atomic_read(&aac->fibs[i].state) != DBG_STATE_POST_INT))
		{
			u32 state = atomic_read(&aac->fibs[i].state);
			if ((cmd != NULL) && (aac->fibs[i].callback_data == cmd))
				adbg_reset(aac, KERN_ERR, "* (s)pending fib: %04u - %s",
					i, strstate[state]);
			else if (aac->fibs[i].callback_data)
				adbg_reset(aac, KERN_ERR, "  (s)pending fib: %04u - %s",
					i, strstate[state]);
			else
				adbg_reset(aac, KERN_ERR, "  ( )pending fib: %04u - %s",
					i, strstate[state]);

		}
	}

	adbg_reset(aac, KERN_ERR, " \n********* Host RRQ Pending handles ********* \n");
	for (i=0; i<1024; i++)
	{

		if (aac->host_rrq[i])
		handle = le32_to_cpu((aac->host_rrq[i])
				& 0x7fffffff);
		/* check fast response bits (30, 1) */
		if (handle & 0x40000000)
			isFastResponse = 1;
		handle &= 0x0000ffff;

		if (handle)
		{
			handle >>= 2;
			adbg_reset(aac, KERN_ERR, " HOST_RRQ[%04u] : %04u, Vec: %02u, FastResponse: %u, FIB Status: %s\n", i,
					handle,
					i/aac->vector_cap,
					isFastResponse,
					strstate[atomic_read(&aac->fibs[handle].state)]);
		}
	}

	adbg_reset(aac, KERN_ERR, " \n\n********* Host RRQ Indexs ********* \n");
	for (i=0; i<aac->max_msix; i++)
	{
		adbg_reset(aac, KERN_ERR, "  host_rrq index vec[%02u]: %04u\n",i, aac->host_rrq_idx[i]);
	}

	adbg_reset(aac, KERN_ERR, "\n\n");

	return;
}
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_RESET) || (0 && defined(BOOTCD)))
int dump_command_queue(struct scsi_cmnd* cmd)
{
	struct scsi_device *dev = cmd->device;
	struct Scsi_Host *host = dev->host;
	struct aac_dev *aac = shost_priv(host);
	struct scsi_cmnd *command;
	int active=0;
	unsigned long flags;

	spin_lock_irqsave(&dev->list_lock, flags);
	list_for_each_entry(command, &dev->cmd_list, list) {
		fwprintf((aac, HBA_FLAGS_DBG_FW_PRINT_B,
		 "%4d %c%c %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		  	 active,
		  	 (command->SCp.phase == AAC_OWNER_FIRMWARE) ? 'A' : 'C',
		  	 (cmd == command) ? '*' : ' ',
		  	 command->cmnd[0], command->cmnd[1], command->cmnd[2],
		  	 command->cmnd[3], command->cmnd[4], command->cmnd[5],
		  	 command->cmnd[6], command->cmnd[7], command->cmnd[8],
		  	 command->cmnd[9]));
		printk(KERN_ERR
		 "%4d %c%c %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		  	 active,
		  	 (command->SCp.phase == AAC_OWNER_FIRMWARE) ? 'A' : 'C',
		  	 (cmd == command) ? '*' : ' ',
		  	 command->cmnd[0], command->cmnd[1], command->cmnd[2],
		  	 command->cmnd[3], command->cmnd[4], command->cmnd[5],
		  	 command->cmnd[6], command->cmnd[7], command->cmnd[8],
		  	 command->cmnd[9]);
			++active;
	}
	spin_unlock_irqrestore(&dev->list_lock, flags);

	return active;
}
#endif

#if (defined(AAC_DEBUG_INSTRUMENT_AAC_CONFIG))
void debug_aac_config(struct scsi_cmnd* scsicmd, __le32 count, unsigned long byte_count)
{
	struct aac_dev *dev = shost_priv(scsicmd->device->host);

	if (le32_to_cpu(count) > aac_config.peak_sg) {
		aac_config.peak_sg  = le32_to_cpu(count);
		adbg_conf(dev, KERN_INFO, "peak_sg=%u\n", aac_config.peak_sg);
	}

	if (byte_count > aac_config.peak_size) {
		aac_config.peak_size = byte_count;
		adbg_conf(dev, KERN_INFO, "peak_size=%u\n", aac_config.peak_size);
	}
}
#endif


