/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Various ASCII to Unicode maps, for the various codepages.
 *
 * Version:	@(#)prt_cpmap.c	1.0.2	2018/10/05
 *
 * Authors:	Michael Dr�ing, <michael@drueing.de>
 *		Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Based on code by Frederic Weymann (originally for DosBox.)
 *
 *		Copyright 2018 Michael Dr�ing.
 *		Copyright 2018 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include "86box.h"
#include "plat.h" 
#include "printer.h"


static const uint16_t cp437Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
    0x00ff,0x00d6,0x00dc,0x00a2,0x00a3,0x00a5,0x20a7,0x0192,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
    0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x03b1,0x00df,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
    0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
    0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp737Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x0391,0x0392,0x0393,0x0394,0x0395,0x0396,0x0397,0x0398,
    0x0399,0x039a,0x039b,0x039c,0x039d,0x039e,0x039f,0x03a0,
    0x03a1,0x03a3,0x03a4,0x03a5,0x03a6,0x03a7,0x03a8,0x03a9,
    0x03b1,0x03b2,0x03b3,0x03b4,0x03b5,0x03b6,0x03b7,0x03b8,
    0x03b9,0x03ba,0x03bb,0x03bc,0x03bd,0x03be,0x03bf,0x03c0,
    0x03c1,0x03c3,0x03c2,0x03c4,0x03c5,0x03c6,0x03c7,0x03c8,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x03c9,0x03ac,0x03ad,0x03ae,0x03ca,0x03af,0x03cc,0x03cd,
    0x03cb,0x03ce,0x0386,0x0388,0x0389,0x038a,0x038c,0x038e,
    0x038f,0x00b1,0x2265,0x2264,0x03aa,0x03ab,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp775Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x0106,0x00fc,0x00e9,0x0101,0x00e4,0x0123,0x00e5,0x0107,
    0x0142,0x0113,0x0156,0x0157,0x012b,0x0179,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x014d,0x00f6,0x0122,0x00a2,0x015a,
    0x015b,0x00d6,0x00dc,0x00f8,0x00a3,0x00d8,0x00d7,0x00a4,
    0x0100,0x012a,0x00f3,0x017b,0x017c,0x017a,0x201d,0x00a6,
    0x00a9,0x00ae,0x00ac,0x00bd,0x00bc,0x0141,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x0104,0x010c,0x0118,
    0x0116,0x2563,0x2551,0x2557,0x255d,0x012e,0x0160,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x0172,0x016a,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x017d,
    0x0105,0x010d,0x0119,0x0117,0x012f,0x0161,0x0173,0x016b,
    0x017e,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x00d3,0x00df,0x014c,0x0143,0x00f5,0x00d5,0x00b5,0x0144,
    0x0136,0x0137,0x013b,0x013c,0x0146,0x0112,0x0145,0x2019,
    0x00ad,0x00b1,0x201c,0x00be,0x00b6,0x00a7,0x00f7,0x201e,
    0x00b0,0x2219,0x00b7,0x00b9,0x00b3,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp850Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
    0x00ff,0x00d6,0x00dc,0x00f8,0x00a3,0x00d8,0x00d7,0x0192,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
    0x00bf,0x00ae,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x00c1,0x00c2,0x00c0,
    0x00a9,0x2563,0x2551,0x2557,0x255d,0x00a2,0x00a5,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x00e3,0x00c3,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x00a4,
    0x00f0,0x00d0,0x00ca,0x00cb,0x00c8,0x0131,0x00cd,0x00ce,
    0x00cf,0x2518,0x250c,0x2588,0x2584,0x00a6,0x00cc,0x2580,
    0x00d3,0x00df,0x00d4,0x00d2,0x00f5,0x00d5,0x00b5,0x00fe,
    0x00de,0x00da,0x00db,0x00d9,0x00fd,0x00dd,0x00af,0x00b4,
    0x00ad,0x00b1,0x2017,0x00be,0x00b6,0x00a7,0x00f7,0x00b8,
    0x00b0,0x00a8,0x00b7,0x00b9,0x00b3,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp852Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x016f,0x0107,0x00e7,
    0x0142,0x00eb,0x0150,0x0151,0x00ee,0x0179,0x00c4,0x0106,
    0x00c9,0x0139,0x013a,0x00f4,0x00f6,0x013d,0x013e,0x015a,
    0x015b,0x00d6,0x00dc,0x0164,0x0165,0x0141,0x00d7,0x010d,
    0x00e1,0x00ed,0x00f3,0x00fa,0x0104,0x0105,0x017d,0x017e,
    0x0118,0x0119,0x00ac,0x017a,0x010c,0x015f,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x00c1,0x00c2,0x011a,
    0x015e,0x2563,0x2551,0x2557,0x255d,0x017b,0x017c,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x0102,0x0103,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x00a4,
    0x0111,0x0110,0x010e,0x00cb,0x010f,0x0147,0x00cd,0x00ce,
    0x011b,0x2518,0x250c,0x2588,0x2584,0x0162,0x016e,0x2580,
    0x00d3,0x00df,0x00d4,0x0143,0x0144,0x0148,0x0160,0x0161,
    0x0154,0x00da,0x0155,0x0170,0x00fd,0x00dd,0x0163,0x00b4,
    0x00ad,0x02dd,0x02db,0x02c7,0x02d8,0x00a7,0x00f7,0x00b8,
    0x00b0,0x00a8,0x02d9,0x0171,0x0158,0x0159,0x25a0,0x00a0
};

