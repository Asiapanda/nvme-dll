#include <fcntl.h>
#include <unistd.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "nvme-dll.h"
#include "linux/nvme_ioctl.h"
#include "linux/nvme.h"
#include "nvme-print.h"
#include "nvme-ioctl.h"
#include "suffix.h"
#include "nvme.h"

#define array_len(x) ((size_t)(sizeof(x) / sizeof(x[0])))
#define min(x, y) ((x) > (y) ? (y) : (x))
#define max(x, y) ((x) > (y) ? (x) : (y))
#define DEVPATH "/dev/"
static struct stat nvme_stat;

int open_dev(char *path)
{
	int err;
	int fd;
	fd = open(path, O_RDONLY);
	if(fd < 0)
		return -1;
	err = fstat(fd, &nvme_stat);
	if(err < 0){
		close(fd);	
		return -2;
	}
	if(!S_ISCHR(nvme_stat.st_mode) && !S_ISBLK(nvme_stat.st_mode)){
		fprintf(stderr, "%s is not a block or character device\n", path);
		return -3;
	}
	return fd;
}
static void print_list_item(struct list_item list_item)
{
	long long int lba	= 1 << list_item.ns.lbaf[(list_item.ns.flbas & 0x0f)].ds;
	double nsze			= le64_to_cpu(list_item.ns.nsze) * lba;
	double nuse			= le64_to_cpu(list_item.ns.nsze) * lba;

	const char *s_suffix = suffix_si_get(&nsze);
	const char *u_suffix = suffix_si_get(&nuse);
	const char *l_suffix = suffix_binary_get(&lba);

	char usage[128];
	char format[128];

	sprintf(usage, "%6.2f %2sB / %6.2f %2sB", nuse, u_suffix, nsze, s_suffix);
	sprintf(format, "%3.0f %2sB + %2d B", (double)lba, l_suffix, list_item.ns.lbaf[(list_item.ns.flbas & 0x0f)].ms);
	printf("%-16s %-*.*s %-*.*s %-9d %-26s %-16s %-.*s\n", list_item.node, 
			(int)sizeof(list_item.ctrl.sn), (int)sizeof(list_item.ctrl.sn), list_item.ctrl.sn,
			(int)sizeof(list_item.ctrl.mn), (int)sizeof(list_item.ctrl.mn), list_item.ctrl.mn,
			list_item.nsid, usage, format, (int)sizeof(list_item.ctrl.fr), list_item.ctrl.fr);
}

static void print_list_items(struct list_item *list_items, unsigned len)
{
	unsigned i;

	printf("%-16s %-20s %-40s %-9s %-26s %-16s %-8s\n",
			"Node", "SN", "Model", "Namespace", "Usage", "Format", "FW Rev");
	printf("%-16s %-20s %-40s %-9s %-26s %-16s %-8s\n",
			 "----------------", "--------------------", "----------------------------------------",
		     "---------", "--------------------------", "----------------", "--------");
	for(i = 0; i < len; i++)
		print_list_item(list_items[i]);
}

static int get_nvme_info(int fd, struct list_item *item, const char *node)
{
	int err;

	err = nvme_identify_ctrl(fd, &item->ctrl);
	if(err)
		return err;
	item->nsid = nvme_get_nsid(fd);
	err = nvme_identify_ns(fd, item->nsid, 0, &item->ns);
	if(err)
		return err;
	strcpy(item->node, node);
	item->block = S_ISBLK(nvme_stat.st_mode);
	return 0;
}

/*Assume every block device starting with /dev/nvme is an nvme namespace*/
static int scan_dev_filter(const struct dirent *d)
{
	char path[264];
	struct stat bd;
	int ctrl, ns, part;
	
	if(d->d_name[0] == '.')
		return 0;

	if(strstr(d->d_name, "nvme")){
		snprintf(path, sizeof(path), "%s%s", DEVPATH, d->d_name);
		if(stat(path, &bd))
			return 0;
		if(!S_ISBLK(bd.st_mode))
			return 0;
		if(sscanf(d->d_name, "nvme%dn%dp%d", &ctrl, &ns, &part) == 3)
			return 0;
		return 1;
	}
	return 0;
}

/**************************************************
 *
 * 函数名称：GetDeviceList
 * 函数参数：无
 * 函数方法：获取NVMe驱动的设备列表
 *
 *************************************************/
