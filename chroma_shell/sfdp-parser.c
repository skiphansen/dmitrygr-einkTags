// SPDX-License-Identifier: GPL-2.0-only
// Original Author: Petr Malat
// Originally from https://github.com/petris/sfdp-parser.git
// 
// Macronix vendor table support added  by Skip Hansen 
// SFDP is defined in JEDEC spec SJESD216H

#include <byteswap.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <err.h>

#define NAME_LEN 47

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define SFDP_SIGNATURE 0x50444653

struct sfdp_param_hdr {
	uint8_t id_lsb;
	uint8_t minor;
	uint8_t major;
	uint8_t len;
	uint8_t ptr[3];
	uint8_t id_msb;
};

struct sfdp_hdr {
	uint32_t signature;
	uint8_t minor;
	uint8_t	major;
	uint8_t	nph;
	uint8_t proto;
};

static void p_num_val(uint32_t val, const char *name, const char *description,
		bool hex)
{
	const char *descriptionfmt = " (%s)";
	int indent, numlen = 10;
	char fmt[32];

	if (!description) {
		descriptionfmt = "%s";
		description = "";
	}

	indent = NAME_LEN - strlen(name);
	if (indent < 1) {
		snprintf(fmt, sizeof fmt, hex ? "%#x" : "%u", val);
		numlen += strlen(fmt) + indent - 2;
		if (numlen < 0)
			numlen = 0;
		indent = 1;
	}

	snprintf(fmt, sizeof fmt, "%%s:%%-%ds%%%s%d%c%s\n", indent, hex ? "#" : "", numlen, hex ? 'x' : 'u', descriptionfmt);
	printf(fmt, name, "", val, description);
}

static void p_dec_val(uint32_t val, const char *name, const char *description)
{
	p_num_val(val, name, description, false);
}

static void p_hex_val(uint32_t val, const char *name, const char *description)
{
	p_num_val(val, name, description, val >= 10);
}

/*
 * Parameter handlers
 */
//#define bits(w, high, low) ((w & ((1ULL << ((high) + 1)) - 1)) >> (low))
#define bits(w, high, low) ((w & (0xffFFffFFU >> (31 - high))) >> (low))

#define parse_bits(w, high, low, name, vals...) do { \
	const char *arr[] = vals; \
	int b = bits(w, high, low); \
        if (b < ARRAY_SIZE(arr) && arr[b]) \
		p_hex_val(b, name, arr[b]); \
        else \
		p_hex_val(b, name, "Invalid value"); \
        } while (0);

#define SUPP_OR_NOT { "not supported", "supported" }

static int sfdp_unknown_parameter(uint32_t *w, size_t len)
{
	unsigned i;

	for (i = 0; i < len; i++) {
		char name[32];

		snprintf(name, sizeof name, "Word %u", i);
		p_hex_val(w[i], name, NULL);
	}

	return 0;
}