static const uint16_t cp855Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x0452,0x0402,0x0453,0x0403,0x0451,0x0401,0x0454,0x0404,
    0x0455,0x0405,0x0456,0x0406,0x0457,0x0407,0x0458,0x0408,
    0x0459,0x0409,0x045a,0x040a,0x045b,0x040b,0x045c,0x040c,
    0x045e,0x040e,0x045f,0x040f,0x044e,0x042e,0x044a,0x042a,
    0x0430,0x0410,0x0431,0x0411,0x0446,0x0426,0x0434,0x0414,
    0x0435,0x0415,0x0444,0x0424,0x0433,0x0413,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x0445,0x0425,0x0438,
    0x0418,0x2563,0x2551,0x2557,0x255d,0x0439,0x0419,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x043a,0x041a,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x00a4,
    0x043b,0x041b,0x043c,0x041c,0x043d,0x041d,0x043e,0x041e,
    0x043f,0x2518,0x250c,0x2588,0x2584,0x041f,0x044f,0x2580,
    0x042f,0x0440,0x0420,0x0441,0x0421,0x0442,0x0422,0x0443,
    0x0423,0x0436,0x0416,0x0432,0x0412,0x044c,0x042c,0x2116,
    0x00ad,0x044b,0x042b,0x0437,0x0417,0x0448,0x0428,0x044d,
    0x042d,0x0449,0x0429,0x0447,0x0427,0x00a7,0x25a0,0x00a0
};

static const uint16_t cp857Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x0131,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
    0x0130,0x00d6,0x00dc,0x00f8,0x00a3,0x00d8,0x015e,0x015f,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x011e,0x011f,
    0x00bf,0x00ae,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x00c1,0x00c2,0x00c0,
    0x00a9,0x2563,0x2551,0x2557,0x255d,0x00a2,0x00a5,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x00e3,0x00c3,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x00a4,
    0x00ba,0x00aa,0x00ca,0x00cb,0x00c8,0x0000,0x00cd,0x00ce,
    0x00cf,0x2518,0x250c,0x2588,0x2584,0x00a6,0x00cc,0x2580,
    0x00d3,0x00df,0x00d4,0x00d2,0x00f5,0x00d5,0x00b5,0x0000,
    0x00d7,0x00da,0x00db,0x00d9,0x00ec,0x00ff,0x00af,0x00b4,
    0x00ad,0x00b1,0x0000,0x00be,0x00b6,0x00a7,0x00f7,0x00b8,
    0x00b0,0x00a8,0x00b7,0x00b9,0x00b3,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp860Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e3,0x00e0,0x00c1,0x00e7,
    0x00ea,0x00ca,0x00e8,0x00cd,0x00d4,0x00ec,0x00c3,0x00c2,
    0x00c9,0x00c0,0x00c8,0x00f4,0x00f5,0x00f2,0x00da,0x00f9,
    0x00cc,0x00d5,0x00dc,0x00a2,0x00a3,0x00d9,0x20a7,0x00d3,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
    0x00bf,0x00d2,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x03b1,0x00df,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
    0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
    0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp861Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00d0,0x00f0,0x00de,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00fe,0x00fb,0x00dd,
    0x00fd,0x00d6,0x00dc,0x00f8,0x00a3,0x00d8,0x20a7,0x0192,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00c1,0x00cd,0x00d3,0x00da,
    0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x03b1,0x00df,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
    0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
    0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp862Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x05d0,0x05d1,0x05d2,0x05d3,0x05d4,0x05d5,0x05d6,0x05d7,
    0x05d8,0x05d9,0x05da,0x05db,0x05dc,0x05dd,0x05de,0x05df,
    0x05e0,0x05e1,0x05e2,0x05e3,0x05e4,0x05e5,0x05e6,0x05e7,
    0x05e8,0x05e9,0x05ea,0x00a2,0x00a3,0x00a5,0x20a7,0x0192,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
    0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x03b1,0x00df,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
    0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
    0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp863Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00c7,0x00fc,0x00e9,0x00e2,0x00c2,0x00e0,0x00b6,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x2017,0x00c0,0x00a7,
    0x00c9,0x00c8,0x00ca,0x00f4,0x00cb,0x00cf,0x00fb,0x00f9,
    0x00a4,0x00d4,0x00dc,0x00a2,0x00a3,0x00d9,0x00db,0x0192,
    0x00a6,0x00b4,0x00f3,0x00fa,0x00a8,0x00b8,0x00b3,0x00af,
    0x00ce,0x2310,0x00ac,0x00bd,0x00bc,0x00be,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x03b1,0x00df,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
    0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
    0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp864Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x066a,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00b0,0x00b7,0x2219,0x221a,0x2592,0x2500,0x2502,0x253c,
    0x2524,0x252c,0x251c,0x2534,0x2510,0x250c,0x2514,0x2518,
    0x03b2,0x221e,0x03c6,0x00b1,0x00bd,0x00bc,0x2248,0x00ab,
    0x00bb,0xfef7,0xfef8,0x0000,0x0000,0xfefb,0xfefc,0x0000,
    0x00a0,0x00ad,0xfe82,0x00a3,0x00a4,0xfe84,0x0000,0x0000,
    0xfe8e,0xfe8f,0xfe95,0xfe99,0x060c,0xfe9d,0xfea1,0xfea5,
    0x0660,0x0661,0x0662,0x0663,0x0664,0x0665,0x0666,0x0667,
    0x0668,0x0669,0xfed1,0x061b,0xfeb1,0xfeb5,0xfeb9,0x061f,
    0x00a2,0xfe80,0xfe81,0xfe83,0xfe85,0xfeca,0xfe8b,0xfe8d,
    0xfe91,0xfe93,0xfe97,0xfe9b,0xfe9f,0xfea3,0xfea7,0xfea9,
    0xfeab,0xfead,0xfeaf,0xfeb3,0xfeb7,0xfebb,0xfebf,0xfec1,
    0xfec5,0xfecb,0xfecf,0x00a6,0x00ac,0x00f7,0x00d7,0xfec9,
    0x0640,0xfed3,0xfed7,0xfedb,0xfedf,0xfee3,0xfee7,0xfeeb,
    0xfeed,0xfeef,0xfef3,0xfebd,0xfecc,0xfece,0xfecd,0xfee1,
    0xfe7d,0x0651,0xfee5,0xfee9,0xfeec,0xfef0,0xfef2,0xfed0,
    0xfed5,0xfef5,0xfef6,0xfedd,0xfed9,0xfef1,0x25a0,0x00a0
};

