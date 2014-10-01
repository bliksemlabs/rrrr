/*
 * Copyright (c) 2008-2014, Dave Benson and the protobuf-c authors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* From protobuf-c. Chopped down to only varint read/write functions. */
#include "intpack.h"
#include <string.h>	/* for strcmp, strlen, memcpy, memmove, memset */

#define TRUE  1
#define FALSE 0

uint32_t
zigzag32(int32_t v)
{
	if (v < 0)
		return ((uint32_t) (-v)) * 2 - 1;
	else
		return v * 2;
}


uint64_t
zigzag64(int64_t v)
{
	if (v < 0)
		return ((uint64_t) (-v)) * 2 - 1;
	else
		return v * 2;
}

size_t
uint32_pack(uint32_t value, uint8_t *out)
{
	unsigned rv = 0;

	if (value >= 0x80) {
		out[rv++] = value | 0x80;
		value >>= 7;
		if (value >= 0x80) {
			out[rv++] = value | 0x80;
			value >>= 7;
			if (value >= 0x80) {
				out[rv++] = value | 0x80;
				value >>= 7;
				if (value >= 0x80) {
					out[rv++] = value | 0x80;
					value >>= 7;
				}
			}
		}
	}
	/* assert: value<128 */
	out[rv++] = value;
	return rv;
}

size_t
sint32_pack(int32_t value, uint8_t *out)
{
	return uint32_pack(zigzag32(value), out);
}

size_t
uint64_pack(uint64_t value, uint8_t *out)
{
	uint32_t hi = (uint32_t) (value >> 32);
	uint32_t lo = (uint32_t) value;
	unsigned rv;

	if (hi == 0)
		return uint32_pack((uint32_t) lo, out);
	out[0] = (lo) | 0x80;
	out[1] = (lo >> 7) | 0x80;
	out[2] = (lo >> 14) | 0x80;
	out[3] = (lo >> 21) | 0x80;
	if (hi < 8) {
		out[4] = (hi << 4) | (lo >> 28);
		return 5;
	} else {
		out[4] = ((hi & 7) << 4) | (lo >> 28) | 0x80;
		hi >>= 3;
	}
	rv = 5;
	while (hi >= 128) {
		out[rv++] = hi | 0x80;
		hi >>= 7;
	}
	out[rv++] = hi;
	return rv;
}

size_t
sint64_pack(int64_t value, uint8_t *out)
{
	return uint64_pack(zigzag64(value), out);
}








