#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// #include "crcspeed/crc64speed.h"
// #include "zlib.h"
#include "lzf/lzf.h"

#include "config.h"
#include "dma.h"
#include "disc.h"
#include "disc_86f.h"
#include "disc_random.h"
#include "fdc.h"
#include "fdd.h"
#include "ibm.h"

#define D86FVER		0x020B

#define CHUNK 16384

uint64_t poly = 0x42F0E1EBA9EA3693;		/* ECMA normal */

uint64_t table[256];

/* Let's give this some more logic:
	Bits 4,3 = Read/write (0 = read, 1 = write, 2 = scan, 3 = verify)
	Bits 6,5 = Sector/track (0 = ID, 1 = sector, 2 = deleted sector, 3 = track)
	Bit  7   = State type (0 = idle states, 1 = active states)
*/
enum
{
	/* 0 ?? ?? ??? */
        STATE_IDLE = 0x00,
	STATE_SECTOR_NOT_FOUND,

	/* 1 00 00 ??? */
	STATE_0A_FIND_ID = 0x80,			/* READ SECTOR ID */
	STATE_0A_READ_ID,

	/* 1 01 00 ??? */
	STATE_06_FIND_ID = 0xA0,			/* READ DATA */
	STATE_06_READ_ID,
	STATE_06_FIND_DATA,
	STATE_06_READ_DATA,

	/* 1 01 01 ??? */
	STATE_05_FIND_ID = 0xA8,			/* WRITE DATA */
	STATE_05_READ_ID,
	STATE_05_FIND_DATA,
	STATE_05_WRITE_DATA,

	/* 1 01 10 ??? */
	STATE_11_FIND_ID = 0xB0,			/* SCAN EQUAL, SCAN LOW OR EQUAL, SCAN HIGH OR EQUAL */
	STATE_11_READ_ID,
	STATE_11_FIND_DATA,
	STATE_11_SCAN_DATA,

	/* 1 01 11 ??? */
	STATE_16_FIND_ID = 0xB8,			/* VERIFY */
	STATE_16_READ_ID,
	STATE_16_FIND_DATA,
	STATE_16_VERIFY_DATA,

	/* 1 10 00 ??? */
	STATE_0C_FIND_ID = 0xC0,			/* READ DELETED DATA */
	STATE_0C_READ_ID,
	STATE_0C_FIND_DATA,
	STATE_0C_READ_DATA,

	/* 1 10 01 ??? */
	STATE_09_FIND_ID = 0xC8,			/* WRITE DELETED DATA */
	STATE_09_READ_ID,
	STATE_09_FIND_DATA,
	STATE_09_WRITE_DATA,

	/* 1 11 00 ??? */
	STATE_02_SPIN_TO_INDEX = 0xE0,			/* READ TRACK */
	STATE_02_FIND_ID,
	STATE_02_READ_ID,
	STATE_02_FIND_DATA,
	STATE_02_READ_DATA,

	/* 1 11 01 ??? */
	STATE_0D_SPIN_TO_INDEX = 0xE8,			/* FORMAT TRACK */
	STATE_0D_FORMAT_TRACK
};

static uint16_t CRCTable[256];

typedef struct __attribute__((packed))
{
	uint8_t buffer[10];
	uint32_t pos;
	uint32_t len;
} sliding_buffer_t;

typedef struct __attribute__((packed))
{
	uint32_t sync_marks;
	uint32_t bits_obtained;
	uint32_t bytes_obtained;
	uint32_t sync_pos;
} find_t;

uint8_t encoded_fm[64] = {	0xAA, 0xAB, 0xAE, 0xAF, 0xBA, 0xBB, 0xBE, 0xBF, 0xEA, 0xEB, 0xEE, 0xEF, 0xFA, 0xFB, 0xFE, 0xFF,
				0xAA, 0xAB, 0xAE, 0xAF, 0xBA, 0xBB, 0xBE, 0xBF, 0xEA, 0xEB, 0xEE, 0xEF, 0xFA, 0xFB, 0xFE, 0xFF,
				0xAA, 0xAB, 0xAE, 0xAF, 0xBA, 0xBB, 0xBE, 0xBF, 0xEA, 0xEB, 0xEE, 0xEF, 0xFA, 0xFB, 0xFE, 0xFF,
				0xAA, 0xAB, 0xAE, 0xAF, 0xBA, 0xBB, 0xBE, 0xBF, 0xEA, 0xEB, 0xEE, 0xEF, 0xFA, 0xFB, 0xFE, 0xFF };

uint8_t encoded_mfm[64] = {	0xAA, 0xA9, 0xA4, 0xA5, 0x92, 0x91, 0x94, 0x95, 0x4A, 0x49, 0x44, 0x45, 0x52, 0x51, 0x54, 0x55,
				0x2A, 0x29, 0x24, 0x25, 0x12, 0x11, 0x14, 0x15, 0x4A, 0x49, 0x44, 0x45, 0x52, 0x51, 0x54, 0x55,
				0xAA, 0xA9, 0xA4, 0xA5, 0x92, 0x91, 0x94, 0x95, 0x4A, 0x49, 0x44, 0x45, 0x52, 0x51, 0x54, 0x55,
				0x2A, 0x29, 0x24, 0x25, 0x12, 0x11, 0x14, 0x15, 0x4A, 0x49, 0x44, 0x45, 0x52, 0x51, 0x54, 0x55 };

enum
{
	FMT_PRETRK_GAP0,
	FMT_PRETRK_SYNC,
	FMT_PRETRK_IAM,
	FMT_PRETRK_GAP1,

	FMT_SECTOR_ID_SYNC,
	FMT_SECTOR_IDAM,
	FMT_SECTOR_ID,
	FMT_SECTOR_ID_CRC,
	FMT_SECTOR_GAP2,
	FMT_SECTOR_DATA_SYNC,
	FMT_SECTOR_DATAAM,
	FMT_SECTOR_DATA,
	FMT_SECTOR_DATA_CRC,
	FMT_SECTOR_GAP3,

	FMT_POSTTRK_CHECK,
	FMT_POSTTRK_GAP4,
};

typedef struct __attribute__((packed))
{
	unsigned nibble0	:4;
	unsigned nibble1	:4;
} split_byte_t;

typedef union {
	uint8_t byte;
	split_byte_t nibbles;
} decoded_t;

/* Disk flags: Bit 0		Has surface data (1 = yes, 0 = no)
	       Bits 2, 1	Hole (3 = ED + 2000 kbps, 2 = ED, 1 = HD, 0 = DD)
	       Bit 3		Sides (1 = 2 sides, 0 = 1 side)
	       Bit 4		Write protect (1 = yes, 0 = no)
	       Bits 6, 5	RPM slowdown (3 = 2%, 2 = 1.5%, 1 = 1%, 0 = 0%)
	       Bit 7		Bitcell mode (1 = Extra bitcells count specified after disk flags, 0 = No extra bitcells)
				The maximum number of extra bitcells is 1024 (which after decoding translates to 64 bytes)
	       Bit 8		Disk type (1 = Zoned, 0 = Fixed RPM)
	       Bits 10, 9	Zone type (3 = Commodore 64 zoned, 2 = Apple zoned, 1 = Pre-Apple zoned #2, 0 = Pre-Apple zoned #1)
	       Bit 11		Data and surface bits are stored in reverse byte endianness */

static struct __attribute__((packed))
{
        FILE *f;
	uint16_t version;
	uint16_t disk_flags;
	int32_t extra_bit_cells[2];
        uint16_t track_encoded_data[2][53048];
        uint16_t track_surface_data[2][53048];
        uint16_t thin_track_encoded_data[2][2][53048];
        uint16_t thin_track_surface_data[2][2][53048];
        uint16_t side_flags[2];
        uint32_t index_hole_pos[2];
        uint32_t track_offset[512];
	uint32_t file_size;
	sector_id_t format_sector_id;
	sector_id_t last_sector;
	sector_id_t req_sector;
	uint32_t index_count;
	uint8_t state;
	uint8_t fill;
	uint32_t track_pos;
	uint32_t datac;
	uint32_t id_pos;
	uint16_t last_word[2];
	find_t id_find;
	find_t data_find;
	crc_t calc_crc;
	crc_t track_crc;
	uint8_t sector_count;
	uint8_t format_state;
	uint16_t satisfying_bytes;
	uint16_t preceding_bit[2];
	uint16_t current_byte[2];
	uint16_t current_bit[2];
	int cur_track;
	uint32_t error_condition;
	int is_compressed;
	int id_found;
	uint8_t original_file_name[2048];
	uint8_t *filebuf;
	uint8_t *outbuf;
	uint32_t dma_over;
} d86f[FDD_NUM];

int d86f_do_log = 0;

void d86f_log(const char *format, ...)
{
#ifdef ENABLE_D86F_LOG
   if (d86f_do_log)
   {
		va_list ap;
		va_start(ap, format);
		vprintf(format, ap);
		va_end(ap);
		fflush(stdout);
   }
#endif
}

static void d86f_setupcrc(uint16_t poly)
{
	int c = 256, bc;
	uint16_t crctemp;

	while(c--)
	{
		crctemp = c << 8;
		bc = 8;

		while(bc--)
		{
			if(crctemp & 0x8000)
			{
				crctemp = (crctemp << 1) ^ poly;
			}
			else
			{
				crctemp <<= 1;
			}
		}

		CRCTable[c] = crctemp;
	}
}

static int d86f_has_surface_desc(int drive)
{
	return (d86f_handler[drive].disk_flags(drive) & 1);
}

int d86f_get_sides(int drive)
{
	return ((d86f_handler[drive].disk_flags(drive) >> 3) & 1) + 1;
}

int d86f_get_rpm_mode(int drive)
{
	return (d86f_handler[drive].disk_flags(drive) & 0x60) >> 5;
}

int d86f_reverse_bytes(int drive)
{
	return (d86f_handler[drive].disk_flags(drive) & 0x800) >> 11;
}

uint16_t d86f_side_flags(int drive);
int d86f_is_mfm(int drive);
void d86f_writeback(int drive);
uint8_t d86f_poll_read_data(int drive, int side, uint16_t pos);
void d86f_poll_write_data(int drive, int side, uint16_t pos, uint8_t data);
int d86f_format_conditions(int drive);

uint16_t d86f_disk_flags(int drive)
{
	return d86f[drive].disk_flags;
}


uint32_t d86f_index_hole_pos(int drive, int side)
{
	return d86f[drive].index_hole_pos[side];
}

uint32_t null_index_hole_pos(int drive, int side)
{
	return 0;
}

uint16_t null_disk_flags(int drive)
{
	return 0x09;
}

uint16_t null_side_flags(int drive)
{
	return 0x0A;
}

void null_writeback(int drive)
{
	return;
}

void null_set_sector(int drive, int side, uint8_t c, uint8_t h, uint8_t r, uint8_t n)
{
	return;
}

void null_write_data(int drive, int side, uint16_t pos, uint8_t data)
{
	return;
}

int null_format_conditions(int drive)
{
	return 0;
}

int32_t d86f_extra_bit_cells(int drive, int side)
{
	return d86f[drive].extra_bit_cells[side];
}

int32_t null_extra_bit_cells(int drive, int side)
{
	return 0;
}

uint16_t* common_encoded_data(int drive, int side)
{
	return d86f[drive].track_encoded_data[side];
}

void common_read_revolution(int drive)
{
	return;
}

uint16_t d86f_side_flags(int drive)
{
	int side = 0;
	side = fdd_get_head(drive);
	return d86f[drive].side_flags[side];
}

uint16_t d86f_track_flags(int drive)
{
	uint16_t tf = 0;
	uint16_t rr = 0;
	uint16_t dr = 0;

	tf = d86f_handler[drive].side_flags(drive);
	rr = tf & 0x67;
	dr = fdd_get_flags(drive) & 7;
	tf &= ~0x67;

	switch (rr)
	{
		case 0x02:
		case 0x21:
			/* 1 MB unformatted medium, treat these two as equivalent. */
			switch (dr)
			{
				case 0x06:
					/* 5.25" Single-RPM HD drive, treat as 300 kbps, 360 rpm. */
					tf |= 0x21;
					break;
				default:
					/* Any other drive, treat as 250 kbps, 300 rpm. */
					tf |= 0x02;
					break;
			}
			break;
		default:
			tf |= rr;
			break;
	}
	return tf;
}

uint32_t common_get_raw_size(int drive, int side)
{
	double rate = 0.0;
	int mfm = 0;
	double rpm;
	double rpm_diff = 0.0;
	double size = 100000.0;

	mfm = d86f_is_mfm(drive);
	rpm = ((d86f_track_flags(drive) & 0xE0) == 0x20) ? 360.0 : 300.0;
	rpm_diff = 1.0;

	switch (d86f_get_rpm_mode(drive))
	{
		case 1:
			rpm_diff = 1.01;
			break;
		case 2:
			rpm_diff = 1.015;
			break;
		case 3:
			rpm_diff = 1.02;
			break;
		default:
			rpm_diff = 1.0;
			break;
	}
	switch (d86f_track_flags(drive) & 7)
	{
		case 0:
			rate = 500.0;
			break;
		case 1:
			rate = 300.0;
			break;
		case 2:
			rate = 250.0;
			break;
		case 3:
			rate = 1000.0;
			break;
		case 5:
			rate = 2000.0;
			break;
		default:
			rate = 250.0;
			break;
	}
	if (!mfm)  rate /= 2.0;
	size = (size / 250.0) * rate;
	size = (size * 300.0) / rpm;
	size *= rpm_diff;
	/* Round down to a multiple of 16 and add the extra bit cells, then return. */
	return ((((uint32_t) size) >> 4) << 4) + d86f_handler[drive].extra_bit_cells(drive, side);
}

