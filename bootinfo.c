/*
 * (C) Copyright 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
 *
 * display information about sunxi boot headers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "types.h"

/* boot_file_head copied from mksunxiboot */
/* boot head definition from sun4i boot code */
typedef struct boot_file_head
{
	u32  jump_instruction;   // one intruction jumping to real code
	u8   magic[8];           // ="eGON.BT0" or "eGON.BT1",  not C-style string.
	u32  check_sum;          // generated by PC
	u32  length;             // generated by PC
	u32  pub_head_size;      // the size of boot_file_head_t
	u8   pub_head_vsn[4];    // the version of boot_file_head_t
	u8   file_head_vsn[4];   // the version of boot0_file_head_t or boot1_file_head_t
	u8   Boot_vsn[4];        // Boot version
	u8   eGON_vsn[4];        // eGON version
	u8   platform[8];        // platform information
} boot_file_head_t;

typedef struct brom_file_head
{
	u32  jump_instruction;   // one intruction jumping to real code
	u8   magic[8];           // ="eGON.BRM",  not C-style string.
	u32  length;             // generated by PC
	u8   Boot_vsn[4];        // Boot version
	u8   eGON_vsn[4];        // eGON version
	u8   platform[8];        // platform information
} brom_file_head_t;

typedef struct _boot_dram_para_t {
  //normal configuration
  unsigned int        dram_clk;
  unsigned int        dram_type;		//dram_type			DDR2: 2				DDR3: 3				LPDDR2: 6	DDR3L: 31
  unsigned int        dram_zq;
  unsigned int		    dram_odt_en;

  //control configuration
  unsigned int		dram_para[2];

  //timing configuration
  unsigned int		dram_mr[4];
  unsigned int		dram_tpr[14];
} boot_dram_para_t;

typedef struct _normal_gpio_cfg {
    __u8 port;
    __u8 port_num;
    __u8 mul_sel;
    __u8 pull;
    __u8 drv_level;
    __u8 data;
    __u8 reserved[2];
} normal_gpio_cfg;

typedef struct _boot0_private_head_t {
    __u32 prvt_head_size;
    char prvt_head_vsn[4];
    boot_dram_para_t dram_para;
    __s32 uart_port;
    normal_gpio_cfg uart_ctrl[2];
    __s32 enable_jtag;
    normal_gpio_cfg jtag_gpio[5];
    normal_gpio_cfg storage_gpio[32];
    __u8 storage_data[256];
} boot0_private_head_t;

typedef struct _boot0_file_head_t {
    boot_file_head_t boot_head;
    boot0_private_head_t prvt_head;
} boot0_file_head_t;

typedef struct _boot_core_para_t {
    __u32 user_set_clock;
    __u32 user_set_core_vol;
    __u32 vol_threshold;
} boot_core_para_t;

typedef struct _boot1_private_head_t {
    __u32 prvt_head_size;
    __u8 prvt_head_vsn[4];
    __s32 uart_port;
    normal_gpio_cfg uart_ctrl[2];
    boot_dram_para_t dram_para;
    char script_buf[32768];
    boot_core_para_t core_para;
    __s32 twi_port;
    normal_gpio_cfg twi_ctrl[2];
    __s32 debug_enable;
    __s32 hold_key_min;
    __s32 hold_key_max;
    __u32 work_mode;
    __u32 storage_type;
    normal_gpio_cfg storage_gpio[32];
    __u8 storage_data[256];
} boot1_private_head_t;

typedef struct _boot1_file_head_t {
    boot_file_head_t boot_head;
    boot1_private_head_t prvt_head;
} boot1_file_head_t;

/* STORAGE DATA on SD loaders */
typedef struct _boot_sdcard_info_t {
    __s32 card_ctrl_num;
    __s32 boot_offset;
    __s32 card_no[4];
    __s32 speed_mode[4];
    __s32 line_sel[4];
    __s32 line_count[4];
} boot_sdcard_info_t;

#define BROM_MAGIC                     "eGON.BRM"
#define BOOT0_MAGIC                     "eGON.BT0"
#define BOOT1_MAGIC                     "eGON.BT1"

union {
	boot_file_head_t boot;
	boot0_file_head_t boot0;
	boot1_file_head_t boot1;
	brom_file_head_t brom;
} boot_hdr;

typedef enum {
	ALLWINNER_UNKNOWN_LOADER=0,
	ALLWINNER_SD_LOADER,
	ALLWINNER_NAND_LOADER
} loader_type;

void fail(char *msg) {
	perror(msg);
	exit(1);
}

void pprintf(void *addr, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("%8x:\t", (unsigned)((char *)addr - (char *)&boot_hdr));
	vprintf(fmt, ap);
	va_end(ap);
}

void print_brom_file_head(brom_file_head_t *hdr)
{
	pprintf(&hdr->magic,		"Magic     : %.8s\n", hdr->magic);
	pprintf(&hdr->length,		"Length    : %u\n", hdr->length);
	pprintf(&hdr->Boot_vsn,		"BOOT ver  : %.4s\n", hdr->Boot_vsn);
	pprintf(&hdr->eGON_vsn,		"eGON ver  : %.4s\n", hdr->eGON_vsn);
	pprintf(&hdr->platform,		"Chip?     : %.8s\n", hdr->platform);
}