static int sfdp_basic(uint32_t *w, size_t len)
{
	unsigned erase_time_mult[] = {1, 16, 128, 1000};
	unsigned program_page_time_mult[] = {8, 64};
	unsigned program_byte_time_mult[] = {1, 8};
	unsigned erase_chip_time_mult[] = {16, 256, 4000, 64000};
	unsigned tmp;

	if (len < 1)
		return 0;

	if (bits(w[1], 31, 31)) {
		p_dec_val(1 << (bits(w[1], 30, 0)-23), "Flash Memory Density", "in megabytes");
	} else {
		p_dec_val(bits(w[1], 30, 0)/8, "Flash Memory Density", "in bytes");
   }

	parse_bits(w[0], 1, 0, "Erase Size", {
		[1] = "4kB supported",
		[3] = "4kB supported",
	});
	parse_bits(w[0], 2, 2, "Write Granularity", {
		"Single byte or less than 64 bytes",
		"64 bytes or more"
	});
	parse_bits(w[0], 3, 3, "Volatile Status Register Block Protect Bits", {
		"non-volatile", "volatile"
	});
	parse_bits(w[0], 4, 4, "Write Enable Instruction for Writing to Volatile Status Register", {
		"50h", "06h"
	});
	p_hex_val(bits(w[0], 15, 8), "4kB Erase Instruction", NULL);
	parse_bits(w[0], 18, 17, "Address Bytes", {
		"3-byte addressing", "3- or 4-byte addressing", "4-byte addressing"
	});
	parse_bits(w[0], 19, 19, "Double transfer rate (DTR) Clocking", SUPP_OR_NOT);
	parse_bits(w[0], 16, 16, "1-1-2 Fast Read", SUPP_OR_NOT);
   if(len >= 4 && (w[0] & (1 << 16))) {
   	p_dec_val(bits(w[3], 4, 0), "1-1-2 Fast Read Number of Wait States Needed", NULL);
   	p_dec_val(bits(w[3], 7, 5), "1-1-2 Fast Read Number of Mode Clocks", NULL);
   	p_hex_val(bits(w[3], 15, 8), "1-1-2 Fast Read Instructions", NULL);
   }

	parse_bits(w[0], 20, 20, "1-2-2 Fast Read", SUPP_OR_NOT);
   if(len >= 4 && (w[0] & (1 << 20))) {
      p_dec_val(bits(w[3], 20, 16), "1-2-2 Fast Read Number of Wait States Needed", NULL);
      p_dec_val(bits(w[3], 23, 21), "1-2-2 Fast Read Number of Mode Clocks", NULL);
      p_hex_val(bits(w[3], 31, 24), "1-2-2 Fast Read Instructions", NULL);
   }

	parse_bits(w[0], 21, 21, "1-4-4 Fast Read", SUPP_OR_NOT);
   if(len >= 3 && (w[0] & (1 << 21))) {
      p_dec_val(bits(w[2], 4, 0), "1-4-4 Fast Read Number of Wait States Needed", NULL);
      p_dec_val(bits(w[2], 7, 5), "1-4-4 Fast Read Number of Mode Clocks", NULL);
      p_hex_val(bits(w[2], 15, 8), "1-4-4 Fast Read Instructions", NULL);
   }

	parse_bits(w[0], 22, 22, "1-1-4 Fast Read", SUPP_OR_NOT);

   if(len >= 3 && (w[0] & (1 << 22))) {
      p_dec_val(bits(w[2], 20, 16), "1-1-4 Fast Read Number of Wait States Needed", NULL);
      p_dec_val(bits(w[2], 23, 21), "1-1-4 Fast Read Number of Mode Clocks", NULL);
      p_hex_val(bits(w[2], 31, 24), "1-1-4 Fast Read Instructions", NULL);
   }

	if (len < 2)
		return 0;


	if (len < 5)
		return 0;

	parse_bits(w[4], 0, 0, "2-2-2 Fast Read", SUPP_OR_NOT);

	if (len >= 6 && (w[4] & (1 << 0))) {
      p_dec_val(bits(w[5], 20, 16), "2-2-2 Fast Read Number of Wait States Needed", NULL);
      p_dec_val(bits(w[5], 23, 21), "2-2-2 Fast Read Number of Mode Clocks", NULL);
      p_hex_val(bits(w[5], 31, 24), "2-2-2 Fast Read Instructions", NULL);
   }

	parse_bits(w[4], 4, 4, "4-4-4 Fast Read", SUPP_OR_NOT);
	if (len >= 7 && (w[4] & (1 << 4))) {
      p_dec_val(bits(w[6], 20, 16), "4-4-4 Fast Read Number of Wait States Needed", NULL);
      p_dec_val(bits(w[6], 23, 21), "4-4-4 Fast Read Number of Mode Clocks", NULL);
      p_hex_val(bits(w[6], 31, 24), "4-4-4 Fast Read Instructions", NULL);
   }

	if (len < 8)
		return 0;
	p_dec_val(1 << bits(w[7], 7, 0), "Erase Type 1 Size", "in bytes");
	p_hex_val(bits(w[7], 15, 8), "Erase Type 1 Instruction", NULL);
	p_dec_val(1 << bits(w[7], 23, 16), "Erase Type 2 Size", bits(w[7], 23, 16) ? "in bytes" : "not supported");
	p_hex_val(bits(w[7], 31, 24), "Erase Type 2 Instruction", NULL);


	if (len < 9)
		return 0;
	p_dec_val(1 << bits(w[8], 7, 0), "Erase Type 3 Size", bits(w[8], 7, 0) ? "in bytes" : "not supported");
   if(bits(w[8], 7, 0)) {
      p_hex_val(bits(w[8], 15, 8), "Erase Type 3 Instruction", NULL);
   }
	p_dec_val(1 << bits(w[8], 23, 16), "Erase Type 4 Size", bits(w[8], 23, 16) ? "in bytes" : "not supported");
   if(bits(w[8], 23, 16)) {
      p_hex_val(bits(w[8], 31, 24), "Erase Type 4 Instruction", NULL);
   }

	if (len < 10)
		return 0;
	p_dec_val(2 * (1 + bits(w[9], 3, 0)), "Typical Erase Time to Maximum Erase Time Multiplier", NULL);
	tmp = erase_time_mult[bits(w[9], 8, 4)] * (bits(w[9], 10, 9) + 1);
	p_dec_val(tmp, "Erase Type 1 Typical Time", "in milliseconds");
	tmp = erase_time_mult[bits(w[9], 17, 16)] * (bits(w[9], 15, 11) + 1);
	p_dec_val(tmp, "Erase Type 2 Typical Time", "in milliseconds");
	tmp = erase_time_mult[bits(w[9], 24, 23)] * (bits(w[9], 22, 18) + 1);
	p_dec_val(tmp, "Erase Type 4 Typical Time", "in milliseconds");
	tmp = erase_time_mult[bits(w[9], 31, 30)] * (bits(w[9], 29, 25) + 1);
	p_dec_val(tmp, "Erase Type 4 Typical Time", "in milliseconds");


	if (len < 11)
		return 0;
	p_dec_val(2 * (1 + bits(w[10], 3, 0)), "Typical Program Time to Maximum Program Time Multiplier", NULL);
	p_dec_val(1 << bits(w[10], 7, 4), "Page Size", NULL);
	tmp = program_page_time_mult[bits(w[10], 13, 13)] * (bits(w[10], 12, 8) + 1);
	p_dec_val(tmp, "Typical Page Program Time", "in microseconds");
	tmp = program_byte_time_mult[bits(w[10], 18, 18)] * (bits(w[10], 17, 14) + 1);
	p_dec_val(tmp, "Typical First Byte Program Time", "in microseconds");
	tmp = program_byte_time_mult[bits(w[10], 23, 23)] * (bits(w[10], 22, 19) + 1);
	p_dec_val(tmp, "Typical Additional Byte Program Time", "in microseconds");
	tmp = erase_chip_time_mult[bits(w[10], 30, 29)] * (bits(w[10], 28, 24) + 1);
	p_dec_val(tmp, "Typical Chip Erase Time", "in milliseconds");
	

	return 0;
}