static const uint16_t cp865Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
    0x00ff,0x00d6,0x00dc,0x00f8,0x00a3,0x00d8,0x20a7,0x0192,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
    0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00a4,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x03b1,0x00df,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,
    0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
    0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,
    0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0
};

static const uint16_t cp866Map[256] = {
    0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
    0x0008,0x0009,0x000a,0x000b,0x000c,0x000d,0x000e,0x000f,
    0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
    0x0018,0x0019,0x001a,0x001b,0x001c,0x001d,0x001e,0x001f,
    0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
    0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
    0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
    0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
    0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
    0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
    0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
    0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
    0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
    0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
    0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
    0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x007f,
    0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,
    0x0418,0x0419,0x041a,0x041b,0x041c,0x041d,0x041e,0x041f,
    0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,
    0x0428,0x0429,0x042a,0x042b,0x042c,0x042d,0x042e,0x042f,
    0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,
    0x0438,0x0439,0x043a,0x043b,0x043c,0x043d,0x043e,0x043f,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,
    0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
    0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,
    0x0448,0x0449,0x044a,0x044b,0x044c,0x044d,0x044e,0x044f,
    0x0401,0x0451,0x0404,0x0454,0x0407,0x0457,0x040e,0x045e,
    0x00b0,0x2219,0x00b7,0x221a,0x2116,0x00a4,0x25a0,0x00a0
};


static const struct {
    uint16_t		code;
    const uint16_t	*map;
} maps[] = {
    { 437,	cp437Map	},
    { 737,	cp737Map	},
    { 775,	cp775Map	},
    { 850,	cp850Map	},
    { 852,	cp852Map	},
    { 855,	cp855Map	},
    { 857,	cp857Map	},
    { 860,	cp860Map	},
    { 861,	cp861Map	},
    { 862,	cp862Map	},
    { 863,	cp863Map	},
    { 864,	cp864Map	},
    { 865,	cp865Map	},
    { 866,	cp866Map	},
    { -1,	NULL		}
};


/* Select a ASCII->Unicode mapping by CP number */
void
select_codepage(uint16_t code, uint16_t *curmap)
{
    int i = 0;
    const uint16_t *map_to_use;

    map_to_use = maps[0].map;

    while (maps[i].code != 0) {
	if (maps[i].code == code) {
		map_to_use = maps[i].map;
		break;
	}
	i++;
    }
    
    for (i = 0; i < 256; i++)
	curmap[i] = map_to_use[i];
}