void d86f_unregister(int drive)
{
	d86f_handler[drive].disk_flags = null_disk_flags;
	d86f_handler[drive].side_flags = null_side_flags;
	d86f_handler[drive].writeback = null_writeback;
	d86f_handler[drive].set_sector = null_set_sector;
	d86f_handler[drive].write_data = null_write_data;
	d86f_handler[drive].format_conditions = null_format_conditions;
	d86f_handler[drive].extra_bit_cells = null_extra_bit_cells;
	d86f_handler[drive].encoded_data = common_encoded_data;
	d86f_handler[drive].read_revolution = common_read_revolution;
	d86f_handler[drive].index_hole_pos = null_index_hole_pos;
	d86f_handler[drive].get_raw_size = common_get_raw_size;
	d86f_handler[drive].check_crc = 0;
	d86f[drive].version = 0x0063;						/* Proxied formats report as version 0.99. */
}

void d86f_register_86f(int drive)
{
	d86f_handler[drive].disk_flags = d86f_disk_flags;
	d86f_handler[drive].side_flags = d86f_side_flags;
	d86f_handler[drive].writeback = d86f_writeback;
	d86f_handler[drive].set_sector = null_set_sector;
	d86f_handler[drive].write_data = null_write_data;
	d86f_handler[drive].format_conditions = d86f_format_conditions;
	d86f_handler[drive].extra_bit_cells = d86f_extra_bit_cells;
	d86f_handler[drive].encoded_data = common_encoded_data;
	d86f_handler[drive].read_revolution = common_read_revolution;
	d86f_handler[drive].index_hole_pos = d86f_index_hole_pos;
	d86f_handler[drive].get_raw_size = common_get_raw_size;
	d86f_handler[drive].check_crc = 1;
}

int d86f_get_array_size(int drive, int side)
{
	int array_size = 0;
	int rm = 0;
	int hole = 0;
	int extra_bytes = 0;
	rm = d86f_get_rpm_mode(drive);
	hole = (d86f_handler[drive].disk_flags(drive) & 6) >> 1;
	switch (hole)
	{
		case 0:
		case 1:
		default:
			array_size = 12500;
			switch (rm)
			{
				case 1:
					array_size = 12625;
					break;
				case 2:
					array_size = 12687;
					break;
				case 3:
					array_size = 12750;
					break;
				default:
					array_size = 12500;
					break;
			}
			break;
		case 2:
			array_size = 25000;
			switch (rm)
			{
				case 1:
					array_size = 25250;
					break;
				case 2:
					array_size = 25375;
					break;
				case 3:
					array_size = 25500;
					break;
				default:
					array_size = 25000;
					break;
			}
			break;
		case 3:
			array_size = 50000;
			switch (rm)
			{
				case 1:
					array_size = 50500;
					break;
				case 2:
					array_size = 50750;
					break;
				case 3:
					array_size = 51000;
					break;
				default:
					array_size = 50000;
					break;
			}
			break;
	}
	array_size <<= 4;
	array_size += d86f_handler[drive].extra_bit_cells(drive, side);
	array_size >>= 4;
	if (d86f_handler[drive].extra_bit_cells(drive, side) & 15)
	{
		array_size++;
	}
	return array_size;
}

int d86f_valid_bit_rate(int drive)
{
	int rate = 0;
	int hole = 0;
	rate = fdc_get_bit_rate();
	hole = (d86f_handler[drive].disk_flags(drive) & 6) >> 1;
	switch (hole)
	{
		case 0:	/* DD */
			if (!rate && (fdd_get_flags(drive) & 0x10))  return 1;
			if ((rate < 1) || (rate > 2))  return 0;
			return 1;
		case 1:	/* HD */
			if (rate != 0)  return 0;
			return 1;
		case 2:	/* ED */
			if (rate != 3)  return 0;
			return 1;
		case 3:	/* ED with 2000 kbps support */
			if (rate < 3)  return 0;
			return 1;
	}
	return 1;
}

int d86f_hole(int drive)
{
	if (((d86f_handler[drive].disk_flags(drive) >> 1) & 3) == 3)  return 2;
	return (d86f_handler[drive].disk_flags(drive) >> 1) & 3;
}

uint8_t d86f_get_encoding(int drive)
{
	return (d86f_track_flags(drive) & 0x18) >> 3;
}

double d86f_byteperiod(int drive)
{
	switch (d86f_track_flags(drive) & 0x0f)
	{
		case 0x02:	/* 125 kbps, FM */
			return 4.0;
		case 0x01:	/* 150 kbps, FM */
			return 20.0 / 6.0;
		case 0x0A:	/* 250 kbps, MFM */
		case 0x00:	/* 250 kbps, FM */
			return 2.0;
		case 0x09:	/* 300 kbps, MFM */
			return 10.0 / 6.0;
		case 0x08:	/* 500 kbps, MFM */
			return 1.0;
		case 0x0B:	/* 1000 kbps, MFM */
			return 0.5;
		case 0x0D:	/* 2000 kbps, MFM */
			return 0.25;
		default:
			return 2.0;
	}
	return 2.0;
}

int d86f_is_mfm(int drive)
{
	return (d86f_track_flags(drive) & 8) ? 1 : 0;
}

uint32_t d86f_get_data_len(int drive)
{
	if (d86f[drive].req_sector.id.n)
	{
		if (d86f[drive].req_sector.id.n == 8)  return 32768;
		return (128 << ((uint32_t) d86f[drive].req_sector.id.n));
	}
	else
	{
		if (fdc_get_dtl() < 128)
		{
			return fdc_get_dtl();
		}
		else
		{
			return (128 << ((uint32_t) d86f[drive].req_sector.id.n));
		}
	}
}

uint32_t d86f_has_extra_bit_cells(int drive)
{
	return (d86f_disk_flags(drive) >> 7) & 1;
}

uint32_t d86f_header_size(int drive)
{
	return 8;
}

static uint16_t d86f_encode_get_data(uint8_t dat)
{
        uint16_t temp;
        temp = 0;
        if (dat & 0x01) temp |= 1;
        if (dat & 0x02) temp |= 4;
        if (dat & 0x04) temp |= 16;
        if (dat & 0x08) temp |= 64;
        if (dat & 0x10) temp |= 256;
        if (dat & 0x20) temp |= 1024;
        if (dat & 0x40) temp |= 4096;
        if (dat & 0x80) temp |= 16384;
        return temp;
}

static uint16_t d86f_encode_get_clock(uint8_t dat)
{
        uint16_t temp;
        temp = 0;
        if (dat & 0x01) temp |= 2;
        if (dat & 0x02) temp |= 8;
        if (dat & 0x40) temp |= 32;
        if (dat & 0x08) temp |= 128;
        if (dat & 0x10) temp |= 512;
        if (dat & 0x20) temp |= 2048;
        if (dat & 0x40) temp |= 8192;
        if (dat & 0x80) temp |= 32768;
        return temp;
}

int d86f_format_conditions(int drive)
{
	return d86f_valid_bit_rate(drive);
}

int d86f_wrong_densel(int drive)
{
	int is_3mode = 0;

	if ((fdd_get_flags(drive) & 7) == 3)
	{
		is_3mode = 1;
	}

	switch (d86f_hole(drive))
	{
		case 0:
		default:
			if (fdd_get_densel(drive))
			{
				return 1;
			}
			else
			{
				return 0;
			}
			break;
		case 1:
			if (fdd_get_densel(drive))
			{
				return 0;
			}
			else
			{
				if (is_3mode)
				{
					return 0;
				}
				else
				{
					return 1;
				}
			}
			break;
		case 2:
			if (fdd_get_densel(drive))
			{
				return 0;
			}
			else
			{
				return 1;
			}
			break;
	}
}

int d86f_can_format(int drive)
{
	int temp;
	temp = !writeprot[drive];
	temp = temp && !swwp;
	temp = temp && fdd_can_read_medium(real_drive(drive));
	temp = temp && d86f_handler[drive].format_conditions(drive);		/* Allows proxied formats to add their own extra conditions to formatting. */
	temp = temp && !d86f_wrong_densel(drive);
	return temp;
}

uint16_t d86f_encode_byte(int drive, int sync, decoded_t b, decoded_t prev_b)
{
	uint8_t encoding = d86f_get_encoding(drive);
	uint8_t bits89AB = prev_b.nibbles.nibble0;
	uint8_t bits7654 = b.nibbles.nibble1;
	uint8_t bits3210 = b.nibbles.nibble0;
	uint16_t encoded_7654, encoded_3210, result;
	if (encoding > 1)  return 0xFF;
	if (sync)
	{
		result = d86f_encode_get_data(b.byte);
		if (encoding)
		{
			switch(b.byte)
			{
				case 0xA1: return result | d86f_encode_get_clock(0x0A);
				case 0xC2: return result | d86f_encode_get_clock(0x14);
				case 0xF8: return result | d86f_encode_get_clock(0x03);
				case 0xFB: case 0xFE: return result | d86f_encode_get_clock(0x00);
				case 0xFC: return result | d86f_encode_get_clock(0x01);
			}
		}
		else
		{
			switch(b.byte)
			{
				case 0xF8: case 0xFB: case 0xFE: return result | d86f_encode_get_clock(0xC7);
				case 0xFC: return result | d86f_encode_get_clock(0xD7);
			}
		}
	}
	bits3210 += ((bits7654 & 3) << 4);
	bits7654 += ((bits89AB & 3) << 4);
	encoded_3210 = (encoding == 1) ? encoded_mfm[bits3210] : encoded_fm[bits3210];
	encoded_7654 = (encoding == 1) ? encoded_mfm[bits7654] : encoded_fm[bits7654];
	result = (encoded_7654 << 8) | encoded_3210;
	return result;
}

static int d86f_get_bitcell_period(int drive)
{
	double rate = 0.0;
	int mfm = 0;
	int tflags = 0;
	double rpm = 0;
	double size = 8000.0;

	tflags = d86f_track_flags(drive);

	mfm = (tflags & 8) ? 1 : 0;
	rpm = ((tflags & 0xE0) == 0x20) ? 360.0 : 300.0;

	switch (tflags & 7)
	{
		case 0:
			rate = 500.0;
			break;
		case 1:
			rate = 300.0;
			break;
		case 2:
			rate = 250.0;
			break;
		case 3:
			rate = 1000.0;
			break;
		case 5:
			rate = 2000.0;
			break;
	}
	if (!mfm)  rate /= 2.0;
	size = (size * 250.0) / rate;
	size = (size * 300.0) / rpm;
	size = (size * fdd_getrpm(real_drive(drive))) / 300.0;
	return (int) size;
}

int d86f_can_read_address(int drive)
{
	int temp = 0;
	temp = (fdc_get_bitcell_period() == d86f_get_bitcell_period(drive));
	temp = temp && fdd_can_read_medium(real_drive(drive));
	temp = temp && (fdc_is_mfm() == d86f_is_mfm(drive));
	temp = temp && (d86f_get_encoding(drive) <= 1);
	return temp;
}

void d86f_get_bit(int drive, int side)
{
	uint32_t track_word;
	uint32_t track_bit;
	uint16_t encoded_data;
	uint16_t surface_data;
	uint16_t current_bit;
	uint16_t surface_bit;

	track_word = d86f[drive].track_pos >> 4;
	/* We need to make sure we read the bits from MSB to LSB. */
	track_bit = 15 - (d86f[drive].track_pos & 15);

	if (d86f_reverse_bytes(drive))
	{
		/* Image is in reverse endianness, read the data as is. */
		encoded_data = d86f_handler[drive].encoded_data(drive, side)[track_word];
	}
	else
	{
		/* We store the words as big endian, so we need to convert them to little endian when reading. */
		encoded_data = (d86f_handler[drive].encoded_data(drive, side)[track_word] & 0xFF) << 8;
		encoded_data |= (d86f_handler[drive].encoded_data(drive, side)[track_word] >> 8);
	}

	if (d86f_has_surface_desc(drive))
	{
		if (d86f_reverse_bytes(drive))
		{
			surface_data = d86f[drive].track_surface_data[side][track_word] & 0xFF;
		}
		else
		{
			surface_data = (d86f[drive].track_surface_data[side][track_word] & 0xFF) << 8;
			surface_data |= (d86f[drive].track_surface_data[side][track_word] >> 8);
		}
	}

	current_bit = (encoded_data >> track_bit) & 1;
	d86f[drive].last_word[side] <<= 1;

	if (d86f_has_surface_desc(drive))
	{
		surface_bit = (surface_data >> track_bit) & 1;
		if (!surface_bit)
		{
			if (!current_bit)
			{
				/* Bit is 0 and is not set to fuzzy, we add it as read. */
				d86f[drive].last_word[side] |= 1;
			}
			else
			{
				/* Bit is 1 and is not set to fuzzy, we add it as read. */
				d86f[drive].last_word[side] |= 1;
			}
		}
		else
		{
			if (current_bit)
			{
				/* Bit is 1 and is set to fuzzy, we randomly generate it. */
				d86f[drive].last_word[side] |= (disc_random_generate() & 1);
			}
		}
	}
	else
	{
		d86f[drive].last_word[side] |= current_bit;
	}
}