int GetDeviceList()
{	
	char path[264];
	struct dirent **devices;
	int n, i, ret;
	int fd;
	struct list_item *list_items;

	n = scandir(DEVPATH, &devices, scan_dev_filter, alphasort);
	if(n < 0){
		fprintf(stderr, "no NVMe device(s) detected.\n");
		return n;
	}
	
	list_items = calloc(n, sizeof(*list_items));
	if(!list_items){
		fprintf(stderr, "can not allocate controller list payload.\n");
		return -1;
	}

	for(i = 0; i < n; i++){
		snprintf(path, sizeof(path), "%s%s", DEVPATH, devices[i]->d_name);
		fd = open(path, O_RDONLY);
		if(fd < 0){
			fprintf(stderr, "can not open %s: %s\n", path, strerror(errno));
			return errno;
		}
		ret = get_nvme_info(fd, &list_items[i], path);
		close(fd);
		if(ret)
			return ret;
	}

	print_list_items(list_items, n);

	for(i = 0; i < n; i++)
		free(devices[i]);
	free(devices);
	free(list_items);
	return 0;
}

/**************************************************
 *
 * 函数名称：DownfwDevice 
 * 函数参数：
 *		path: NVMe设备的路径，如果为NULL表示全部执行
 *		fwpath：firmware文件的路径
 * 函数方法：下载指定的fw到指定的设备上
 *
 *************************************************/
int DownfwDevice(char *path, char *fwpath)
{
	int fd, err, fw_fd = -1;
	unsigned int fw_size;
	struct stat sb;
	void *fw_buf;

	struct config {
		char *fw;
		__u32 xfer;
		__u32 offset;
	};

	struct config cfg = {
		.fw		= fwpath,
		.xfer   = 4096,
		.offset = 0,
	};
	fd = open_dev(path);
	if (fd < 0)
		return fd;
	fw_fd = open(cfg.fw, O_RDONLY);
	cfg.offset <<= 2;
	if (fw_fd < 0){
		fprintf(stderr, "no firmware file provided\n");
		return EINVAL;
	}

	err = fstat(fw_fd, &sb);
	if (err < 0){
		perror("fstat");
		return errno;
	}

	fw_size = sb.st_size;
	if (fw_size & 0x3){
		fprintf(stderr, "Invalid size:%d for f/w image\n", fw_size);
		return EINVAL;
	}
	if (posix_memalign(&fw_buf, getpagesize(), fw_size)) {
		fprintf(stderr, "No memory for f/w size:%d\n", fw_size);
		return ENOMEM;
	}
	if (cfg.xfer == 0 || cfg.xfer % 4096)
		cfg.xfer = 4096;
	if (read(fw_fd, fw_buf, fw_size) != ((ssize_t)(fw_size)))
		return EIO;

	while (fw_size > 0) {
		cfg.xfer = min(cfg.xfer, fw_size);

		err = nvme_fw_download(fd, cfg.offset, cfg.xfer, fw_buf);
		if (err < 0) {
			perror("fw-download");
			break;
		}else if (err != 0) {
			fprintf(stderr, "NVME Admin command error:%s(%x)\n", nvme_status_to_string(err), err);
			break;
		}
		fw_buf		+= cfg.xfer;
		fw_size		-= cfg.xfer;
		cfg.offset	+= cfg.xfer;
	}
	if (!err)
		printf("Firmware download success\n");
	close(fd);
	close(fw_fd);
	return err;
}

static int get_nsid(int fd)
{
	int nsid = nvme_get_nsid(fd);

	if(nsid <= 0)
	{
		fprintf(stderr, "failed to return namespace id\n");
		return errno;
	}
	return nsid;
}

/**************************************************
 *
 * 函数名称：FormatDevice
 * 函数参数：
 *		path：NVMe设备的路径，如果为NULL表示全部执行
 * 函数方法：对指定的设备进行format
 *
 *************************************************/