void print_boot_file_head(boot_file_head_t *hdr)
{
	pprintf(&hdr->magic,		"Magic     : %.8s\n", hdr->magic);
	pprintf(&hdr->length,		"Length    : %u\n", hdr->length);
	pprintf(&hdr->pub_head_size,	"HSize     : %u\n", hdr->pub_head_size);
	pprintf(&hdr->pub_head_vsn,	"HEAD ver  : %.4s\n", hdr->pub_head_vsn);
	pprintf(&hdr->file_head_vsn,	"FILE ver  : %.4s\n", hdr->file_head_vsn);
	pprintf(&hdr->Boot_vsn,		"BOOT ver  : %.4s\n", hdr->Boot_vsn);
	pprintf(&hdr->eGON_vsn,		"eGON ver  : %.4s\n", hdr->eGON_vsn);
	pprintf(&hdr->platform,		"platform  : %c%c%c%c%c%c%c%c\n", hdr->platform[0], hdr->platform[1], hdr->platform[2], hdr->platform[3], hdr->platform[4], hdr->platform[5], hdr->platform[6], hdr->platform[7]);
}

void print_boot_dram_para(boot_dram_para_t *dram)
{
  int i;
	pprintf(&dram->dram_clk,	"DRAM clk  : %d\n", dram->dram_clk);
	pprintf(&dram->dram_type,	"DRAM type : %d\n", dram->dram_type);
	pprintf(&dram->dram_zq,		"DRAM zq   : %d\n", dram->dram_zq);
	pprintf(&dram->dram_odt_en,	"DRAM odt  : 0x%x\n", dram->dram_odt_en);
  for(i = 1; i <= 2; ++i) {
    pprintf(&dram->dram_para[i-1],	"DRAM para%d : %08x\n", i, dram->dram_para[i-1]);
  }
  for(i = 0; i < 4; ++i) {
    pprintf(&dram->dram_mr[i],	"DRAM mr%d : %08x\n", i, dram->dram_mr[i]);
  }
  for(i = 0; i < 14; ++i) {
    pprintf(&dram->dram_tpr[i],	"DRAM tpr%d : %08x\n", i, dram->dram_tpr[i]);
  }
}

void print_normal_gpio_cfg(normal_gpio_cfg *gpio, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		if (gpio[i].port)
			pprintf(&gpio[i], " GPIO %d   : port=%c%d, sel=%d, pull=%d, drv=%d, data=%d, reserved=%02x,%02x\n", i, 'A'+gpio[i].port-1, gpio[i].port_num, gpio[i].mul_sel, gpio[i].pull, gpio[i].drv_level, gpio[i].data, gpio[i].reserved[0], gpio[i].reserved[1]);
	}
}

void print_boot_sdcard_info(boot_sdcard_info_t *info)
{
	pprintf(&info->card_ctrl_num,	" CARD Ctrl Num: %d\n", info->card_ctrl_num);
	pprintf(&info->boot_offset,	" BOOT Offset: %08x\n", info->boot_offset);

	for (int i = 0; i < 4; i++) {
		if (info->card_no[i] == -1)
			continue;
		pprintf(&info->card_no[i],    " CARD No  : %d (%d)\n", info->card_no[i], i);
		pprintf(&info->speed_mode[i], "  Speed   : %d\n", info->speed_mode[i]);
		pprintf(&info->line_sel[i],   "  Line sel: %d\n", info->line_sel[i]);
		pprintf(&info->line_count[i], "  Line cnt: %d\n", info->line_count[i]);
	}
}

void print_boot0_private_head(boot0_private_head_t *hdr, loader_type type)
{
	pprintf(&hdr->prvt_head_size,	"FHSize    : %u\n", hdr->prvt_head_size);
	pprintf(&hdr->prvt_head_vsn,	"FILE ver  : %.4s\n", hdr->prvt_head_vsn);
	print_boot_dram_para(&hdr->dram_para);
	pprintf(&hdr->uart_port,	"UART port : %d\n", hdr->uart_port);
	print_normal_gpio_cfg(hdr->uart_ctrl, 2);
	pprintf(&hdr->enable_jtag,	"JTAG en   : %d\n", hdr->enable_jtag);
	print_normal_gpio_cfg(hdr->jtag_gpio, 5);
	pprintf(&hdr->storage_gpio,	"STORAGE   :\n");
	print_normal_gpio_cfg(hdr->storage_gpio, 32);
	int i = 0;
	if (type == ALLWINNER_SD_LOADER) {
		print_boot_sdcard_info((boot_sdcard_info_t *)hdr->storage_data);
		i = sizeof(boot_sdcard_info_t);
	}
	for (int n = 0; i < 256; i++, n++) {
		if (n % 16 == 0) {
			if (n) {
				printf("\n");
			}
			pprintf(&hdr->storage_data[i], " DATA %02x  :", i);
		}
		printf(" %02x", hdr->storage_data[i]);
	}
	printf("\n");
}