void d86f_put_bit(int drive, int side, int bit)
{
	uint32_t track_word;
	uint32_t track_bit;
	uint16_t encoded_data;
	uint16_t surface_data;
	uint16_t current_bit;
	uint16_t surface_bit;

	track_word = d86f[drive].track_pos >> 4;
	/* We need to make sure we read the bits from MSB to LSB. */
	track_bit = 15 - (d86f[drive].track_pos & 15);

	if (d86f_reverse_bytes(drive))
	{
		/* Image is in reverse endianness, read the data as is. */
		encoded_data = d86f_handler[drive].encoded_data(drive, side)[track_word];
	}
	else
	{
		/* We store the words as big endian, so we need to convert them to little endian when reading. */
		encoded_data = (d86f_handler[drive].encoded_data(drive, side)[track_word] & 0xFF) << 8;
		encoded_data |= (d86f_handler[drive].encoded_data(drive, side)[track_word] >> 8);
	}

	if (d86f_has_surface_desc(drive))
	{
		if (d86f_reverse_bytes(drive))
		{
			surface_data = d86f[drive].track_surface_data[side][track_word] & 0xFF;
		}
		else
		{
			surface_data = (d86f[drive].track_surface_data[side][track_word] & 0xFF) << 8;
			surface_data |= (d86f[drive].track_surface_data[side][track_word] >> 8);
		}
	}

	current_bit = (encoded_data >> track_bit) & 1;
	d86f[drive].last_word[side] <<= 1;

	if (d86f_has_surface_desc(drive))
	{
		surface_bit = (surface_data >> track_bit) & 1;
		if (!surface_bit)
		{
			if (!current_bit)
			{
				/* Bit is 0 and is not set to fuzzy, we overwrite it as is. */
				d86f[drive].last_word[side] |= bit;
				current_bit = bit;
			}
			else
			{
				/* Bit is 1 and is not set to fuzzy, we overwrite it as is. */
				d86f[drive].last_word[side] |= bit;
				current_bit = bit;
			}
		}
		else
		{
			if (current_bit)
			{
				/* Bit is 1 and is set to fuzzy, we overwrite it with a non-fuzzy bit. */
				d86f[drive].last_word[side] |= bit;
				current_bit = bit;
				surface_bit = 0;
			}
		}

		surface_data &= ~(1 << track_bit);
		surface_data |= (surface_bit << track_bit);
		if (d86f_reverse_bytes(drive))
		{
			d86f[drive].track_surface_data[side][track_word] = surface_data;
		}
		else
		{
			d86f[drive].track_surface_data[side][track_word] = (surface_data & 0xFF) << 8;
			d86f[drive].track_surface_data[side][track_word] |= (surface_data >> 8);
		}
	}
	else
	{
		d86f[drive].last_word[side] |= bit;
		current_bit = bit;
	}

	encoded_data &= ~(1 << track_bit);
	encoded_data |= (current_bit << track_bit);

	if (d86f_reverse_bytes(drive))
	{
		d86f_handler[drive].encoded_data(drive, side)[track_word] = encoded_data;
	}
	else
	{
		d86f_handler[drive].encoded_data(drive, side)[track_word] = (encoded_data & 0xFF) << 8;
		d86f_handler[drive].encoded_data(drive, side)[track_word] |= (encoded_data >> 8);
	}
}

static uint8_t decodefm(int drive, uint16_t dat)
{
        uint8_t temp = 0;
	/* We write the encoded bytes in big endian, so we process the two 8-bit halves swapped here. */
        if (dat & 0x0001) temp |= 1;
        if (dat & 0x0004) temp |= 2;
        if (dat & 0x0010) temp |= 4;
        if (dat & 0x0040) temp |= 8;
        if (dat & 0x0100) temp |= 16;
        if (dat & 0x0400) temp |= 32;
        if (dat & 0x1000) temp |= 64;
        if (dat & 0x4000) temp |= 128;
        return temp;
}

void disc_calccrc(uint8_t byte, crc_t *crc_var)
{
	crc_var->word = (crc_var->word << 8) ^ CRCTable[(crc_var->word >> 8)^byte];
}

static void d86f_calccrc(int drive, uint8_t byte)
{
	disc_calccrc(byte, &(d86f[drive].calc_crc));
}