static uint32_t sfdp_basic_get_size(uint32_t *w)
{
   uint32_t Ret;

   if (bits(w[1], 31, 31)) {
   // Flash Memory in megabytes
      Ret = (1 << (bits(w[1], 30, 0)-23)) * 1024 * 1024;
   } else {
   // Flash Memory in bytes
      Ret = bits(w[1], 30, 0)/8;
   }
	return Ret;
}


unsigned int Bcd2Bin(unsigned int Hex)
{
   unsigned int Ret = 0;

   for(int i = 0; i < 4; i++) {
      Ret *= 10;
      Ret += (Hex & 0xf000) >> 12;
      Hex <<= 4;
   }

   return Ret;
}

static int sfdp_macronix(uint32_t *w, size_t len)
{
	if (len < 1)
		return 0;

// voltage is BCD, convert to decimal
	p_dec_val(Bcd2Bin((w[0] >> 16) & 0xffff), "Minimum Vcc Voltage", "in millivolts");
	p_dec_val(Bcd2Bin(w[0] & 0xffff), "Maximum Vcc Voltage", "in millivolts");

	parse_bits(w[1], 0, 0, "H/W Reset# pin", SUPP_OR_NOT);
	parse_bits(w[1], 1, 1, "H/W Hold# pin", SUPP_OR_NOT);
	parse_bits(w[1], 2, 2, "Deep Power down mode",SUPP_OR_NOT);
	parse_bits(w[1], 3, 3, "S/W Reset", SUPP_OR_NOT);
   if(bits(w[1],3,3)) {
      p_hex_val(bits(w[1], 11, 4), "S/W Reset opcode", NULL);
   }
   parse_bits(w[1], 12, 12, "Program Suspend/Resume", SUPP_OR_NOT);
   parse_bits(w[1], 13, 13, "Erase Suspend/Resume", SUPP_OR_NOT);
   parse_bits(w[1], 15, 15, "Wrap-Around Read mode", SUPP_OR_NOT);
   if(bits(w[1],15,15)) {
      p_hex_val(bits(w[1], 23, 16), "Wrap-Around Read mode opcode", NULL);
   }
   parse_bits(w[2], 0, 0, "Individual block lock", SUPP_OR_NOT);

   if(w[2] & (1 << 0)) {
      parse_bits(w[2], 1, 1, "Individual block lock bit", {
         "Volatile",
         "Nonvolatile"
      });
      parse_bits(w[2], 10, 10, "Individual block lock default", {
         "protect",
         "unprotect"
      });
   }
   parse_bits(w[2], 11, 11, "Secure OTP", SUPP_OR_NOT);
   parse_bits(w[2], 12, 12, "Read lock", SUPP_OR_NOT);
   parse_bits(w[2], 13, 13, "Permantent lock", SUPP_OR_NOT);

	return 0;
}