void print_script(void *UNUSED(script))
{
}

void print_core_para(boot_core_para_t *core)
{
	pprintf(&core->user_set_clock,	"Set Clock : %d\n", core->user_set_clock);
	pprintf(&core->user_set_core_vol, "Set Core Vol: %d\n", core->user_set_core_vol);
	pprintf(&core->vol_threshold,	"Vol Threshold: %d\n", core->vol_threshold);
}

void print_boot1_private_head(boot1_private_head_t *hdr, loader_type type)
{
	pprintf(&hdr->prvt_head_size,	"FHSize    : %u\n", hdr->prvt_head_size);
	pprintf(&hdr->prvt_head_vsn,	"FILE ver  : %.4s\n", hdr->prvt_head_vsn);
	pprintf(&hdr->uart_port,	"UART port : %d\n", hdr->uart_port);
	print_normal_gpio_cfg(hdr->uart_ctrl, 2);
	print_boot_dram_para(&hdr->dram_para);
	print_script(&hdr->script_buf);
	print_core_para(&hdr->core_para);
	pprintf(&hdr->twi_port,		"TWI port  : %d\n", hdr->twi_port);
	print_normal_gpio_cfg(hdr->twi_ctrl, 2);
	pprintf(&hdr->debug_enable,	"Debug     : %d\n", hdr->debug_enable);
	pprintf(&hdr->hold_key_min,	"Hold key min : %d\n", hdr->hold_key_min);
	pprintf(&hdr->hold_key_max,	"Hold key max : %d\n", hdr->hold_key_max);
	pprintf(&hdr->work_mode,	"Work mode : %d\n", hdr->work_mode);
	pprintf(&hdr->storage_type,	"STORAGE   :\n");
	pprintf(&hdr->storage_type,	" type   : %d\n", hdr->storage_type);
	print_normal_gpio_cfg(hdr->storage_gpio, 32);
	int i = 0;
	if (type == ALLWINNER_SD_LOADER) {
		print_boot_sdcard_info((boot_sdcard_info_t *)hdr->storage_data);
		i = sizeof(boot_sdcard_info_t);
	}
	for (int n = 0; i < 256; i++, n++) {
		if (n % 16 == 0) {
			if (n) {
				printf("\n");
			}
			pprintf(&hdr->storage_data[i], " DATA %02x  :", i);
		}
		printf(" %02x", hdr->storage_data[i]);
	}
	printf("\n");
}

void print_boot0_file_head(boot0_file_head_t *hdr, loader_type type)
{
	print_boot_file_head(&hdr->boot_head);
	if (strncmp((char *)hdr->boot_head.file_head_vsn, "1230", 4) == 0)
		print_boot0_private_head(&hdr->prvt_head, type);
	else
		printf("Unknown boot0 header version\n");
}

void print_boot1_file_head(boot1_file_head_t *hdr, loader_type type)
{
	print_boot_file_head(&hdr->boot_head);
	if (strncmp((char *)hdr->boot_head.file_head_vsn, "1230", 4) == 0)
		print_boot1_private_head(&hdr->prvt_head, type);
	else
		printf("Unknown boot0 header version\n");
}

static void usage(const char *cmd)
{
	puts("sunxi-bootinfo " VERSION "\n");
	printf("Usage: %s [<filename>]\n", cmd);
	printf("       With no <filename> given, will read from stdin instead\n");
}

int main(int argc, char * argv[])
{
	FILE *in = stdin;
	loader_type type = ALLWINNER_UNKNOWN_LOADER;
	if (argc > 1 && strcmp(argv[1], "--type=sd") == 0) {
		type = ALLWINNER_SD_LOADER;
		argc--;
		argv++;
	}
	if (argc > 1 && strcmp(argv[1], "--type=nand") == 0) {
		type = ALLWINNER_NAND_LOADER;
		argc--;
		argv++;
	}
	if (argc > 1) {
		in = fopen(argv[1], "rb");
		if (!in) {
			if (*argv[1] == '-')
				usage(argv[0]);
			fail("open input");
		}
	}
	int len;

	len = fread(&boot_hdr, 1, sizeof(boot_hdr), in);
	if (len < (int)sizeof(boot_file_head_t))
		fail("Failed to read header:");
	if (strncmp((char *)boot_hdr.boot.magic, BOOT0_MAGIC, strlen(BOOT0_MAGIC)) == 0) {
		print_boot0_file_head(&boot_hdr.boot0, type);
	} else if (strncmp((char *)boot_hdr.boot.magic, BOOT1_MAGIC, strlen(BOOT1_MAGIC)) == 0) {
		print_boot1_file_head(&boot_hdr.boot1, type);
	} else if (strncmp((char *)boot_hdr.boot.magic, BROM_MAGIC, strlen(BROM_MAGIC)) == 0) {
		print_brom_file_head(&boot_hdr.brom);
	} else {
		fail("Invalid magic\n");
	}

	return 0;
}