int FormatDevice(char *path)
{
	int fd, err;
	struct nvme_id_ns ns;
	__u8 prev_lbaf = 0;
	struct config{
		__u32 namespace_id;
		__u32 timeout;
		__u8  lbaf;
		__u8  ses;
		__u8  pi;
		__u8  pil;
		__u8  ms;
		int   reset;
	};

	struct config cfg = {
		.namespace_id = NVME_NSID_ALL,
		.timeout	  = 600000,
		.lbaf		  = 0xff,
		.ses		  = 0,
		.pi			  = 0,
		.reset		  = 0,
	};
	fd = open_dev(path);
	if(fd < 0)
		return -1;

	if (S_ISBLK(nvme_stat.st_mode))
		cfg.namespace_id = get_nsid(fd);
	if (cfg.namespace_id != NVME_NSID_ALL){
		err = nvme_identify_ns(fd, cfg.namespace_id, 0, &ns);
		if(err){
			if(err < 0)
				perror("identify-namespace");
			else
				fprintf(stderr, "NVME Admin command error:%s(%x)\n", nvme_status_to_string(err), err);
			return err;
		}
		prev_lbaf = ns.flbas & 0xf;
	}
	if (cfg.lbaf == 0xff)
		cfg.lbaf = prev_lbaf;

	// see & pi checks set to 7 for forware-compatibility
	if (cfg.ses > 7){
		fprintf(stderr, "invalid secure erase settings:%d\n", cfg.ses);
		return EINVAL;
	}
	if (cfg.lbaf > 15){
		fprintf(stderr, "invalid lbaf:%d\n", cfg.lbaf);
		return EINVAL;
	}
	if (cfg.pi > 7){
		fprintf(stderr, "invalid pi:%d\n", cfg.pi);
		return EINVAL;
	}
	if (cfg.pil > 1){
		fprintf(stderr, "invalid pil:%d\n", cfg.pil);
		return EINVAL;
	}
	if (cfg.ms > 1){
		fprintf(stderr, "invalid ms:%d\n", cfg.ms);
		return EINVAL;
	}

	err = nvme_format(fd, cfg.namespace_id, cfg.lbaf, cfg.ses, cfg.pi,
			cfg.pil, cfg.ms, cfg.timeout);
	if (err < 0)
		perror("format");
	else if (err != 0)
		fprintf(stderr, "NVME Admin command error:%s(%x)\n",
				nvme_status_to_string(err), err);
	else {
		printf("Success formatting namespace:%x\n", cfg.namespace_id);
		ioctl(fd, BLKRRPART);
		if(cfg.reset && S_ISCHR(nvme_stat.st_mode))
			nvme_reset_controller(fd);
	}
	return err;
}

static char *nvme_fw_status_reset_type(__u32 status)
{
	switch (status & 0x3ff) {
	case NVME_SC_FW_NEEDS_CONV_RESET:	return "conventional";
	case NVME_SC_FW_NEEDS_SUBSYS_RESET:	return "subsystem";
	case NVME_SC_FW_NEEDS_RESET:		return "any controller";
	default:							return "unknown";
	}
}

/**************************************************
 *
 * 函数名称：ActivateDevice
 * 函数参数：
 *		path：NVMe设备的路径
 *		slot：设备的槽位置，可通过帮助文档找到此值
 *		action：
 * 函数方法：对指定的设备进行激活
 *
 *************************************************/
int ActivateDevice(char *path, int slot, int action)
{
	int err, fd;

	fd = open_dev(path);
	if (fd < 0)
		return fd;

	if (slot > 7) {
		fprintf(stderr, "invalid slot:%d\n", slot);
		return EINVAL;
	}
	if (action > 7 || action == 4 || action == 5) {
		fprintf(stderr, "invalid action:%d\n", action);
		return EINVAL;
	}

	err = nvme_fw_activate(fd, slot, action);
	if (err < 0)
		perror("fw-activate");
	else if (err != 0)
		switch (err & 0x3ff) {
		case NVME_SC_FW_NEEDS_CONV_RESET:
		case NVME_SC_FW_NEEDS_SUBSYS_RESET:
		case NVME_SC_FW_NEEDS_RESET:
			printf("Success activating firmware action:%d slot:%d. but firmware requires %s reset\n",
					action, slot, nvme_fw_status_reset_type(err));
			break;
		default:
			fprintf(stderr, "NVME Admin command error:%s(%x)\n", 
					nvme_status_to_string(err), err);
			break;
		}
	else 
		printf("Success activating firmware action:%d slot:%d\n",
				action, slot);
	return err;
}