struct sfdp_param_handler {
	uint16_t id;
	const char *name;
	int (*dumper)(uint32_t *ptr, size_t len);
};

static const struct sfdp_param_handler sfdp_unknown_param = {
	0, "Unknown parameter", sfdp_unknown_parameter
};

static const struct sfdp_param_handler handlers[] = {
	{ 0xff00, "Basic flash parameter table", sfdp_basic },
	{ 0xffc2, "Macronix flash parameter table", sfdp_macronix }
};

// return eeprom size in bytes
uint32_t sfdp_dump(uint32_t *buf,int sz,bool bSilent)
{
	struct {
		struct sfdp_hdr hdr;
		struct sfdp_param_hdr param[];
	} *sfdp = (void*)buf;
	int i, j;
   uint32_t Ret = 0;

	if (sfdp->hdr.signature == bswap_32(SFDP_SIGNATURE)) {
		for (i = 0; i < sz/4; i++)
			buf[i] = bswap_32(buf[i]);
	}

	if (sfdp->hdr.signature != SFDP_SIGNATURE) {
      if(!bSilent) {
         printf("Invalid signature %#08x, expected %#08x\n",
               sfdp->hdr.signature, SFDP_SIGNATURE);
      }
      return 0;
	}

   if(!bSilent) {
   	p_hex_val(sfdp->hdr.signature, "Signature",NULL);
   	p_dec_val(sfdp->hdr.major, "Major", NULL);
   	p_dec_val(sfdp->hdr.minor, "Minor", NULL);
   	p_dec_val(sfdp->hdr.nph + 1, "Parameters", NULL);
   }

	for (i = 0; i <= sfdp->hdr.nph; i++) {
		const struct sfdp_param_handler *h = &sfdp_unknown_param;
		struct sfdp_param_hdr *p = &sfdp->param[i];
		unsigned id = p->id_msb << 8 | p->id_lsb;
		unsigned off = p->ptr[2] << 16 | p->ptr[1] << 8 | p->ptr[0];

		for (j = 0; j < ARRAY_SIZE(handlers); j++) {
         if(id == 0xff00 && bSilent) {
         // just wanted flash memory size
            return sfdp_basic_get_size(&buf[off/4]) + 1;
         }
			if (id == handlers[i].id) {
				h = &handlers[i];
				break;
			}
		}

		printf("\nParameter %d (%s)\n", i, h->name);
		p_hex_val(id, "ID", NULL);
		p_dec_val(p->major, "Major", NULL);
		p_dec_val(p->minor, "Minor", NULL);
		p_dec_val(off, "Offset", off % 4 ? "Invalid alignment" : NULL);
		p_dec_val(p->len * 4, "Length", NULL);

		if (off % 4) {
			printf("Warning: Unaligned offset of parameter %u\n", id);
			continue;
		}

		if (off + sfdp->param[i].len * 4 <= sz)
			h->dumper(&buf[off/4], sfdp->param[i].len);
		else
			printf("Warning: Parameter %u data are behind the end of the file\n", id);
	}

	return Ret;
}