int d86f_word_is_aligned(int drive, int side, uint32_t base_pos)
{
	int adjusted_track_pos = d86f[drive].track_pos;

	if (base_pos == 0xFFFFFFFF)
	{
		return 0;
	}

	/* This is very important, it makes sure alignment is detected correctly even across the index hole of a track whose length is not divisible by 16. */
	if (adjusted_track_pos < base_pos)
	{
		adjusted_track_pos += d86f_handler[drive].get_raw_size(drive, side);
	}

	if ((adjusted_track_pos & 15) == (base_pos & 15))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/* State 1: Find sector ID */
void d86f_find_address_mark_fm(int drive, int side, find_t *find, uint16_t req_am, uint16_t other_am, uint16_t ignore_other_am)
{
	d86f_get_bit(drive, side);

	if (d86f[drive].last_word[side] == req_am)
	{
		d86f[drive].calc_crc.word = 0xFFFF;
		disc_calccrc(decodefm(drive, d86f[drive].last_word[side]), &(d86f[drive].calc_crc));
		find->sync_marks = find->bits_obtained = find->bytes_obtained = 0;
		find->sync_pos = 0xFFFFFFFF;
		d86f[drive].preceding_bit[side] = d86f[drive].last_word[side] & 1;
		d86f[drive].state++;
		return;
	}

	if ((ignore_other_am & 2) && (d86f[drive].last_word[side] == other_am))
	{
		d86f[drive].calc_crc.word = 0xFFFF;
		disc_calccrc(decodefm(drive, d86f[drive].last_word[side]), &(d86f[drive].calc_crc));
		find->sync_marks = find->bits_obtained = find->bytes_obtained = 0;
		find->sync_pos = 0xFFFFFFFF;
		if (ignore_other_am & 1)
		{
			/* Skip mode, let's go back to finding ID. */
			d86f[drive].state -= 2;
		}
		else
		{
			/* Not skip mode, process the sector anyway. */
			fdc_set_wrong_am();
			d86f[drive].preceding_bit[side] = d86f[drive].last_word[side] & 1;
			d86f[drive].state++;
		}
		return;
	}
}

/* When writing in FM mode, we find the beginning of the address mark by looking for 352 (22 * 16) set bits (gap fill = 0xFF, 0xFFFF FM-encoded). */
void d86f_write_find_address_mark_fm(int drive, int side, find_t *find)
{
	d86f_get_bit(drive, side);

	if (d86f[drive].last_word[side] & 1)
	{
		find->sync_marks++;
		if (find->sync_marks == 352)
		{
			d86f[drive].calc_crc.word = 0xFFFF;
			d86f[drive].preceding_bit[side] = 1;
			find->sync_marks = 0;
			d86f[drive].state++;
			return;
		}
	}

	/* If we hadn't found enough set bits but have found a clear bit, null the counter of set bits. */
	if (!(d86f[drive].last_word[side] & 1))
	{
		find->sync_marks = find->bits_obtained = find->bytes_obtained = 0;
		find->sync_pos = 0xFFFFFFFF;
	}
}

void d86f_find_address_mark_mfm(int drive, int side, find_t *find, uint16_t req_am, uint16_t other_am, uint16_t ignore_other_am)
{
	d86f_get_bit(drive, side);

	if (d86f[drive].last_word[side] == 0x4489)
	{
		find->sync_marks++;
		find->sync_pos = d86f[drive].track_pos;
		// d86f_log("Sync marks: %i\n", find->sync_marks);
		
		return;
	}

	if ((d86f[drive].last_word[side] == req_am) && (find->sync_marks >= 3))
	{
		if (d86f_word_is_aligned(drive, side, find->sync_pos))
		{
			d86f[drive].calc_crc.word = 0xCDB4;
			disc_calccrc(decodefm(drive, d86f[drive].last_word[side]), &(d86f[drive].calc_crc));
			find->sync_marks = find->bits_obtained = find->bytes_obtained = 0;
			find->sync_pos = 0xFFFFFFFF;
			// d86f_log("AM found (%04X) (%02X)\n", req_am, d86f[drive].state);
			d86f[drive].preceding_bit[side] = d86f[drive].last_word[side] & 1;
			d86f[drive].state++;
			return;
		}
	}

	if ((ignore_other_am & 2) && (d86f[drive].last_word[side] == other_am) && (find->sync_marks >= 3))
	{
		if (d86f_word_is_aligned(drive, side, find->sync_pos))
		{
			d86f[drive].calc_crc.word = 0xCDB4;
			disc_calccrc(decodefm(drive, d86f[drive].last_word[side]), &(d86f[drive].calc_crc));
			find->sync_marks = find->bits_obtained = find->bytes_obtained = 0;
			find->sync_pos = 0xFFFFFFFF;
			if (ignore_other_am & 1)
			{
				/* Skip mode, let's go back to finding ID. */
				d86f[drive].state -= 2;
			}
			else
			{
				/* Not skip mode, process the sector anyway. */
				// d86f_log("Wrong AM found (%04X) (%02X)\n", other_am, d86f[drive].state);
				fdc_set_wrong_am();
				d86f[drive].preceding_bit[side] = d86f[drive].last_word[side] & 1;
				d86f[drive].state++;
			}
			return;
		}
	}

	if (d86f[drive].last_word[side] != 0x4489)
	{
		if (d86f_word_is_aligned(drive, side, find->sync_pos))
		{
			find->sync_marks = find->bits_obtained = find->bytes_obtained = 0;
			find->sync_pos = 0xFFFFFFFF;
		}
	}
}

/* When writing in MFM mode, we find the beginning of the address mark by looking for 3 0xA1 sync bytes. */
void d86f_write_find_address_mark_mfm(int drive, int side, find_t *find)
{
	d86f_get_bit(drive, side);

	if (d86f[drive].last_word[side] == 0x4489)
	{
		find->sync_marks++;
		find->sync_pos = d86f[drive].track_pos;
		if (find->sync_marks == 3)
		{
			d86f[drive].calc_crc.word = 0xCDB4;
			d86f[drive].preceding_bit[side] = 1;
			find->sync_marks = 0;
			d86f[drive].state++;
			return;
		}
	}

	/* If we hadn't found enough address mark sync marks, null the counter. */
	if (d86f[drive].last_word[side] != 0x4489)
	{
		if (d86f_word_is_aligned(drive, side, find->sync_pos))
		{
			find->sync_marks = find->bits_obtained = find->bytes_obtained = 0;
			find->sync_pos = 0xFFFFFFFF;
		}
	}
}

/* State 2: Read sector ID and CRC*/
void d86f_read_sector_id(int drive, int side, int match)
{
	uint16_t temp;

	if (d86f[drive].id_find.bits_obtained)
	{
		if (!(d86f[drive].id_find.bits_obtained & 15))
		{
			/* We've got a byte. */
			if (d86f[drive].id_find.bytes_obtained < 4)
			{
				d86f[drive].last_sector.byte_array[d86f[drive].id_find.bytes_obtained] = decodefm(drive, d86f[drive].last_word[side]);
				disc_calccrc(d86f[drive].last_sector.byte_array[d86f[drive].id_find.bytes_obtained], &(d86f[drive].calc_crc));
			}
			else if ((d86f[drive].id_find.bytes_obtained >= 4) && (d86f[drive].id_find.bytes_obtained < 6))
			{
				d86f[drive].track_crc.bytes[(d86f[drive].id_find.bytes_obtained & 1) ^ 1] = decodefm(drive, d86f[drive].last_word[side]);
			}
			d86f[drive].id_find.bytes_obtained++;

			if (d86f[drive].id_find.bytes_obtained == 6)
			{
				/* We've got the ID. */
				if (d86f[drive].calc_crc.word != d86f[drive].track_crc.word)
				{
					d86f[drive].id_find.sync_marks = d86f[drive].id_find.bits_obtained = d86f[drive].id_find.bytes_obtained = 0;
					printf("ID CRC error: %04X != %04X (%08X)\n", d86f[drive].track_crc.word, d86f[drive].calc_crc.word, d86f[drive].last_sector.dword);
					if ((d86f[drive].state != STATE_02_READ_ID) && (d86f[drive].state != STATE_0A_READ_ID))
					{
						d86f[drive].error_condition = 0;
						d86f[drive].state = STATE_IDLE;
						fdc_finishread();
						fdc_headercrcerror();
					}
					else if (d86f[drive].state == STATE_0A_READ_ID)
					{
						d86f[drive].state--;
					}
					else
					{
						d86f[drive].error_condition |= 1;	/* Mark that there was an ID CRC error. */
						d86f[drive].state++;
					}
				}
				else if ((d86f[drive].calc_crc.word == d86f[drive].track_crc.word) && (d86f[drive].state == STATE_0A_READ_ID))
				{
					/* CRC is valid and this is a read sector ID command. */
					d86f[drive].id_find.sync_marks = d86f[drive].id_find.bits_obtained = d86f[drive].id_find.bytes_obtained = d86f[drive].error_condition = 0;
					fdc_sectorid(d86f[drive].last_sector.id.c, d86f[drive].last_sector.id.h, d86f[drive].last_sector.id.r, d86f[drive].last_sector.id.n, 0, 0);
					d86f[drive].state = STATE_IDLE;
				}
				else
				{
					/* CRC is valid. */
					// d86f_log("Sector ID found: %08X; Requested: %08X\n", d86f[drive].last_sector.dword, d86f[drive].req_sector.dword);
					d86f[drive].id_find.sync_marks = d86f[drive].id_find.bits_obtained = d86f[drive].id_find.bytes_obtained = 0;
					d86f[drive].id_found++;
					if ((d86f[drive].last_sector.dword == d86f[drive].req_sector.dword) || !match)
					{
						// d86f_log("ID read (%02X)\n", d86f[drive].state);
						d86f_handler[drive].set_sector(drive, side, d86f[drive].last_sector.id.c, d86f[drive].last_sector.id.h, d86f[drive].last_sector.id.r, d86f[drive].last_sector.id.n);
						if (d86f[drive].state == STATE_02_READ_ID)
						{
							/* READ TRACK command, we need some special handling here. */
							/* Code corrected: Only the C, H, and N portions of the sector ID are compared, the R portion (the sector number) is ignored. */
							if ((d86f[drive].last_sector.id.c != fdc_get_read_track_sector().id.c) || (d86f[drive].last_sector.id.h != fdc_get_read_track_sector().id.h) || (d86f[drive].last_sector.id.n != fdc_get_read_track_sector().id.n))
							{
								d86f[drive].error_condition |= 4;	/* Mark that the sector ID is not the one expected by the FDC. */
								/* Make sure we use the sector size from the FDC. */
								d86f[drive].last_sector.id.n = fdc_get_read_track_sector().id.n;
							}
							/* If the two ID's are identical, then we do not need to do anything regarding the sector size. */
						}
						d86f[drive].state++;
					}
					else
					{
						if (d86f[drive].last_sector.id.c != d86f[drive].req_sector.id.c)
						{
							if (d86f[drive].last_sector.id.c == 0xFF)
							{
								// d86f_log("[State: %02X] [Side %i] Bad cylinder (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
								d86f[drive].error_condition |= 8;
							}
							else
							{
								// d86f_log("[State: %02X] [Side %i] Wrong cylinder (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
								d86f[drive].error_condition |= 0x10;
							}
						}

						d86f[drive].state--;
					}
				}
			}
		}
	}

	d86f_get_bit(drive, side);

	d86f[drive].id_find.bits_obtained++;
}

uint8_t d86f_get_data(int drive, int base)
{
	int data;

	if (d86f[drive].data_find.bytes_obtained < (d86f_get_data_len(drive) + base))
	{
		data = fdc_getdata(d86f[drive].data_find.bytes_obtained == (d86f_get_data_len(drive) + base - 1));
		if ((data & DMA_OVER) || (data == -1))
		{
			d86f[drive].dma_over++;
			if (data == -1)
			{
				data = 0;
			}
			else
			{
				data &= 0xff;
			}
		}
	}
	else
	{
		data = 0;
	}

	return data;
}

void d86f_compare_byte(int drive, uint8_t received_byte, uint8_t disk_byte)
{
	switch(fdc_get_compare_condition())
	{
		case 0:		/* SCAN EQUAL */
			if ((received_byte == disk_byte) || (received_byte == 0xFF))
			{
				d86f[drive].satisfying_bytes++;
			}
			break;
		case 1:		/* SCAN LOW OR EQUAL */
			if ((received_byte <= disk_byte) || (received_byte == 0xFF))
			{
			d86f[drive].satisfying_bytes++;
			}
			break;
		case 2:		/* SCAN HIGH OR EQUAL */
			if ((received_byte >= disk_byte) || (received_byte == 0xFF))
			{
				d86f[drive].satisfying_bytes++;
			}
			break;
	}
}

/* State 4: Read sector data and CRC*/
void d86f_read_sector_data(int drive, int side)
{
        int data = 0;
	int recv_data = 0;
	int read_status = 0;
	uint16_t temp;
	uint32_t sector_len = d86f[drive].last_sector.id.n;
	uint32_t crc_pos = 0;
	sector_len = 1 << (7 + sector_len);
	crc_pos = sector_len + 2;

	if (d86f[drive].data_find.bits_obtained)
	{
		if (!(d86f[drive].data_find.bits_obtained & 15))
		{
			/* We've got a byte. */
			if (d86f[drive].data_find.bytes_obtained < sector_len)
			{
				data = decodefm(drive, d86f[drive].last_word[side]);
				if (d86f[drive].state == STATE_11_SCAN_DATA)
				{
					/* Scan/compare command. */
					recv_data = d86f_get_data(drive, 0);
					d86f_compare_byte(drive, recv_data, data);
				}
				else
				{
					if (d86f[drive].data_find.bytes_obtained < d86f_get_data_len(drive))
					{
						if (d86f[drive].state != STATE_16_VERIFY_DATA)
						{
							read_status = fdc_data(data);
							if (read_status == -1)
							{
								d86f[drive].dma_over++;
								// d86f_log("DMA over now: %i\n", d86f[drive].dma_over);
							}
						}
					}
				}
				disc_calccrc(data, &(d86f[drive].calc_crc));
			}
			else if (d86f[drive].data_find.bytes_obtained < crc_pos)
			{
				d86f[drive].track_crc.bytes[(d86f[drive].data_find.bytes_obtained - sector_len) ^ 1] = decodefm(drive, d86f[drive].last_word[side]);
			}
			d86f[drive].data_find.bytes_obtained++;

			if (d86f[drive].data_find.bytes_obtained == (crc_pos + fdc_get_gap()))
			{
				/* We've got the data. */
				if (d86f[drive].dma_over > 1)
				{
					// d86f_log("DMA overrun while reading data!\n");
					d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
					d86f[drive].error_condition = 0;
					d86f[drive].state = STATE_IDLE;
					fdc_finishread();
					fdc_overrun();

					d86f_get_bit(drive, side);

					d86f[drive].data_find.bits_obtained++;
					return;
				}
				else
				{
					// d86f_log("Bytes over DMA: %i\n", d86f[drive].dma_over);
				}

				if ((d86f[drive].calc_crc.word != d86f[drive].track_crc.word) && (d86f[drive].state != STATE_02_READ_DATA))
				{
					printf("Data CRC error: %04X != %04X (%08X)\n", d86f[drive].track_crc.word, d86f[drive].calc_crc.word, d86f[drive].last_sector.dword);
					d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
					d86f[drive].error_condition = 0;
					d86f[drive].state = STATE_IDLE;
					fdc_finishread();
					fdc_datacrcerror();
				}
				else if ((d86f[drive].calc_crc.word != d86f[drive].track_crc.word) && (d86f[drive].state == STATE_02_READ_DATA))
				{
					// printf("%04X != %04X (%08X)\n", d86f[drive].track_crc.word, d86f[drive].calc_crc.word, d86f[drive].last_sector.dword);
					d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
					d86f[drive].error_condition |= 2;	/* Mark that there was a data error. */
					d86f[drive].state = STATE_IDLE;
					fdc_track_finishread(d86f[drive].error_condition);
				}
				else
				{
					/* CRC is valid. */
					d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
					d86f[drive].error_condition = 0;
					if (d86f[drive].state == STATE_11_SCAN_DATA)
					{
						d86f[drive].state = STATE_IDLE;
						fdc_sector_finishcompare((d86f[drive].satisfying_bytes == ((128 << ((uint32_t) d86f[drive].last_sector.id.n)) - 1)) ? 1 : 0);
					}
					else
					{
						d86f[drive].state = STATE_IDLE;
						fdc_sector_finishread();
					}
				}
			}
		}
	}

	d86f_get_bit(drive, side);

	d86f[drive].data_find.bits_obtained++;
}

void d86f_write_sector_data(int drive, int side, int mfm, uint16_t am)
{
	uint16_t bit_pos;
	uint16_t temp;
	uint32_t sector_len = d86f[drive].last_sector.id.n;
	uint32_t crc_pos = 0;
	sector_len = (1 << (7 + sector_len)) + 1;
	crc_pos = sector_len + 2;

	if (!(d86f[drive].data_find.bits_obtained & 15))
	{
		if (d86f[drive].data_find.bytes_obtained < crc_pos)
		{
			if (!d86f[drive].data_find.bytes_obtained)
			{
				/* We're writing the address mark. */
				d86f[drive].current_byte[side] = am;
			}
			else if (d86f[drive].data_find.bytes_obtained < sector_len)
			{
				/* We're in the data field of the sector, read byte from FDC and request new byte. */
				d86f[drive].current_byte[side] = d86f_get_data(drive, 1);
				d86f_handler[drive].write_data(drive, side, d86f[drive].data_find.bytes_obtained - 1, d86f[drive].current_byte[side]);
			}
			else
			{
				/* We're in the data field of the sector, use a CRC byte. */
				d86f[drive].current_byte[side] = d86f[drive].calc_crc.bytes[(d86f[drive].data_find.bytes_obtained & 1)];
				// d86f_log("BO: %04X (%02X)\n", d86f[drive].data_find.bytes_obtained, d86f[drive].current_byte[side]);
			}

			d86f[drive].current_bit[side] = (15 - (d86f[drive].data_find.bits_obtained & 15)) >> 1;

			/* Write the bit. */
			temp = (d86f[drive].current_byte[side] >> d86f[drive].current_bit[side]) & 1;
			if ((!temp && !d86f[drive].preceding_bit[side]) || !mfm)
			{
				temp |= 2;
			}

			/* This is an even bit, so write the clock. */
			if (!d86f[drive].data_find.bytes_obtained)
			{
				/* Address mark, write bit directly. */
				d86f_put_bit(drive, side, am >> 15);
			}
			else
			{
				d86f_put_bit(drive, side, temp >> 1);
			}

			if (d86f[drive].data_find.bytes_obtained < sector_len)
			{
				/* This is a data byte, so CRC it. */
				if (!d86f[drive].data_find.bytes_obtained)
				{
					disc_calccrc(decodefm(drive, am), &(d86f[drive].calc_crc));
				}
				else
				{
					disc_calccrc(d86f[drive].current_byte[side], &(d86f[drive].calc_crc));
				}
			}
		}
	}
	else
	{
		if (d86f[drive].data_find.bytes_obtained < crc_pos)
		{
			/* Encode the bit. */
			bit_pos = 15 - (d86f[drive].data_find.bits_obtained & 15);
			d86f[drive].current_bit[side] = bit_pos >> 1;

			temp = (d86f[drive].current_byte[side] >> d86f[drive].current_bit[side]) & 1;
			if ((!temp && !d86f[drive].preceding_bit[side]) || !mfm)
			{
				temp |= 2;
			}

			if (!d86f[drive].data_find.bytes_obtained)
			{
				/* Address mark, write directly. */
				d86f_put_bit(drive, side, am >> bit_pos);
				if (!(bit_pos & 1))
				{
					d86f[drive].preceding_bit[side] = am >> bit_pos;
				}
			}
			else
			{
				if (bit_pos & 1)
				{
					/* Clock bit */
					d86f_put_bit(drive, side, temp >> 1);
				}
				else
				{
					/* Data bit */
					d86f_put_bit(drive, side, temp & 1);
					d86f[drive].preceding_bit[side] = temp & 1;
				}
			}
		}

		if ((d86f[drive].data_find.bits_obtained & 15) == 15)
		{
			d86f[drive].data_find.bytes_obtained++;

			if (d86f[drive].data_find.bytes_obtained == (crc_pos + fdc_get_gap()))
			{
				if (d86f[drive].dma_over > 1)
				{
					// d86f_log("DMA overrun while writing data!\n");
					d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
					d86f[drive].error_condition = 0;
					d86f[drive].state = STATE_IDLE;
					fdc_finishread();
					fdc_overrun();

					d86f[drive].data_find.bits_obtained++;
					return;
				}

				/* We've written the data. */
				d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
				d86f[drive].error_condition = 0;
				d86f[drive].state = STATE_IDLE;
				d86f_handler[drive].writeback(drive);
				fdc_sector_finishread();
				return;
			}
		}
	}

	d86f[drive].data_find.bits_obtained++;
}

void d86f_advance_bit(int drive, int side)
{
	d86f[drive].track_pos++;
	d86f[drive].track_pos %= d86f_handler[drive].get_raw_size(drive, side);

	if (d86f[drive].track_pos == d86f_handler[drive].index_hole_pos(drive, side))
	{
		d86f_handler[drive].read_revolution(drive);

		if (d86f[drive].state != STATE_IDLE)
		{
			d86f[drive].index_count++;
			// d86f_log("Index count now: %i\n", d86f[drive].index_count);
		}
	}
}

void d86f_advance_word(int drive, int side)
{
	d86f[drive].track_pos += 16;
	d86f[drive].track_pos %= d86f_handler[drive].get_raw_size(drive, side);

	if ((d86f[drive].track_pos == d86f_handler[drive].index_hole_pos(drive, side)) && (d86f[drive].state != STATE_IDLE))  d86f[drive].index_count++;
}

void d86f_spin_to_index(int drive, int side)
{
	d86f_get_bit(drive, side);
	d86f_get_bit(drive, side ^ 1);

	d86f_advance_bit(drive, side);

	if (d86f[drive].track_pos == d86f_handler[drive].index_hole_pos(drive, side))
	{
		if (d86f[drive].state == STATE_0D_SPIN_TO_INDEX)
		{
			/* When starting format, reset format state to the beginning. */
			d86f[drive].preceding_bit[side] = 1;
			d86f[drive].format_state = FMT_PRETRK_GAP0;
		}
		/* This is to make sure both READ TRACK and FORMAT TRACK command don't end prematurely. */
		d86f[drive].index_count = 0;
		d86f[drive].state++;
	}
}

void d86f_write_direct_common(int drive, int side, uint16_t byte, uint8_t type, uint32_t pos)
{
	uint16_t encoded_byte, mask_data, mask_surface, mask_hole, mask_fuzzy;
	decoded_t dbyte, dpbyte;

	dbyte.byte = byte;
	dpbyte.byte = d86f[drive].preceding_bit[side];
	d86f[drive].preceding_bit[side] = encoded_byte & 1;

	if (type == 0)
	{
		/* Byte write. */
		encoded_byte = d86f_encode_byte(drive, 0, dbyte, dpbyte);
		if (!d86f_reverse_bytes(drive))
		{
			mask_data = encoded_byte >> 8;
			encoded_byte &= 0xFF;
			encoded_byte <<= 8;
			encoded_byte |= mask_data;
		}
	}
	else
	{
		/* Word write. */
		encoded_byte = byte;
		if (d86f_reverse_bytes(drive))
		{
			mask_data = encoded_byte >> 8;
			encoded_byte &= 0xFF;
			encoded_byte <<= 8;
			encoded_byte |= mask_data;
		}
	}

	if (d86f_has_surface_desc(drive))
	{
		mask_data = d86f[drive].track_encoded_data[side][pos] ^= 0xFFFF;
		mask_surface = d86f[drive].track_surface_data[side][pos];
		mask_hole = (mask_surface & mask_data) ^ 0xFFFF;	/* This will retain bits that are both fuzzy and 0, therefore physical holes. */
		encoded_byte &= mask_hole;				/* Filter out physical hole bits from the encoded data. */
		mask_data ^= 0xFFFF;					/* Invert back so bits 1 are 1 again. */
		mask_fuzzy = (mask_surface & mask_data) ^ 0xFFFF;	/* All fuzzy bits are 0. */
		d86f[drive].track_surface_data[side][pos] &= mask_fuzzy;	/* Remove fuzzy bits (but not hole bits) from the surface mask, making them regular again. */
	}

	d86f[drive].track_encoded_data[side][pos] = encoded_byte;
	d86f[drive].last_word[side] = encoded_byte;
}

void d86f_write_direct(int drive, int side, uint16_t byte, uint8_t type)
{
	d86f_write_direct_common(drive, side, byte, type, d86f[drive].track_pos >> 4);
}

uint16_t endian_swap(uint16_t word)
{
	uint16_t temp;

	temp = word & 0xff;
	temp <<= 8;
	temp |= (word >> 8);
	return temp;
}

void d86f_format_finish(int drive, int side, int mfm, uint16_t sc, uint16_t gap_fill, int do_write)
{
	if (mfm && do_write)
	{
		if (do_write && (d86f[drive].track_pos == d86f_handler[drive].index_hole_pos(drive, side)))
		{
			d86f_write_direct_common(drive, side, gap_fill, 0, 0);
		}
	}

	d86f[drive].state = STATE_IDLE;
	d86f_handler[drive].writeback(drive);
	// d86f_log("Format finished (%i) (%i)!\n", d86f[drive].track_pos, sc);
	d86f[drive].error_condition = 0;
	d86f[drive].datac = 0;
	fdc_sector_finishread();
}

void d86f_format_track(int drive, int side)
{
        int data;
	uint16_t max_len, temp, temp2;

	int mfm;
	uint16_t i = 0;
	uint16_t j = 0;
	uint16_t sc = 0;
	uint16_t dtl = 0;
	int gap_sizes[4] = { 0, 0, 0, 0 };
	int am_len = 0;
	int sync_len = 0;
	uint16_t iam_mfm[4] = { 0x2452, 0x2452, 0x2452, 0x5255 };
	uint16_t idam_mfm[4] = { 0x8944, 0x8944, 0x8944, 0x5455 };
	uint16_t dataam_mfm[4] = { 0x8944, 0x8944, 0x8944, 0x4555 };
	uint16_t iam_fm = 0xFAF7;
	uint16_t idam_fm = 0x7EF5;
	uint16_t dataam_fm = 0x6FF5;
	uint16_t gap_fill = 0x4E;
	int do_write = 0;

	mfm = d86f_is_mfm(drive);
	am_len = mfm ? 4 : 1;
	gap_sizes[0] = mfm ? 80 : 40;
	gap_sizes[1] = mfm ? 50 : 26;
	gap_sizes[2] = fdc_get_gap2(real_drive(drive));
	gap_sizes[3] = fdc_get_gap();
	sync_len = mfm ? 12 : 6;
	sc = fdc_get_format_sectors();
	dtl = 128 << fdc_get_format_n();
	gap_fill = mfm ? 0x4E : 0xFF;
	do_write = (d86f[drive].version == D86FVER);

	switch(d86f[drive].format_state)
	{
		case FMT_POSTTRK_GAP4:
			max_len = 60000;
			if (do_write)  d86f_write_direct(drive, side, gap_fill, 0);
			break;
		case FMT_PRETRK_GAP0:
			max_len = gap_sizes[0];
			if (do_write)  d86f_write_direct(drive, side, gap_fill, 0);
			break;
		case FMT_SECTOR_ID_SYNC:
			if (d86f[drive].datac <= 3)
			{
	               		data = fdc_getdata(0);
				if (data != -1)
				{
					data &= 0xff;
				}
       		        	if ((data == -1) && (d86f[drive].datac < 3))
				{
					data = 0;
				}
				d86f[drive].format_sector_id.byte_array[d86f[drive].datac] = data & 0xff;
				// d86f_log("format_sector_id[%i] = %i\n", d86f[drive].datac, d86f[drive].format_sector_id.byte_array[d86f[drive].datac]);
       	        		if (d86f[drive].datac == 3)
				{
					fdc_stop_id_request();
					// d86f_log("Formatting sector: %08X (%i) (%i)...\n", d86f[drive].format_sector_id.dword, d86f[drive].track_pos, sc);
				}
			}
		case FMT_PRETRK_SYNC:
		case FMT_SECTOR_DATA_SYNC:
			max_len = sync_len;
			if (do_write)  d86f_write_direct(drive, side, 0x00, 0);
			break;
		case FMT_PRETRK_IAM:
			max_len = am_len;
			if (do_write)
			{
				if (mfm)
				{
					d86f_write_direct(drive, side, iam_mfm[d86f[drive].datac], 1);
				}
				else
				{
					d86f_write_direct(drive, side, iam_fm, 1);
				}
			}
			break;
		case FMT_PRETRK_GAP1:
			max_len = gap_sizes[1];
			if (do_write)  d86f_write_direct(drive, side, gap_fill, 0);
			break;
		case FMT_SECTOR_IDAM:
			max_len = am_len;
			if (mfm)
			{
				if (do_write)  d86f_write_direct(drive, side, idam_mfm[d86f[drive].datac], 1);
				d86f_calccrc(drive, (d86f[drive].datac < 3) ? 0xA1 : 0xFE);
			}
			else
			{
				if (do_write)  d86f_write_direct(drive, side, idam_fm, 1);
				d86f_calccrc(drive, 0xFE);
			}
			break;
		case FMT_SECTOR_ID:
			max_len = 4;
			if (do_write)
			{
				d86f_write_direct(drive, side, d86f[drive].format_sector_id.byte_array[d86f[drive].datac], 0);
				d86f_calccrc(drive, d86f[drive].format_sector_id.byte_array[d86f[drive].datac]);
			}
			else
			{
				if (d86f[drive].datac == 3)
				{
					d86f_handler[drive].set_sector(drive, side, d86f[drive].format_sector_id.id.c, d86f[drive].format_sector_id.id.h, d86f[drive].format_sector_id.id.r, d86f[drive].format_sector_id.id.n);
				}
			}
			break;
		case FMT_SECTOR_ID_CRC:
		case FMT_SECTOR_DATA_CRC:
			max_len = 2;
			if (do_write)  d86f_write_direct(drive, side, d86f[drive].calc_crc.bytes[d86f[drive].datac ^ 1], 0);
			break;
		case FMT_SECTOR_GAP2:
			max_len = gap_sizes[2];
			if (do_write)  d86f_write_direct(drive, side, gap_fill, 0);
			break;
		case FMT_SECTOR_DATAAM:
			max_len = am_len;
			if (mfm)
			{
				if (do_write)  d86f_write_direct(drive, side, dataam_mfm[d86f[drive].datac], 1);
				d86f_calccrc(drive, (d86f[drive].datac < 3) ? 0xA1 : 0xFB);
			}
			else
			{
				if (do_write)  d86f_write_direct(drive, side, dataam_fm, 1);
				d86f_calccrc(drive, 0xFB);
			}
			break;
		case FMT_SECTOR_DATA:
			max_len = dtl;
			if (do_write)  d86f_write_direct(drive, side, d86f[drive].fill, 0);
			d86f_calccrc(drive, d86f[drive].fill);
			break;
		case FMT_SECTOR_GAP3:
			max_len = gap_sizes[3];
			if (do_write)  d86f_write_direct(drive, side, gap_fill, 0);
			break;
	}

	d86f[drive].datac++;

	d86f_advance_word(drive, side);

	if ((d86f[drive].index_count) && (d86f[drive].format_state < FMT_SECTOR_ID_SYNC) || (d86f[drive].format_state > FMT_SECTOR_GAP3))
	{
		// d86f_log("Format finished regularly\n");
		d86f_format_finish(drive, side, mfm, sc, gap_fill, do_write);
		return;
	}

	if (d86f[drive].datac >= max_len)
	{
		d86f[drive].datac = 0;
		d86f[drive].format_state++;

		switch (d86f[drive].format_state)
		{
			case FMT_SECTOR_ID_SYNC:
				fdc_request_next_sector_id();
				break;
			case FMT_SECTOR_IDAM:
			case FMT_SECTOR_DATAAM:
				d86f[drive].calc_crc.word = 0xffff;
				break;
			case FMT_POSTTRK_CHECK:
				if (d86f[drive].index_count)
				{
					// d86f_log("Format finished with delay\n");
					d86f_format_finish(drive, side, mfm, sc, gap_fill, do_write);
					return;
				}
				d86f[drive].sector_count++;
				if (d86f[drive].sector_count < sc)
				{
					/* Sector within allotted amount, change state to SECTOR_ID_SYNC. */
					d86f[drive].format_state = FMT_SECTOR_ID_SYNC;
					fdc_request_next_sector_id();
					break;
				}
				else
				{
					d86f[drive].format_state = FMT_POSTTRK_GAP4;
					d86f[drive].sector_count = 0;
					break;
				}
		}
	}
}

void d86f_poll(int drive)
{
	int side = 0;
	int mfm = 1;

	side = fdd_get_head(drive);
	mfm = fdc_is_mfm();

	if ((d86f[drive].state & 0xF8) == 0xE8)
	{
		if (!d86f_can_format(drive))
		{
			d86f[drive].state = STATE_SECTOR_NOT_FOUND;
		}
	}

	if ((d86f[drive].state != STATE_IDLE) && (d86f[drive].state != STATE_SECTOR_NOT_FOUND) && ((d86f[drive].state & 0xF8) != 0xE8))
	{
		if (!d86f_can_read_address(drive))
		{
			/* if (fdc_get_bitcell_period() != d86f_get_bitcell_period(drive))  d86f_log("[%i, %i] Bitcell period mismatch (%i != %i)\n", drive, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive));
			if (!fdd_can_read_medium(real_drive(drive)))  d86f_log("[%i, %i] Drive can not read medium (hole = %01X)\n", drive, side, d86f_hole(drive));
			if (fdc_is_mfm() != d86f_is_mfm(drive))  d86f_log("[%i, %i] Encoding mismatch\n", drive, side);
			if (d86f_get_encoding(drive) > 1)  d86f_log("[%i, %i] Image encoding (%s) not FM or MFM\n", drive, side, (d86f_get_encoding(drive) == 2) ? "M2FM" : "GCR"); */

			d86f[drive].state = STATE_SECTOR_NOT_FOUND;
		}
	}

	if ((d86f[drive].state != STATE_02_SPIN_TO_INDEX) && (d86f[drive].state != STATE_0D_SPIN_TO_INDEX))
	{
		d86f_get_bit(drive, side ^ 1);
	}

	switch(d86f[drive].state)
	{
		case STATE_02_SPIN_TO_INDEX:
		case STATE_0D_SPIN_TO_INDEX:
			d86f_spin_to_index(drive, side);
			return;
		case STATE_02_FIND_ID:
		case STATE_05_FIND_ID:
		case STATE_09_FIND_ID:
		case STATE_06_FIND_ID:
		case STATE_0A_FIND_ID:
		case STATE_0C_FIND_ID:
		case STATE_11_FIND_ID:
		case STATE_16_FIND_ID:
			if (mfm)
			{
				d86f_find_address_mark_mfm(drive, side, &(d86f[drive].id_find), 0x5554, 0, 0);
			}
			else
			{
				d86f_find_address_mark_fm(drive, side, &(d86f[drive].id_find), 0xF57E, 0, 0);
			}
			break;
		case STATE_0A_READ_ID:
		case STATE_02_READ_ID:
			d86f_read_sector_id(drive, side, 0);
			break;
		case STATE_05_READ_ID:
		case STATE_09_READ_ID:
		case STATE_06_READ_ID:
		case STATE_0C_READ_ID:
		case STATE_11_READ_ID:
		case STATE_16_READ_ID:
			d86f_read_sector_id(drive, side, 1);
			break;
		case STATE_02_FIND_DATA:
			if (mfm)
			{
				d86f_find_address_mark_mfm(drive, side, &(d86f[drive].data_find), 0x5545, 0x554A, 2);
			}
			else
			{
				d86f_find_address_mark_fm(drive, side, &(d86f[drive].data_find), 0xF56F, 0xF56A, 2);
			}
			break;
		case STATE_06_FIND_DATA:
		case STATE_11_FIND_DATA:
		case STATE_16_FIND_DATA:
			if (mfm)
			{
				d86f_find_address_mark_mfm(drive, side, &(d86f[drive].data_find), 0x5545, 0x554A, fdc_is_sk() | 2);
			}
			else
			{
				d86f_find_address_mark_fm(drive, side, &(d86f[drive].data_find), 0xF56F, 0xF56A, fdc_is_sk() | 2);
			}
			break;
		case STATE_05_FIND_DATA:
		case STATE_09_FIND_DATA:
			if (mfm)
			{
				d86f_write_find_address_mark_mfm(drive, side, &(d86f[drive].data_find));
			}
			else
			{
				d86f_write_find_address_mark_fm(drive, side, &(d86f[drive].data_find));
			}
			break;
		case STATE_0C_FIND_DATA:
			if (mfm)
			{
				d86f_find_address_mark_mfm(drive, side, &(d86f[drive].data_find), 0x554A, 0x5545, fdc_is_sk() | 2);
			}
			else
			{
				d86f_find_address_mark_fm(drive, side, &(d86f[drive].data_find), 0xF56A, 0xF56F, fdc_is_sk() | 2);
			}
			break;
		case STATE_02_READ_DATA:
		case STATE_06_READ_DATA:
		case STATE_0C_READ_DATA:
		case STATE_11_SCAN_DATA:
		case STATE_16_VERIFY_DATA:
			d86f_read_sector_data(drive, side);
			break;
		case STATE_05_WRITE_DATA:
			if (mfm)
			{
				d86f_write_sector_data(drive, side, mfm, 0x5545);
			}
			else
			{
				d86f_write_sector_data(drive, side, mfm, 0xF56F);
			}
			break;
		case STATE_09_WRITE_DATA:
			if (mfm)
			{
				d86f_write_sector_data(drive, side, mfm, 0x554A);
			}
			else
			{
				d86f_write_sector_data(drive, side, mfm, 0xF56A);
			}
			break;
		case STATE_0D_FORMAT_TRACK:
			if (!(d86f[drive].track_pos & 15))
			{
				d86f_format_track(drive, side);
			}
			return;
		case STATE_IDLE:
		case STATE_SECTOR_NOT_FOUND:
		default:
			d86f_get_bit(drive, side);
			break;
	}

	d86f_advance_bit(drive, side);

	if (d86f_wrong_densel(drive) && (d86f[drive].state != STATE_IDLE))
	{
		// d86f_log("[State: %02X] [Side %i] No ID address mark (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
		d86f[drive].state = STATE_IDLE;
		fdc_noidam();
		return;
	}

	if ((d86f[drive].index_count == 2) && (d86f[drive].state != STATE_IDLE))
	{
		switch(d86f[drive].state)
		{
			case STATE_0A_FIND_ID:
			case STATE_SECTOR_NOT_FOUND:
				// d86f_log("[State: %02X] [Side %i] No ID address mark (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
				d86f[drive].state = STATE_IDLE;
				fdc_noidam();
				break;
			case STATE_02_FIND_DATA:
			case STATE_06_FIND_DATA:
			case STATE_11_FIND_DATA:
			case STATE_16_FIND_DATA:
			case STATE_05_FIND_DATA:
			case STATE_09_FIND_DATA:
			case STATE_0C_FIND_DATA:
				// d86f_log("[State: %02X] [Side %i] No data address mark (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
				d86f[drive].state = STATE_IDLE;
				fdc_nodataam();
				break;
			case STATE_02_SPIN_TO_INDEX:
			case STATE_02_READ_DATA:
			case STATE_05_WRITE_DATA:
			case STATE_06_READ_DATA:
			case STATE_09_WRITE_DATA:
			case STATE_0C_READ_DATA:
			case STATE_0D_SPIN_TO_INDEX:
			case STATE_0D_FORMAT_TRACK:
			case STATE_11_SCAN_DATA:
			case STATE_16_VERIFY_DATA:
				/* In these states, we should *NEVER* care about how many index pulses there have been. */
				break;
			default:
				d86f[drive].state = STATE_IDLE;
				if (d86f[drive].id_found)
				{
					if (d86f[drive].error_condition & 0x18)
					{
						if ((d86f[drive].error_condition & 0x18) == 0x08)
						{
							// d86f_log("[State: %02X] [Side %i] Bad cylinder (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
							fdc_badcylinder();
						}
						if ((d86f[drive].error_condition & 0x10) == 0x10)
						{
							// d86f_log("[State: %02X] [Side %i] Wrong cylinder (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
							fdc_wrongcylinder();
						}
					}
					else
					{
						// d86f_log("[State: %02X] [Side %i] Sector not found (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
						fdc_nosector();
					}
				}
				else
				{
					// d86f_log("[State: %02X] [Side %i] No ID address mark (%i != %i?) (%02X) (%08X) (%i)\n", d86f[drive].state, side, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), d86f_handler[drive].side_flags(drive), d86f[drive].req_sector.dword, d86f_handler[drive].get_raw_size(drive, side));
					fdc_noidam();
				}
				break;
		}
	}
}

#if 0
void d86f_poll(int drive)
{
	int i = 0;
	for (i = 0; i < 16; i++)
	{
		d86f_bit_poll(drive);
	}
}

void d86f_poll()
{
	int drive = 0;
	drive = fdc_get_drive();
	d86f_poll_per_drive(drive);
}
#endif

void d86f_reset_index_hole_pos(int drive, int side)
{
	d86f[drive].index_hole_pos[side] = 0;
}

uint16_t d86f_prepare_pretrack(int drive, int side, int iso)
{
	uint16_t i, pos;

	int mfm = 0;
	int real_gap0_len = 0;
	int sync_len = 0;
	int real_gap1_len = 0;
	uint16_t gap_fill = 0;
	uint32_t raw_size = 0;
	uint16_t iam_fm = 0xFAF7;
	uint16_t iam_mfm = 0x5255;

	mfm = d86f_is_mfm(drive);
	real_gap0_len = mfm ? 80 : 40;
	sync_len = mfm ? 12 : 6;
	real_gap1_len = mfm ? 50 : 26;
	gap_fill = mfm ? 0x4E : 0xFF;
	raw_size = d86f_handler[drive].get_raw_size(drive, side) >> 4;

	d86f[drive].index_hole_pos[side] = 0;

	for (i = 0; i < raw_size; i++)
	{
		d86f_write_direct_common(drive, side, gap_fill, 0, i);
	}

	pos = 0;

	if (!iso)
	{
		for (i = 0; i < real_gap0_len; i++)
		{
			d86f_write_direct_common(drive, side, gap_fill, 0, pos);
			pos = (pos + 1) % raw_size;
		}
		for (i = 0; i < sync_len; i++)
		{
			d86f_write_direct_common(drive, side, 0, 0, pos);
			pos = (pos + 1) % raw_size;
		}
		if (mfm)
		{
			for (i = 0; i < 3; i++)
			{
				d86f_write_direct_common(drive, side, 0x2452, 1, pos);
				pos = (pos + 1) % raw_size;
			}
		}
		d86f_write_direct_common(drive, side, mfm ? iam_mfm : iam_fm, 1, pos);
		pos = (pos + 1) % raw_size;
	}
	for (i = 0; i < real_gap0_len; i++)
	{
		d86f_write_direct_common(drive, side, gap_fill, 0, pos);
		pos = (pos + 1) % raw_size;
	}

	return pos;
}

uint16_t d86f_prepare_sector(int drive, int side, int prev_pos, uint8_t *id_buf, uint8_t *data_buf, int data_len, int gap2, int gap3, int deleted, int bad_crc)
{
	uint16_t pos;

	int i;

	int real_gap2_len = gap2;
	int real_gap3_len = gap3;
	int mfm = 0;
	int sync_len = 0;
	uint16_t gap_fill = 0;
	uint32_t raw_size = 0;
	uint16_t idam_fm = 0x7EF5;
	uint16_t dataam_fm = 0x6FF5;
	uint16_t datadam_fm = 0x6AF5;
	uint16_t idam_mfm = 0x5455;
	uint16_t dataam_mfm = 0x4555;
	uint16_t datadam_mfm = 0x4A55;

	mfm = d86f_is_mfm(drive);

	gap_fill = mfm ? 0x4E : 0xFF;
	raw_size = d86f_handler[drive].get_raw_size(drive, side) >> 4;

	pos = prev_pos;

	sync_len = mfm ? 12 : 6;

	for (i = 0; i < sync_len; i++)
	{
		d86f_write_direct_common(drive, side, 0, 0, pos);
		pos = (pos + 1) % raw_size;
	}
	d86f[drive].calc_crc.word = 0xffff;
	if (mfm)
	{
		for (i = 0; i < 3; i++)
		{
			d86f_write_direct_common(drive, side, 0x8944, 1, pos);
			pos = (pos + 1) % raw_size;
			d86f_calccrc(drive, 0xA1);
		}
	}
	d86f_write_direct_common(drive, side, mfm ? idam_mfm : idam_fm, 1, pos);
	pos = (pos + 1) % raw_size;
	d86f_calccrc(drive, 0xFE);
	for (i = 0; i < 4; i++)
	{
		d86f_write_direct_common(drive, side, id_buf[i], 0, pos);
		pos = (pos + 1) % raw_size;
		d86f_calccrc(drive, id_buf[i]);
	}
	for (i = 1; i >= 0; i--)
	{
		d86f_write_direct_common(drive, side, d86f[drive].calc_crc.bytes[i], 0, pos);
		pos = (pos + 1) % raw_size;
	}
	for (i = 0; i < real_gap2_len; i++)
	{
		d86f_write_direct_common(drive, side, gap_fill, 0, pos);
		pos = (pos + 1) % raw_size;
	}
	for (i = 0; i < sync_len; i++)
	{
		d86f_write_direct_common(drive, side, 0, 0, pos);
		pos = (pos + 1) % raw_size;
	}
	d86f[drive].calc_crc.word = 0xffff;
	if (mfm)
	{
		for (i = 0; i < 3; i++)
		{
			d86f_write_direct_common(drive, side, 0x8944, 1, pos);
			pos = (pos + 1) % raw_size;
			d86f_calccrc(drive, 0xA1);
		}
	}
	d86f_write_direct_common(drive, side, mfm ? (deleted ? datadam_mfm : dataam_mfm) : (deleted ? datadam_fm : dataam_fm), 1, pos);
	pos = (pos + 1) % raw_size;
	d86f_calccrc(drive, deleted ? 0xF8 : 0xFB);
	for (i = 0; i < data_len; i++)
	{
		d86f_write_direct_common(drive, side, data_buf[i], 0, pos);
		pos = (pos + 1) % raw_size;
		d86f_calccrc(drive, data_buf[i]);
	}
	if (bad_crc)
	{
		d86f[drive].calc_crc.word ^= 0xffff;
	}
	for (i = 1; i >= 0; i--)
	{
		d86f_write_direct_common(drive, side, d86f[drive].calc_crc.bytes[i], 0, pos);
		pos = (pos + 1) % raw_size;
	}
	for (i = 0; i < real_gap3_len; i++)
	{
		d86f_write_direct_common(drive, side, gap_fill, 0, pos);
		pos = (pos + 1) % raw_size;
	}

	return pos;
}

/* Note on handling of tracks on thick track drives:
	- On seek, encoded data is constructed from both (track << 1) and ((track << 1) + 1);
	- Any bits that differ are treated as thus:
		- Both are regular but contents differ -> Output is fuzzy;
		- One is regular and one is fuzzy -> Output is fuzzy;
		- Both are fuzzy -> Output is fuzzy;
		- Both are physical holes -> Output is a physical hole;
		- One is regular and one is a physical hole -> Output is puzzy, the hole half is handled appropriately on writeback;
		- One is fuzzy and one is a physical hole -> Output is puzzy, the hole half is handled appropriately on writeback;
	- On write back, apart from the above notes, the final two tracks are written;
	- Destination ALWAYS has surface data even if the image does not.
   In case of a thin track drive, tracks are handled normally. */

void d86f_construct_encoded_buffer(int drive, int side)
{
	int i = 0;
	/* *_fuzm are fuzzy bit masks, *_holm are hole masks, dst_neim are masks is mask for bits that are neither fuzzy nor holes in both,
	   and src1_d and src2_d are filtered source data. */
	uint16_t src1_fuzm, src2_fuzm, dst_fuzm, src1_holm, src2_holm, dst_holm, dst_neim, src1_d, src2_d;
	uint32_t len;
	uint16_t *dst = d86f[drive].track_encoded_data[side];
	uint16_t *dst_s = d86f[drive].track_surface_data[side];
	uint16_t *src1 = d86f[drive].thin_track_encoded_data[0][side];
	uint16_t *src1_s = d86f[drive].thin_track_surface_data[0][side];
	uint16_t *src2 = d86f[drive].thin_track_encoded_data[1][side];
	uint16_t *src2_s = d86f[drive].thin_track_surface_data[1][side];
	len = d86f_get_array_size(drive, side);

	for (i = 0; i < len; i++)
	{
		/* The two bits differ. */
		if (d86f_has_surface_desc(drive))
		{
			/* Source image has surface description data, so we have some more handling to do. */
			src1_fuzm = src1[i] & src1_s[i];
			src2_fuzm = src2[i] & src2_s[i];
			dst_fuzm = src1_fuzm | src2_fuzm;			/* The bits that remain set are fuzzy in either one or
										   the other or both. */
			src1_holm = src1[i] | (src1_s[i] ^ 0xffff);
			src2_holm = src2[i] | (src2_s[i] ^ 0xffff);
			dst_holm = (src1_holm & src2_holm) ^ 0xffff;		/* The bits that remain set are holes in both. */
			dst_neim = (dst_fuzm | dst_holm) ^ 0xffff;		/* The bits that remain set are those that are neither
										   fuzzy nor are holes in both. */
			src1_d = src1[i] & dst_neim;
			src2_d = src2[i] & dst_neim;

			dst_s[i] = (dst_neim ^ 0xffff);				/* The set bits are those that are either fuzzy or are
										   holes in both. */
			dst[i] = (src1_d | src2_d);				/* Initial data is remaining data from Source 1 and
										   Source 2. */
			dst[i] |= dst_fuzm;					/* Add to it the fuzzy bytes (holes have surface bit set
										   but data bit clear). */
		}
		else
		{
			/* No surface data, the handling is much simpler - a simple OR. */
			dst[i] = src1[i] | src2[i];
			dst_s[i] = 0;
		}
	}
}

/* Decomposition is easier since we at most have to care about the holes. */
void d86f_decompose_encoded_buffer(int drive, int side)
{
	int i = 0;
	uint16_t temp, temp2;
	uint32_t len;
	uint16_t *dst = d86f[drive].track_encoded_data[side];
	uint16_t *dst_s = d86f[drive].track_surface_data[side];
	uint16_t *src1 = d86f[drive].thin_track_encoded_data[0][side];
	uint16_t *src1_s = d86f[drive].thin_track_surface_data[0][side];
	uint16_t *src2 = d86f[drive].thin_track_encoded_data[1][side];
	uint16_t *src2_s = d86f[drive].thin_track_surface_data[1][side];
	len = d86f_get_array_size(drive, side);

	for (i = 0; i < len; i++)
	{
		if (d86f_has_surface_desc(drive))
		{
			/* Source image has surface description data, so we have some more handling to do.
			   We need hole masks for both buffers. Holes have data bit clear and surface bit set. */
			temp = src1[i] & (src1_s[i] ^ 0xffff);
			temp2 = src2[i] & (src2_s[i] ^ 0xffff);
			src1[i] = dst[i] & temp;
			src1_s[i] = temp ^ 0xffff;
			src2[i] = dst[i] & temp2;
			src2_s[i] = temp2 ^ 0xffff;
		}
		else
		{
			src1[i] = src2[i] = dst[i];
		}
	}
}

int d86f_track_header_size(int drive)
{
	int temp = 6;
	if (d86f_has_extra_bit_cells(drive))
	{
		temp += 4;
	}
	return temp;
}

void d86f_read_track(int drive, int track, int thin_track, int side, uint16_t *da, uint16_t *sa)
{
	int logical_track = 0;
	int array_size = 0;

	if (d86f_get_sides(drive) == 2)
	{
		logical_track = ((track + thin_track) << 1) + side;
	}
	else
	{
		logical_track = track + thin_track;
	}

	if (d86f[drive].track_offset[logical_track])
	{
		if (!thin_track)
		{
			fseek(d86f[drive].f, d86f[drive].track_offset[logical_track], SEEK_SET);
			fread(&(d86f[drive].side_flags[side]), 2, 1, d86f[drive].f);
			if (d86f_has_extra_bit_cells(drive))
			{
				fread(&(d86f[drive].extra_bit_cells[side]), 4, 1, d86f[drive].f);
				if (d86f[drive].extra_bit_cells[side] < -32768)
				{
					d86f[drive].extra_bit_cells[side] = -32768;
				}
				if (d86f[drive].extra_bit_cells[side] > 32768)
				{
					d86f[drive].extra_bit_cells[side] = 32768;
				}
			}
			else
			{
				d86f[drive].extra_bit_cells[side] = 0;
			}
			fread(&(d86f[drive].index_hole_pos[side]), 4, 1, d86f[drive].f);
		}
		else
		{
			fseek(d86f[drive].f, d86f[drive].track_offset[logical_track] + d86f_track_header_size(drive), SEEK_SET);
		}
		array_size = d86f_get_array_size(drive, side) << 1;
		if (d86f_has_surface_desc(drive))
		{
			fread(sa, 1, array_size, d86f[drive].f);
		}
		fread(da, 1, array_size, d86f[drive].f);
	}
	else
	{
		if (!thin_track)
		{
			switch((d86f[drive].disk_flags >> 1) & 3)
			{
				case 0:
				default:
					d86f[drive].side_flags[side] = 0x0A;
					break;
				case 1:
					d86f[drive].side_flags[side] = 0x00;
					break;
				case 2:
				case 3:
					d86f[drive].side_flags[side] = 0x03;
					break;
			}
			d86f[drive].extra_bit_cells[side] = 0;
		}
	}
}

void d86f_seek(int drive, int track)
{
	uint8_t track_id = track;
	int sides;
        int side, thin_track;
	sides = d86f_get_sides(drive);

	/* If the drive has thick tracks, shift the track number by 1. */
        if (!fdd_doublestep_40(drive))
	{
                track <<= 1;

		for (thin_track = 0; thin_track < sides; thin_track++)
		{
			for (side = 0; side < sides; side++)
			{
				if (d86f_has_surface_desc(drive))
				{
					memset(d86f[drive].thin_track_surface_data[thin_track][side], 0, 106096);
				}
				memset(d86f[drive].thin_track_encoded_data[thin_track][side], 0, 106096);
			}
		}
	}

	for (side = 0; side < sides; side++)
	{
		if (d86f_has_surface_desc(drive))
		{
			memset(d86f[drive].track_surface_data[side], 0, 106096);
		}
		memset(d86f[drive].track_encoded_data[side], 0, 106096);
	}

	d86f[drive].cur_track = track;

        if (!fdd_doublestep_40(drive))
	{
		for (side = 0; side < sides; side++)
		{
			for (thin_track = 0; thin_track < 2; thin_track++)
			{
				d86f_read_track(drive, track, thin_track, side, d86f[drive].thin_track_encoded_data[thin_track][side], d86f[drive].thin_track_surface_data[thin_track][side]);
			}

			d86f_construct_encoded_buffer(drive, side);
		}
	}
	else
	{
		for (side = 0; side < sides; side++)
		{
			d86f_read_track(drive, track, 0, side, d86f[drive].track_encoded_data[side], d86f[drive].track_surface_data[side]);
		}
	}

	d86f[drive].state = STATE_IDLE;
}

void d86f_write_track(int drive, int side, uint16_t *da0, uint16_t *sa0)
{
	// d86f_log("Pos: %08X\n", ftell(d86f[drive].f));

	fwrite(&(d86f[drive].side_flags[side]), 1, 2, d86f[drive].f);

	if (d86f_has_extra_bit_cells(drive))
	{
		fwrite(&(d86f[drive].extra_bit_cells[side]), 1, 4, d86f[drive].f);
	}

	fwrite(&(d86f[drive].index_hole_pos[side]), 1, 4, d86f[drive].f);

	if (d86f_has_surface_desc(drive))
	{
		fwrite(sa0, 1, d86f_get_array_size(drive, side) << 1, d86f[drive].f);
	}

	fwrite(da0, 1, d86f_get_array_size(drive, side) << 1, d86f[drive].f);

	// d86f_log("Pos: %08X\n", ftell(d86f[drive].f));
}

int d86f_get_track_table_size(int drive)
{
	int temp = 2048;

	if (d86f_get_sides(drive) == 1)
	{
		temp >>= 1;
	}

	return temp;
}

void d86f_writeback(int drive)
{
	uint8_t track_id = d86f[drive].cur_track;
	uint8_t header[32];
	int sides,  header_size;
        int side, thin_track;
	// uint64_t crc64;
	uint32_t len;
	int i = 0;
	int ret = 0;
	int logical_track = 0;
	uint8_t tempb;
	FILE *cf;
	sides = d86f_get_sides(drive);
	header_size = d86f_header_size(drive);

        if (!d86f[drive].f)
	{
                return;
	}

	/* First write the track offsets table. */
	fseek(d86f[drive].f, 0, SEEK_SET);
	fread(header, 1, header_size, d86f[drive].f);

	fseek(d86f[drive].f, 8, SEEK_SET);
	// d86f_log("PosEx: %08X\n", ftell(d86f[drive].f));
	fwrite(d86f[drive].track_offset, 1, d86f_get_track_table_size(drive), d86f[drive].f);
	// d86f_log("PosEx: %08X\n", ftell(d86f[drive].f));

        if (!fdd_doublestep_40(drive))
	{
		for (side = 0; side < sides; side++)
		{
			d86f_decompose_encoded_buffer(drive, side);

			for (thin_track = 0; thin_track < 2; thin_track++)
			{
				if (d86f_get_sides(drive) == 2)
				{
					logical_track = ((d86f[drive].cur_track + thin_track) << 1) + side;
				}
				else
				{
					logical_track = d86f[drive].cur_track + thin_track;
				}
				if (d86f[drive].track_offset[logical_track])
				{
					fseek(d86f[drive].f, d86f[drive].track_offset[logical_track], SEEK_SET);
					d86f_write_track(drive, side, d86f[drive].thin_track_encoded_data[thin_track][side], d86f[drive].thin_track_surface_data[thin_track][side]);
				}
			}
		}
	}
	else
	{
		for (side = 0; side < sides; side++)
		{
			if (d86f_get_sides(drive) == 2)
			{
				logical_track = (d86f[drive].cur_track << 1) + side;
			}
			else
			{
				logical_track = d86f[drive].cur_track;
			}
			if (d86f[drive].track_offset[logical_track])
			{
				// d86f_log("Writing track...\n");
				fseek(d86f[drive].f, d86f[drive].track_offset[logical_track], SEEK_SET);
				d86f_write_track(drive, side, d86f[drive].track_encoded_data[side], d86f[drive].track_surface_data[side]);
			}
		}
	}

	// d86f_log("Position: %08X\n", ftell(d86f[drive].f));

	if (d86f[drive].is_compressed)
	{
		/* The image is compressed. */

		/* Open the original, compressed file. */
		cf = fopen(d86f[drive].original_file_name, "wb");

		/* Write the header to the original file. */
		fwrite(header, 1, header_size, cf);

		fseek(d86f[drive].f, 0, SEEK_END);
		len = ftell(d86f[drive].f);
		len -= header_size;

		fseek(d86f[drive].f, header_size, SEEK_SET);

		/* Compress data from the temporary uncompressed file to the original, compressed file. */
	        d86f[drive].filebuf = (uint8_t *) malloc(len);
	        d86f[drive].outbuf = (uint8_t *) malloc(len - 1);
	        fread(d86f[drive].filebuf, 1, len, d86f[drive].f);
	        ret = lzf_compress(d86f[drive].filebuf, len, d86f[drive].outbuf, len - 1);

		// ret = d86f_zlib(cf, d86f[drive].f, 0);
		if (!ret)
		{
			d86f_log("86F: Error compressing file\n");
		}

		fwrite(d86f[drive].outbuf, 1, ret, cf);
		free(d86f[drive].outbuf);
		free(d86f[drive].filebuf);

#ifdef DO_CRC64
		len = ftell(cf);

		fclose(cf);
		cf = fopen(d86f[drive].original_file_name, "rb+");

		crc64 = 0xffffffffffffffff;
		fseek(cf, 8, SEEK_SET);
		fwrite(&crc64, 1, 8, cf);

		fseek(cf, 0, SEEK_SET);
		d86f[drive].filebuf = (uint8_t *) malloc(len);
		fread(d86f[drive].filebuf, 1, len, cf);
		*(uint64_t *) &(d86f[drive].filebuf[8]) = 0xffffffffffffffff;

		crc64 = (uint64_t) crc64speed(0, d86f[drive].filebuf, len);
		free(d86f[drive].filebuf);
		
		fseek(cf, 8, SEEK_SET);
		fwrite(&crc64, 1, 8, cf);

		/* Close the original file. */
		fclose(cf);
#endif
	}
#ifdef DO_CRC64
	else
	{
		fseek(d86f[drive].f, 0, SEEK_END);
		len = ftell(d86f[drive].f);

		fseek(d86f[drive].f, 0, SEEK_SET);

		crc64 = 0xffffffffffffffff;
		fseek(d86f[drive].f, 8, SEEK_SET);
		fwrite(&crc64, 1, 8, d86f[drive].f);

		fseek(d86f[drive].f, 0, SEEK_SET);
		d86f[drive].filebuf = (uint8_t *) malloc(len);
		fread(d86f[drive].filebuf, 1, len, d86f[drive].f);
		*(uint64_t *) &(d86f[drive].filebuf[8]) = 0xffffffffffffffff;

		crc64 = (uint64_t) crc64speed(0, d86f[drive].filebuf, len);
		free(d86f[drive].filebuf);

		fseek(d86f[drive].f, 8, SEEK_SET);
		fwrite(&crc64, 1, 8, d86f[drive].f);
	}
#endif

	// d86f_log("d86f_writeback(): %08X\n", d86f[drive].track_offset[track]);
}

void d86f_stop(int drive)
{
        d86f[drive].state = STATE_IDLE;
}

int d86f_common_command(int drive, int sector, int track, int side, int rate, int sector_size)
{
        d86f_log("d86f_common_command (drive %i): fdc_period=%i img_period=%i rate=%i sector=%i track=%i side=%i\n", drive, fdc_get_bitcell_period(), d86f_get_bitcell_period(drive), rate, sector, track, side);

        d86f[drive].req_sector.id.c = track;
        d86f[drive].req_sector.id.h = side;
	if (sector == SECTOR_FIRST)
	{
		d86f[drive].req_sector.id.r = 1;
	}
	else if (sector == SECTOR_NEXT)
	{
		d86f[drive].req_sector.id.r++;
	}
	else
	{
	        d86f[drive].req_sector.id.r = sector;
	}
	d86f[drive].req_sector.id.n = sector_size;

	if (fdd_get_head(drive) && (d86f_get_sides(drive) == 1))
	{
		// d86f_log("Wrong side!\n");
		fdc_noidam();
		d86f[drive].state = STATE_IDLE;
		d86f[drive].index_count = 0;
		return 0;
	}

	d86f[drive].id_find.sync_marks = d86f[drive].id_find.bits_obtained = d86f[drive].id_find.bytes_obtained = 0;
	d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
	d86f[drive].index_count = d86f[drive].error_condition = d86f[drive].satisfying_bytes = 0;
	d86f[drive].id_found = 0;
	d86f[drive].dma_over = 0;

	return 1;
}

void d86f_readsector(int drive, int sector, int track, int side, int rate, int sector_size)
{
	int ret = 0;

	ret = d86f_common_command(drive, sector, track, side, rate, sector_size);
	if (!ret)  return;

        if (sector == SECTOR_FIRST)
                d86f[drive].state = STATE_02_SPIN_TO_INDEX;
        else if (sector == SECTOR_NEXT)
                d86f[drive].state = STATE_02_FIND_ID;
	else
	        d86f[drive].state = fdc_is_deleted() ? STATE_0C_FIND_ID : (fdc_is_verify() ? STATE_16_FIND_ID : STATE_06_FIND_ID);
}

void d86f_writesector(int drive, int sector, int track, int side, int rate, int sector_size)
{
	int ret = 0;

	if (writeprot[drive])
	{
		fdc_writeprotect();
		d86f[drive].state = STATE_IDLE;
		d86f[drive].index_count = 0;
		return;
	}

	ret = d86f_common_command(drive, sector, track, side, rate, sector_size);
	if (!ret)  return;

        d86f[drive].state = fdc_is_deleted() ? STATE_09_FIND_ID : STATE_05_FIND_ID;
}

void d86f_comparesector(int drive, int sector, int track, int side, int rate, int sector_size)
{
	int ret = 0;

	ret = d86f_common_command(drive, sector, track, side, rate, sector_size);
	if (!ret)  return;

        d86f[drive].state = STATE_11_FIND_ID;
}

void d86f_readaddress(int drive, int side, int rate)
{
	// d86f_log("Reading sector ID on drive %i...\n", drive);

	if (fdd_get_head(drive) && (d86f_get_sides(drive) == 1))
	{
		// d86f_log("Trying to access the second side of a single-sided disk\n");
		fdc_noidam();
		d86f[drive].state = STATE_IDLE;
		d86f[drive].index_count = 0;
		return;
	}

	d86f[drive].id_find.sync_marks = d86f[drive].id_find.bits_obtained = d86f[drive].id_find.bytes_obtained = 0;
	d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
	d86f[drive].index_count = d86f[drive].error_condition = d86f[drive].satisfying_bytes = 0;
	d86f[drive].id_found = 0;
	d86f[drive].dma_over = 0;

        d86f[drive].state = STATE_0A_FIND_ID;
}

void d86f_add_track(int drive, int track, int side)
{
	uint32_t array_size;
	int logical_track = 0;

	array_size = d86f_get_array_size(drive, side);
	array_size <<= 1;

	if (d86f_get_sides(drive) == 2)
	{
		logical_track = (track << 1) + side;
	}
	else
	{
		if (side)
		{
			return;
		}
		logical_track = track;
	}

	if (!d86f[drive].track_offset[logical_track])
	{
		/* Track is absent from the file, let's add it. */
		d86f[drive].track_offset[logical_track] = d86f[drive].file_size;

		d86f[drive].file_size += (array_size + 6);
		if (d86f_has_extra_bit_cells(drive))
		{
			d86f[drive].file_size += 4;
		}
		if (d86f_has_surface_desc(drive))
		{
			d86f[drive].file_size += array_size;
		}
	}
}

void d86f_common_format(int drive, int side, int rate, uint8_t fill, int proxy)
{
	int i = 0;
	uint16_t temp, temp2;
	uint32_t array_size;

	if (writeprot[drive])
	{
		fdc_writeprotect();
		d86f[drive].state = STATE_IDLE;
		d86f[drive].index_count = 0;
		return;
	}

	if ((side && (d86f_get_sides(drive) == 1)) || !(d86f_can_format(drive)))
	{
		fdc_cannotformat();
		d86f[drive].state = STATE_IDLE;
		d86f[drive].index_count = 0;
		return;
	}

	if (!proxy)
	{
		d86f_reset_index_hole_pos(drive, side);

		if (d86f[drive].cur_track > 256)
		{
			// d86f_log("Track above 256\n");
			fdc_writeprotect();
			d86f[drive].state = STATE_IDLE;
			d86f[drive].index_count = 0;
			return;
		}

		array_size = d86f_get_array_size(drive, side);

		if (d86f_has_surface_desc(drive))
		{
			/* Preserve the physical holes but get rid of the fuzzy bytes. */
			for (i = 0; i < array_size; i++)
			{
				temp = d86f[drive].track_encoded_data[side][i] ^ 0xffff;
				temp2 = d86f[drive].track_surface_data[side][i];
				temp &= temp2;
				d86f[drive].track_surface_data[side][i] = temp;
			}
		}
		/* Zero the data buffer. */
		memset(d86f[drive].track_encoded_data[side], 0, array_size << 1);

		d86f_add_track(drive, d86f[drive].cur_track, side);
		if (!fdd_doublestep_40(drive))
		{
			d86f_add_track(drive, d86f[drive].cur_track + 1, side);
		}
	}

	// d86f_log("Formatting track %i side %i\n", track, side);

        d86f[drive].fill  = fill;

	if (!proxy)
	{
		// d86f[drive].side_flags[side] &= 0xc0;
		d86f[drive].side_flags[side] = 0;
		d86f[drive].side_flags[side] |= (fdd_getrpm(real_drive(drive)) == 360) ? 0x20 : 0;
		d86f[drive].side_flags[side] |= fdc_get_bit_rate();
		d86f[drive].side_flags[side] |= fdc_is_mfm() ? 8 : 0;

		d86f[drive].index_hole_pos[side] = 0;
	}

	d86f[drive].id_find.sync_marks = d86f[drive].id_find.bits_obtained = d86f[drive].id_find.bytes_obtained = 0;
	d86f[drive].data_find.sync_marks = d86f[drive].data_find.bits_obtained = d86f[drive].data_find.bytes_obtained = 0;
	d86f[drive].index_count = d86f[drive].error_condition = d86f[drive].satisfying_bytes = d86f[drive].sector_count = 0;
	d86f[drive].dma_over = 0;

        d86f[drive].state = STATE_0D_SPIN_TO_INDEX;
}

void d86f_proxy_format(int drive, int side, int rate, uint8_t fill)
{
	d86f_common_format(drive, side, rate, fill, 1);
}

void d86f_format(int drive, int side, int rate, uint8_t fill)
{
	d86f_common_format(drive, side, rate, fill, 0);
}

void d86f_common_handlers(int drive)
{
        drives[drive].readsector  = d86f_readsector;
        drives[drive].writesector = d86f_writesector;
        drives[drive].comparesector=d86f_comparesector;
        drives[drive].readaddress = d86f_readaddress;
        drives[drive].hole        = d86f_hole;
        drives[drive].byteperiod  = d86f_byteperiod;
        drives[drive].poll        = d86f_poll;
        drives[drive].format      = d86f_proxy_format;
        drives[drive].stop        = d86f_stop;
}

void d86f_load(int drive, char *fn)
{
	uint32_t magic = 0;
	uint32_t len = 0;
	uint32_t len2 = 0;
	uint8_t temp_file_name[2048];
	uint16_t temp = 0;
	uint8_t tempb = 0;
	// uint64_t crc64 = 0;
	// uint64_t read_crc64 = 0;
	int i = 0;
	FILE *tf;

	d86f_unregister(drive);

	writeprot[drive] = 0;
        d86f[drive].f = fopen(fn, "rb+");
        if (!d86f[drive].f)
        {
                d86f[drive].f = fopen(fn, "rb");
                if (!d86f[drive].f)
                        return;
                writeprot[drive] = 1;
        }
	if (ui_writeprot[drive])
	{
                writeprot[drive] = 1;
	}
        fwriteprot[drive] = writeprot[drive];

	fseek(d86f[drive].f, 0, SEEK_END);
	len = ftell(d86f[drive].f);
	fseek(d86f[drive].f, 0, SEEK_SET);

	fread(&magic, 4, 1, d86f[drive].f);

	if (len < 16)
	{
		/* File is WAY too small, abort. */
		fclose(d86f[drive].f);
		return;
	}

	if ((magic != 0x46423638) && (magic != 0x66623638))
	{
		/* File is not of the valid format, abort. */
		d86f_log("86F: Unrecognized magic bytes: %08X\n", magic);
		fclose(d86f[drive].f);
		return;
	}

	fread(&(d86f[drive].version), 2, 1, d86f[drive].f);

	if (d86f[drive].version != D86FVER)
	{
		/* File is not of a recognized format version, abort. */
		if (d86f[drive].version == 0x0063)
		{
			d86f_log("86F: File has emulator-internal version 0.99, this version is not valid in a file\n");
		}
		else if ((d86f[drive].version >= 0x0100) && (d86f[drive].version < D86FVER))
		{
			d86f_log("86F: No longer supported development file version: %i.%02i\n", d86f[drive].version >> 8, d86f[drive].version & 0xFF);
		}
		else
		{
			d86f_log("86F: Unrecognized file version: %i.%02i\n", d86f[drive].version >> 8, d86f[drive].version & 0xFF);
		}
		fclose(d86f[drive].f);
		return;
	}
	else
	{
		d86f_log("86F: Recognized file version: %i.%02i\n", d86f[drive].version >> 8, d86f[drive].version & 0xFF);
	}

	fread(&(d86f[drive].disk_flags), 2, 1, d86f[drive].f);

	d86f[drive].is_compressed = (magic == 0x66623638) ? 1 : 0;

	if ((len < 51052) && !d86f[drive].is_compressed)
	{
		/* File too small, abort. */
		fclose(d86f[drive].f);
		return;
	}

#ifdef DO_CRC64
	fseek(d86f[drive].f, 8, SEEK_SET);
	fread(&read_crc64, 1, 8, d86f[drive].f);

	fseek(d86f[drive].f, 0, SEEK_SET);

	crc64 = 0xffffffffffffffff;

	d86f[drive].filebuf = malloc(len);
	fread(d86f[drive].filebuf, 1, len, d86f[drive].f);
	*(uint64_t *) &(d86f[drive].filebuf[8]) = 0xffffffffffffffff;
	crc64 = (uint64_t) crc64speed(0, d86f[drive].filebuf, len);
	free(d86f[drive].filebuf);

	if (crc64 != read_crc64)
	{
		d86f_log("86F: CRC64 error\n");
		fclose(d86f[drive].f);
		return;
	}
#endif

	if (d86f[drive].is_compressed)
	{
	        append_filename(temp_file_name, pcempath, drive ? "TEMP$$$1.$$$" : "TEMP$$$0.$$$", 511);
		memcpy(temp_file_name, drive ? "TEMP$$$1.$$$" : "TEMP$$$0.$$$", 13);
		memcpy(d86f[drive].original_file_name, fn, strlen(fn) + 1);

		fclose(d86f[drive].f);

	        d86f[drive].f = fopen(temp_file_name, "wb");
        	if (!d86f[drive].f)
	        {
			d86f_log("86F: Unable to create temporary decompressed file\n");
                        return;
		}

		tf = fopen(fn, "rb");

		for (i = 0; i < 8; i++)
		{
			fread(&temp, 1, 2, tf);
			fwrite(&temp, 1, 2, d86f[drive].f);
		}

		// temp = d86f_zlib(d86f[drive].f, tf, 1);
		
		d86f[drive].filebuf = (uint8_t *) malloc(len);
		d86f[drive].outbuf = (uint8_t *) malloc(67108864);
		fread(d86f[drive].filebuf, 1, len, tf);
		temp = lzf_decompress(d86f[drive].filebuf, len, d86f[drive].outbuf, 67108864);
		if (temp)
		{
			fwrite(d86f[drive].outbuf, 1, temp, d86f[drive].f);
		}
		free(d86f[drive].outbuf);
		free(d86f[drive].filebuf);

		fclose(tf);
		fclose(d86f[drive].f);

		if (!temp)
		{
			d86f_log("86F: Error decompressing file\n");
			remove(temp_file_name);
			return;
		}

		d86f[drive].f = fopen(temp_file_name, "rb+");
	}

	if (d86f[drive].disk_flags & 0x100)
	{
		/* Zoned disk. */
		d86f_log("86F: Disk is zoned (Apple or Sony)\n");
		fclose(d86f[drive].f);
		if (d86f[drive].is_compressed)
		{
			remove(temp_file_name);
		}
		return;
	}

	if (d86f[drive].disk_flags & 0x600)
	{
		/* Zone type is not 0 but the disk is fixed-RPM. */
		d86f_log("86F: Disk is fixed-RPM but zone type is not 0\n");
		fclose(d86f[drive].f);
		if (d86f[drive].is_compressed)
		{
			remove(temp_file_name);
		}
		return;
	}

	if (!writeprot[drive])
	{
		writeprot[drive] = (d86f[drive].disk_flags & 0x10) ? 1 : 0;
	        fwriteprot[drive] = writeprot[drive];
	}

	if (writeprot[drive])
	{
		fclose(d86f[drive].f);

		if (d86f[drive].is_compressed)
		{
			d86f[drive].f = fopen(temp_file_name, "rb");
		}
		else
		{
			d86f[drive].f = fopen(fn, "rb");
		}
	}

	fseek(d86f[drive].f, 8, SEEK_SET);

	fread(d86f[drive].track_offset, 1, d86f_get_track_table_size(drive), d86f[drive].f);

	if (!(d86f[drive].track_offset[0]))
	{
		/* File has no track 0 side 0, abort. */
		d86f_log("86F: No Track 0 side 0\n");
		fclose(d86f[drive].f);
		return;
	}

	if ((d86f_get_sides(drive) == 2) && !(d86f[drive].track_offset[1]))
	{
		/* File is 2-sided but has no track 0 side 1, abort. */
		d86f_log("86F: No Track 0 side 0\n");
		fclose(d86f[drive].f);
		return;
	}

	/* Load track 0 flags as default. */
	fseek(d86f[drive].f, d86f[drive].track_offset[0], SEEK_SET);
	fread(&(d86f[drive].side_flags[0]), 2, 1, d86f[drive].f);
	if (d86f[drive].disk_flags & 0x80)
	{
		fread(&(d86f[drive].extra_bit_cells[0]), 4, 1, d86f[drive].f);
		if (d86f[drive].extra_bit_cells[0] < -32768)  d86f[drive].extra_bit_cells[0] = -32768;
		if (d86f[drive].extra_bit_cells[0] > 32768)  d86f[drive].extra_bit_cells[0] = 32768;
	}
	else
	{
		d86f[drive].extra_bit_cells[0] = 0;
	}

	if (d86f_get_sides(drive) == 2)
	{
		fseek(d86f[drive].f, d86f[drive].track_offset[1], SEEK_SET);
		fread(&(d86f[drive].side_flags[1]), 2, 1, d86f[drive].f);
		if (d86f[drive].disk_flags & 0x80)
		{
			fread(&(d86f[drive].extra_bit_cells[1]), 4, 1, d86f[drive].f);
			if (d86f[drive].extra_bit_cells[1] < -32768)  d86f[drive].extra_bit_cells[1] = -32768;
			if (d86f[drive].extra_bit_cells[1] > 32768)  d86f[drive].extra_bit_cells[1] = 32768;
		}
		else
		{
			d86f[drive].extra_bit_cells[0] = 0;
		}
	}
	else
	{
		switch ((d86f[drive].disk_flags >> 1) >> 3)
		{
			case 0:
			default:
				d86f[drive].side_flags[1] = 0x0A;
				break;
			case 1:
				d86f[drive].side_flags[1] = 0x00;
				break;
			case 2:
			case 3:
				d86f[drive].side_flags[1] = 0x03;
				break;
		}
		d86f[drive].extra_bit_cells[1] = 0;
	}

	fseek(d86f[drive].f, 0, SEEK_END);
	d86f[drive].file_size = ftell(d86f[drive].f);

	fseek(d86f[drive].f, 0, SEEK_SET);

	d86f_register_86f(drive);

        drives[drive].seek        = d86f_seek;
	d86f_common_handlers(drive);
        drives[drive].format      = d86f_format;

	d86f_log("86F: Disk is %scompressed and %s surface description data\n", d86f[drive].is_compressed ? "" : "not ", d86f_has_surface_desc(drive) ? "has" : "does not have");
}

void d86f_init()
{
	disc_random_init();

        memset(d86f, 0, sizeof(d86f));
        d86f_setupcrc(0x1021);

	// crc64speed_init();

	d86f[0].state = d86f[1].state = STATE_IDLE;
}

void d86f_close(int drive)
{
	uint8_t temp_file_name[2048];

        append_filename(temp_file_name, pcempath, drive ? "TEMP$$$1.$$$" : "TEMP$$$0.$$$", 511);
	memcpy(temp_file_name, drive ? "TEMP$$$1.$$$" : "TEMP$$$0.$$$", 13);

        if (d86f[drive].f)
                fclose(d86f[drive].f);
	if (d86f[drive].is_compressed)
                remove(temp_file_name);
        d86f[drive].f = NULL;
}
