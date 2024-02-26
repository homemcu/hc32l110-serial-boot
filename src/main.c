/*
* Copyright (c) 2022, 2024 Vladimir Alemasov
* All rights reserved
*
* This program and the accompanying materials are distributed under
* the terms of GNU General Public License version 2
* as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <stdint.h>     /* uint8_t ... uint64_t */
#include <stdlib.h>     /* exit */
#include <stdio.h>      /* printf */
#include <string.h>     /* memcpy */
#include <time.h>       /* time */
#include <stdbool.h>    /* bool */
#include <errno.h>		/* errno */
#include <assert.h>     /* assert */
#ifdef _WIN32
#include <windows.h>    /* Windows stuff */
#include "getopt.h"
#include "gettimeofday.h"
#undef sleep
#define sleep(a) Sleep(a)
#else
#include <stddef.h>     /* offsetof */
#include <signal.h>     /* signal */
#include <sys/time.h>   /* gettimeofday */
#include <fcntl.h>      /* open */
#include <unistd.h>     /* usleep, getopt, write, close */
#define sleep(a) usleep((a) * 1000)
extern char *optarg;
#ifndef HANDLE
#define HANDLE int
#endif
#endif
#include "serial.h"

//--------------------------------------------
#define HC32L110_FLASH_SIZE              0x4000
#define READ_PACKET_MAX_DATA_SIZE        0x200
#define WRITE_PACKET_MAX_DATA_SIZE       0x200

//--------------------------------------------
static uint32_t flash_addr;
static uint16_t flash_size;
static FILE *file;

//--------------------------------------------
static uint8_t sum8(uint8_t *buf, size_t len)
{
	uint8_t sum = 0;
	for (size_t cnt = 0; cnt < len; cnt++)
	{
		sum += buf[cnt];
	}
	return sum;
}
#if 0
//--------------------------------------------
static uint8_t crc8(uint8_t *buf, size_t len)
{
	uint8_t crc = 0xff;
	size_t i, j;
	for (i = 0; i < len; i++)
	{
		crc ^= buf[i];
		for (j = 0; j < 8; j++)
		{
			if ((crc & 0x80) != 0)
			{
				crc = (uint8_t)((crc << 1) ^ 0x31);
			}
			else
			{
				crc <<= 1;
			}
		}
	}
	return crc;
}
#endif

//--------------------------------------------
static const uint8_t buf_connect[] = {
	0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff,
	0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff,
	0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff,
	0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff,
	0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff,
	0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff, 0x18, 0xff
};
static const uint8_t buf_ramcode[] = {
	0xb8, 0x0a, 0x00, 0x20, 0x09, 0x00, 0x00, 0x20, 0x72, 0xb6, 0x03, 0x48, 0x01, 0x68, 0x81, 0xf3,
	0x08, 0x88, 0x02, 0x48, 0x00, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x95, 0x07, 0x00, 0x20,
	0xc0, 0x68, 0x01, 0x68, 0x6e, 0x48, 0x01, 0x62, 0x6e, 0x4a, 0x89, 0x18, 0x6e, 0x4a, 0x91, 0x42,
	0x01, 0xd3, 0x06, 0x21, 0x81, 0x71, 0x70, 0x47, 0x80, 0xb5, 0x00, 0xf0, 0xe7, 0xf8, 0x6b, 0x48,
	0x01, 0x68, 0x03, 0x22, 0x0a, 0x43, 0x02, 0x60, 0x00, 0x21, 0x09, 0x60, 0x01, 0x68, 0xca, 0x06,
	0xd2, 0x0f, 0xfb, 0xd1, 0x01, 0xbd, 0x10, 0xb5, 0x04, 0x00, 0x60, 0x68, 0x80, 0x21, 0x09, 0x02,
	0x88, 0x42, 0x05, 0xd3, 0x62, 0x49, 0x40, 0x18, 0x80, 0x21, 0x89, 0x00, 0x88, 0x42, 0x10, 0xd2,
	0x00, 0xf0, 0xcc, 0xf8, 0x5d, 0x48, 0x01, 0x68, 0x03, 0x22, 0x91, 0x43, 0x02, 0x22, 0x0a, 0x43,
	0x02, 0x60, 0x00, 0x21, 0x62, 0x68, 0x11, 0x60, 0x01, 0x68, 0xca, 0x06, 0xd2, 0x0f, 0x03, 0xd0,
	0xfa, 0xe7, 0x05, 0x20, 0x52, 0x49, 0x88, 0x71, 0x10, 0xbd, 0x80, 0xb5, 0x01, 0x00, 0x4a, 0x69,
	0x48, 0x68, 0x80, 0x23, 0x1b, 0x02, 0x9a, 0x42, 0x05, 0xd3, 0x52, 0x4b, 0x98, 0x42, 0x07, 0xd3,
	0x51, 0x4b, 0x9a, 0x42, 0x04, 0xd2, 0x0a, 0x89, 0xc9, 0x68, 0x00, 0xf0, 0x01, 0xf9, 0x01, 0xbd,
	0x05, 0x20, 0x47, 0x49, 0x88, 0x71, 0x01, 0xbd, 0x80, 0xb5, 0x01, 0x89, 0x44, 0x4a, 0x91, 0x80,
	0x02, 0x89, 0xc1, 0x68, 0x40, 0x68, 0x00, 0xf0, 0x28, 0xf9, 0x01, 0xbd, 0x1c, 0xb5, 0x00, 0x21,
	0x6a, 0x46, 0x11, 0x80, 0xc1, 0x68, 0x09, 0x68, 0x40, 0x68, 0x3d, 0x4c, 0x42, 0x18, 0x52, 0x1e,
	0x80, 0x23, 0x1b, 0x02, 0x9a, 0x42, 0x02, 0xd3, 0x05, 0x20, 0xa0, 0x71, 0x02, 0xe0, 0x6a, 0x46,
	0x00, 0xf0, 0xbf, 0xf8, 0x3d, 0x48, 0x69, 0x46, 0x09, 0x88, 0x01, 0x72, 0x69, 0x46, 0x09, 0x88,
	0x09, 0x0a, 0x41, 0x72, 0x02, 0x20, 0xa0, 0x80, 0x13, 0xbd, 0x7c, 0xb5, 0x00, 0x22, 0x00, 0x92,
	0xc1, 0x68, 0x09, 0x68, 0x2e, 0x4e, 0x01, 0x25, 0xb5, 0x80, 0x34, 0x4c, 0x22, 0x72, 0x6a, 0x46,
	0x40, 0x68, 0x00, 0xf0, 0xb3, 0xf8, 0x00, 0x28, 0x02, 0xd0, 0x00, 0x98, 0x30, 0x60, 0x73, 0xbd,
	0x25, 0x72, 0x73, 0xbd, 0x01, 0x20, 0x26, 0x49, 0x88, 0x80, 0x2c, 0x49, 0x2c, 0x4a, 0x12, 0x78,
	0xff, 0x2a, 0x00, 0xd1, 0x00, 0x20, 0x08, 0x72, 0x70, 0x47, 0x80, 0xb5, 0xee, 0x20, 0x69, 0x46,
	0x08, 0x70, 0x01, 0x22, 0x26, 0x48, 0x00, 0xf0, 0xab, 0xf8, 0x01, 0x22, 0x69, 0x46, 0x25, 0x48,
	0x00, 0xf0, 0xa6, 0xf8, 0x01, 0xbd, 0x70, 0x47, 0x10, 0xb5, 0x19, 0x4c, 0x10, 0xe0, 0x02, 0x20,
	0xa0, 0x71, 0x20, 0x00, 0x00, 0xf0, 0x52, 0xf9, 0xa0, 0x88, 0x00, 0xf0, 0x6e, 0xf9, 0x60, 0x7a,
	0x01, 0x28, 0x05, 0xd1, 0xa0, 0x79, 0x00, 0x28, 0x02, 0xd1, 0x20, 0x6a, 0x00, 0xf0, 0x8a, 0xf9,
	0x00, 0xf0, 0x6b, 0xf9, 0x00, 0xf0, 0xee, 0xf8, 0x01, 0x28, 0xf9, 0xd1, 0x18, 0x21, 0x20, 0x00,
	0x08, 0x30, 0x00, 0xf0, 0xc5, 0xf9, 0x20, 0x00, 0x08, 0x30, 0x00, 0xf0, 0xfa, 0xf8, 0xa0, 0x71,
	0xe1, 0x68, 0x21, 0x60, 0x00, 0x21, 0xa1, 0x80, 0x00, 0x28, 0xda, 0xd1, 0x0e, 0x48, 0x61, 0x7a,
	0x89, 0x00, 0x41, 0x58, 0x00, 0x29, 0xd2, 0xd0, 0x20, 0x00, 0x08, 0x30, 0x88, 0x47, 0xd0, 0xe7,
	0x10, 0x0a, 0x00, 0x20, 0x80, 0xda, 0xff, 0xff, 0xc1, 0x1c, 0x0f, 0x00, 0x20, 0x00, 0x02, 0x40,
	0x00, 0xf6, 0xef, 0xff, 0x00, 0x0a, 0x10, 0x00, 0x00, 0x0c, 0x10, 0x00, 0x04, 0x08, 0x00, 0x20,
	0xfc, 0x0b, 0x10, 0x00, 0xf6, 0x0b, 0x10, 0x00, 0xd4, 0x06, 0x00, 0x20, 0x4d, 0x48, 0x4e, 0x49,
	0x01, 0x60, 0x4e, 0x49, 0x01, 0x60, 0x70, 0x47, 0x10, 0xb5, 0x4d, 0x49, 0x4a, 0x4a, 0xca, 0x62,
	0x4a, 0x4b, 0xcb, 0x62, 0x44, 0x01, 0x0c, 0x60, 0xca, 0x62, 0xcb, 0x62, 0x17, 0x24, 0x44, 0x43,
	0x4c, 0x60, 0xca, 0x62, 0xcb, 0x62, 0x1b, 0x24, 0x44, 0x43, 0x8c, 0x60, 0xca, 0x62, 0xcb, 0x62,
	0x44, 0x4c, 0x44, 0x43, 0xcc, 0x60, 0xca, 0x62, 0xcb, 0x62, 0x43, 0x4c, 0x44, 0x43, 0x0c, 0x61,
	0xca, 0x62, 0xcb, 0x62, 0x18, 0x24, 0x44, 0x43, 0x4c, 0x61, 0xca, 0x62, 0xcb, 0x62, 0xf0, 0x24,
	0x44, 0x43, 0x8c, 0x61, 0xca, 0x62, 0xcb, 0x62, 0xfa, 0x24, 0xa4, 0x00, 0x60, 0x43, 0xc8, 0x61,
	0xca, 0x62, 0xcb, 0x62, 0x00, 0x20, 0x08, 0x62, 0xca, 0x62, 0xcb, 0x62, 0x37, 0x48, 0x08, 0x63,
	0x10, 0xbd, 0x30, 0xb5, 0x00, 0x23, 0x00, 0x24, 0x03, 0xe0, 0x05, 0x78, 0x5b, 0x19, 0x40, 0x1c,
	0x64, 0x1c, 0x8c, 0x42, 0xf9, 0xd3, 0x13, 0x80, 0x00, 0x20, 0x30, 0xbd, 0x30, 0xb5, 0x03, 0x00,
	0x00, 0x24, 0x00, 0xe0, 0x64, 0x1c, 0x8c, 0x42, 0x08, 0xd2, 0x1d, 0x00, 0x6b, 0x1c, 0x2d, 0x78,
	0xff, 0x2d, 0xf7, 0xd0, 0x00, 0x19, 0x10, 0x60, 0x01, 0x20, 0x30, 0xbd, 0x00, 0x20, 0x30, 0xbd,
	0x70, 0xb4, 0x27, 0x4b, 0x20, 0x4c, 0xdc, 0x60, 0x20, 0x4c, 0xdc, 0x60, 0x01, 0x24, 0x1d, 0x68,
	0x03, 0x26, 0xb5, 0x43, 0x25, 0x43, 0x1d, 0x60, 0x00, 0x26, 0xb6, 0x18, 0xb6, 0x08, 0xb6, 0x00,
	0x95, 0x1b, 0x10, 0xd1, 0x85, 0x07, 0x0e, 0xd1, 0x15, 0x00, 0x18, 0xd0, 0x1d, 0x4d, 0x1e, 0x68,
	0x36, 0x09, 0x26, 0x40, 0xfb, 0xd1, 0x0e, 0x68, 0x06, 0x60, 0x09, 0x1d, 0x00, 0x1d, 0x52, 0x19,
	0x16, 0x04, 0xf4, 0xd1, 0x0b, 0xe0, 0x15, 0x00, 0x09, 0xd0, 0x1d, 0x68, 0x2d, 0x09, 0x25, 0x40,
	0xfb, 0xd1, 0x0d, 0x78, 0x05, 0x70, 0x49, 0x1c, 0x40, 0x1c, 0x52, 0x1e, 0xf5, 0xd1, 0x18, 0x68,
	0x00, 0x09, 0x20, 0x40, 0xfb, 0xd1, 0x70, 0xbc, 0x70, 0x47, 0x10, 0xb5, 0x00, 0x23, 0x04, 0xe0,
	0x04, 0x78, 0x0c, 0x70, 0x40, 0x1c, 0x49, 0x1c, 0x5b, 0x1c, 0x9c, 0xb2, 0x94, 0x42, 0xf7, 0xd3,
	0x00, 0x20, 0x10, 0xbd, 0x2c, 0x00, 0x02, 0x40, 0x5a, 0x5a, 0x00, 0x00, 0xa5, 0xa5, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x40, 0x50, 0x46, 0x00, 0x00, 0xe0, 0x22, 0x02, 0x00, 0xff, 0xff, 0x00, 0x00,
	0x20, 0x00, 0x02, 0x40, 0xfc, 0xff, 0x00, 0x00, 0x10, 0xb5, 0x0a, 0x00, 0x00, 0x21, 0x00, 0x23,
	0x03, 0xe0, 0x04, 0x78, 0x09, 0x19, 0x40, 0x1c, 0x5b, 0x1c, 0x9c, 0xb2, 0x94, 0x42, 0xf8, 0xd3,
	0xc8, 0xb2, 0x10, 0xbd, 0x48, 0x48, 0x01, 0x88, 0x09, 0x29, 0x0b, 0xdb, 0x47, 0x49, 0x4a, 0x78,
	0x05, 0x2a, 0x09, 0xd0, 0x8a, 0x79, 0xc9, 0x79, 0x09, 0x02, 0x11, 0x43, 0x09, 0x31, 0x00, 0x88,
	0x81, 0x42, 0x04, 0xd0, 0x00, 0x20, 0x70, 0x47, 0x00, 0x88, 0x09, 0x28, 0xfa, 0xd1, 0x01, 0x20,
	0x70, 0x47, 0x38, 0xb5, 0x04, 0x00, 0x00, 0x20, 0x00, 0x25, 0x3b, 0x4a, 0x11, 0x88, 0x10, 0x80,
	0x3a, 0x48, 0x02, 0x78, 0x22, 0x70, 0x42, 0x78, 0x62, 0x70, 0x82, 0x78, 0xc3, 0x78, 0x1b, 0x02,
	0x13, 0x43, 0x02, 0x79, 0x12, 0x04, 0x1a, 0x43, 0x43, 0x79, 0x1b, 0x06, 0x13, 0x43, 0x63, 0x60,
	0x83, 0x79, 0xc2, 0x79, 0x12, 0x02, 0x1a, 0x43, 0x22, 0x81, 0x63, 0x68, 0x9b, 0x18, 0x5b, 0x1e,
	0x63, 0x61, 0x23, 0x78, 0x49, 0x2b, 0x16, 0xd1, 0x63, 0x78, 0x0b, 0x2b, 0x01, 0xda, 0x00, 0x2b,
	0x01, 0xd1, 0x02, 0x25, 0x10, 0xe0, 0x00, 0x2a, 0x02, 0xd0, 0x02, 0x00, 0x08, 0x32, 0xe2, 0x60,
	0x42, 0x18, 0x52, 0x1e, 0x12, 0x78, 0x22, 0x74, 0x49, 0x1e, 0x89, 0xb2, 0xff, 0xf7, 0xa4, 0xff,
	0x21, 0x7c, 0x88, 0x42, 0x00, 0xd0, 0x01, 0x25, 0x28, 0x00, 0x32, 0xbd, 0x38, 0xb5, 0x04, 0x00,
	0x1e, 0x4d, 0xa0, 0x79, 0x68, 0x70, 0x20, 0x68, 0xa8, 0x70, 0x20, 0x68, 0x00, 0x0a, 0xe8, 0x70,
	0x20, 0x68, 0x00, 0x0c, 0x28, 0x71, 0x20, 0x68, 0x00, 0x0e, 0x68, 0x71, 0xa0, 0x88, 0xa8, 0x71,
	0xa0, 0x88, 0x00, 0x0a, 0xe8, 0x71, 0xa1, 0x88, 0x08, 0x31, 0x89, 0xb2, 0x28, 0x00, 0xff, 0xf7,
	0x83, 0xff, 0xa1, 0x88, 0x69, 0x18, 0x08, 0x72, 0x31, 0xbd, 0x80, 0xb5, 0x01, 0x00, 0x09, 0x31,
	0x89, 0xb2, 0x0e, 0x48, 0x00, 0xf0, 0x38, 0xf8, 0x01, 0xbd, 0x38, 0xb5, 0x00, 0xf0, 0x48, 0xf8,
	0x00, 0x23, 0x09, 0x49, 0x0a, 0x88, 0x00, 0x2a, 0x01, 0xd1, 0x49, 0x28, 0x0a, 0xd1, 0x07, 0x4a,
	0x0c, 0x88, 0x07, 0x4d, 0xac, 0x42, 0x04, 0xda, 0x0b, 0x88, 0x5c, 0x1c, 0x0c, 0x80, 0xd0, 0x54,
	0x31, 0xbd, 0x13, 0x70, 0x0b, 0x80, 0x31, 0xbd, 0x34, 0x0a, 0x00, 0x20, 0x04, 0x08, 0x00, 0x20,
	0x09, 0x02, 0x00, 0x00, 0x30, 0xb5, 0x00, 0x21, 0x00, 0x22, 0x09, 0x4b, 0xd4, 0xb2, 0xa5, 0x00,
	0x5d, 0x59, 0xa8, 0x42, 0x0a, 0xd0, 0x52, 0x1c, 0xd4, 0xb2, 0x0c, 0x2c, 0xf6, 0xdb, 0x00, 0xbf,
	0x15, 0xa0, 0x40, 0x5a, 0x03, 0x49, 0x08, 0x60, 0x48, 0x60, 0x30, 0xbd, 0x61, 0x00, 0xf6, 0xe7,
	0x74, 0x06, 0x00, 0x20, 0x00, 0x0c, 0x00, 0x40, 0x30, 0xb5, 0x00, 0x22, 0x80, 0x23, 0xdb, 0x05,
	0x0a, 0xe0, 0x04, 0x5d, 0x1c, 0x60, 0x1c, 0x69, 0xa5, 0x07, 0xed, 0x0f, 0xfb, 0xd0, 0x5c, 0x69,
	0x02, 0x25, 0xac, 0x43, 0x5c, 0x61, 0x52, 0x1c, 0x94, 0xb2, 0x8c, 0x42, 0xf1, 0xd3, 0x30, 0xbd,
	0x80, 0x20, 0xc0, 0x05, 0x01, 0x69, 0xc9, 0x07, 0xfc, 0xd5, 0x41, 0x69, 0x01, 0x22, 0x91, 0x43,
	0x41, 0x61, 0x00, 0x68, 0xc0, 0xb2, 0x70, 0x47, 0x70, 0xff, 0xa0, 0xff, 0xb8, 0xff, 0xdc, 0xff,
	0xe8, 0xff, 0xf4, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfa, 0xff, 0xfd, 0xff, 0xfe, 0xff,
	0x00, 0x22, 0x00, 0xbf, 0x09, 0x42, 0x02, 0xd0, 0x49, 0x1e, 0x42, 0x54, 0xfc, 0xd1, 0x70, 0x47,
	0xf8, 0xb5, 0x2d, 0x4c, 0x2d, 0x48, 0xa0, 0x60, 0x2d, 0x4d, 0xa5, 0x60, 0x20, 0x68, 0xe0, 0x21,
	0x49, 0x00, 0x01, 0x43, 0x21, 0x60, 0x2b, 0x48, 0x2b, 0x49, 0x82, 0x88, 0x0a, 0x40, 0xe2, 0x60,
	0x42, 0x88, 0x0a, 0x40, 0xe2, 0x60, 0x00, 0x88, 0x01, 0x40, 0xe1, 0x60, 0x23, 0x48, 0xa0, 0x60,
	0xa5, 0x60, 0x20, 0x68, 0x25, 0x49, 0x01, 0x40, 0x21, 0x60, 0x06, 0x20, 0xff, 0xf7, 0x44, 0xfe,
	0x00, 0x21, 0x23, 0x48, 0x01, 0x60, 0x03, 0x20, 0x22, 0x4a, 0x23, 0x4b, 0x23, 0x4e, 0x27, 0x6a,
	0xff, 0x07, 0x26, 0x62, 0x13, 0xd5, 0x19, 0x4e, 0xa6, 0x60, 0xa5, 0x60, 0x65, 0x68, 0x96, 0x0d,
	0x2e, 0x43, 0x66, 0x60, 0x05, 0x24, 0x1c, 0x60, 0x15, 0x68, 0x80, 0x26, 0x2e, 0x43, 0x16, 0x60,
	0xd1, 0x64, 0x9c, 0x62, 0x11, 0x6c, 0x02, 0x23, 0x99, 0x43, 0x11, 0x64, 0x0a, 0xe0, 0x98, 0x63,
	0x14, 0x6c, 0x20, 0x25, 0xac, 0x43, 0x14, 0x64, 0xd1, 0x64, 0xd8, 0x63, 0x11, 0x6c, 0x40, 0x23,
	0x0b, 0x43, 0x13, 0x64, 0x12, 0x49, 0x13, 0x4a, 0x0a, 0x60, 0x4a, 0x60, 0xc8, 0x60, 0x0c, 0x48,
	0x90, 0x21, 0x89, 0x00, 0x01, 0x60, 0x01, 0x68, 0x10, 0x22, 0x0a, 0x43, 0x02, 0x60, 0xff, 0xf7,
	0xbb, 0xfd, 0x00, 0x20, 0xf2, 0xbd, 0x00, 0xbf, 0x00, 0x20, 0x00, 0x40, 0x5a, 0x5a, 0x00, 0x00,
	0xa5, 0xa5, 0x00, 0x00, 0x02, 0x0c, 0x10, 0x00, 0xff, 0x07, 0x00, 0x00, 0x3f, 0xfe, 0xff, 0xff,
	0x04, 0x00, 0x00, 0x40, 0x80, 0x0d, 0x02, 0x40, 0x9c, 0x0c, 0x02, 0x40, 0x01, 0x01, 0x00, 0xf0,
	0x00, 0x0c, 0x00, 0x40, 0x70, 0xff, 0x00, 0x00, 0x70, 0xb4, 0x01, 0x23, 0x00, 0x24, 0x13, 0xe0,
	0x01, 0x68, 0x00, 0x1d, 0x19, 0x42, 0x02, 0xd0, 0x4d, 0x46, 0x6d, 0x1e, 0x49, 0x19, 0x0c, 0x60,
	0x09, 0x1d, 0x12, 0x1f, 0x04, 0x2a, 0xfa, 0xd2, 0x0d, 0x00, 0x96, 0x07, 0x01, 0xd5, 0x0c, 0x80,
	0xad, 0x1c, 0x1a, 0x40, 0x00, 0xd0, 0x2c, 0x70, 0x02, 0x68, 0x00, 0x1d, 0x00, 0x2a, 0xe7, 0xd1,
	0x70, 0xbc, 0x70, 0x47, 0x80, 0x25, 0x00, 0x00, 0x40, 0x38, 0x00, 0x00, 0x00, 0x4b, 0x00, 0x00,
	0x00, 0x96, 0x00, 0x00, 0x00, 0xe1, 0x00, 0x00, 0x00, 0xc2, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x84, 0x03, 0x00, 0x00, 0x08, 0x07, 0x00,
	0x00, 0x8c, 0x0a, 0x00, 0x30, 0xb4, 0x01, 0x22, 0x01, 0x68, 0x00, 0x1d, 0x00, 0x29, 0x0f, 0xd0,
	0x03, 0x68, 0xc3, 0x18, 0x44, 0x68, 0x08, 0x30, 0x14, 0x42, 0x02, 0xd0, 0x4d, 0x46, 0x6d, 0x1e,
	0x64, 0x19, 0x1d, 0x68, 0x25, 0x60, 0x1b, 0x1d, 0x24, 0x1d, 0x09, 0x1f, 0xec, 0xd0, 0xf8, 0xe7,
	0x30, 0xbc, 0x70, 0x47, 0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x20, 0x39, 0x00, 0x00, 0x20,
	0x57, 0x00, 0x00, 0x20, 0x9b, 0x00, 0x00, 0x20, 0xc9, 0x00, 0x00, 0x20, 0xdd, 0x00, 0x00, 0x20,
	0x1b, 0x01, 0x00, 0x20, 0x45, 0x01, 0x00, 0x20, 0x5b, 0x01, 0x00, 0x20, 0x77, 0x01, 0x00, 0x20,
	0x10, 0xb5, 0x07, 0x49, 0x79, 0x44, 0x18, 0x31, 0x06, 0x4c, 0x7c, 0x44, 0x16, 0x34, 0x04, 0xe0,
	0x08, 0x1d, 0x0a, 0x68, 0x89, 0x18, 0x88, 0x47, 0x01, 0x00, 0xa1, 0x42, 0xf8, 0xd1, 0x10, 0xbd,
	0x08, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x11, 0xff, 0xff, 0xff, 0x34, 0x02, 0x00, 0x00,
	0x04, 0x08, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x6d, 0xff, 0xff, 0xff, 0x04, 0x00, 0x00, 0x00,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x01, 0x20, 0xc0, 0x46,
	0x00, 0x28, 0x01, 0xd0, 0xff, 0xf7, 0xd4, 0xff, 0x00, 0xbf, 0x00, 0xbf, 0x00, 0x20, 0x00, 0xbf,
	0x00, 0xbf, 0xff, 0xf7, 0xf5, 0xfe, 0x00, 0xf0, 0x00, 0xf8, 0x80, 0xb5, 0x00, 0xf0, 0x02, 0xf8,
	0x01, 0xbd, 0x00, 0x00, 0x07, 0x46, 0x38, 0x46, 0x00, 0xf0, 0x02, 0xf8, 0xfb, 0xe7, 0x00, 0x00,
	0x80, 0xb5, 0x00, 0xbf, 0x00, 0xbf, 0x02, 0x4a, 0x11, 0x00, 0x18, 0x20, 0xab, 0xbe, 0xfb, 0xe7,
	0x26, 0x00, 0x02, 0x00, 0x00, 0xbf, 0x00, 0xbf, 0x00, 0xbf, 0x00, 0xbf, 0xff, 0xf7, 0xd6, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x9f
};
static const uint8_t buf_upload[] = {
	0x00, 0x00, 0x00, 0x00, 0x20, 0xa4, 0x07, 0x00, 0x00, 0xcb
};
static const uint8_t buf_execute[] = {
	0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0
};

//--------------------------------------------
static uint8_t buf_cmd[] = {
	0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static uint8_t buf_cmd_write[sizeof(buf_cmd) + 0x200] = {
	0x49, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


//--------------------------------------------
static int serial_read_connect_ack(HANDLE dev, size_t timeout_ms)
{
	struct timeval current_time;
	size_t start_ms;
	size_t check_ms;

	gettimeofday(&current_time, NULL);
	start_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;

	for (;;)
	{
		int res;
		uint8_t ack;

		res = serial_read(dev, &ack, 1);
		if (res > 0 && ack == 0x11)
		{
			return 0;
		}
		if (res < 0)
		{
			return -1;
		}

		gettimeofday(&current_time, NULL);
		check_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
		size_t stop_ms = check_ms - start_ms;
		if (stop_ms > timeout_ms)
		{
			return -1;
		}
	}
}

//--------------------------------------------
static int serial_read_success_ack(HANDLE dev, size_t timeout_ms)
{
	struct timeval current_time;
	size_t start_ms;
	size_t check_ms;

	gettimeofday(&current_time, NULL);
	start_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;

	for (;;)
	{
		volatile int res;
		uint8_t ack;

		res = serial_read(dev, &ack, 1);
		if (res > 0)
		{
			if (ack == 0x01)
			{
				return 0;
			}
			else
			{
				return -1;
			}
		}
		if (res < 0)
		{
			return -1;
		}

		gettimeofday(&current_time, NULL);
		check_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
		size_t stop_ms = check_ms - start_ms;
		if (stop_ms > timeout_ms)
		{
			return -1;
		}
		sleep(1);
	}
}

//--------------------------------------------
static int serial_read_execute_ack(HANDLE dev, size_t timeout_ms)
{
	struct timeval current_time;
	size_t start_ms;
	size_t check_ms;

	gettimeofday(&current_time, NULL);
	start_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;

	for (;;)
	{
		int res;
		uint8_t ack;
		static size_t ack_cnt;

		res = serial_read(dev, &ack, 1);
		if (res > 0)
		{
			ack_cnt++;
			if (ack_cnt == 11)
			{
				return 0;
			}
		}
		if (res < 0)
		{
			return -1;
		}

		gettimeofday(&current_time, NULL);
		check_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
		size_t stop_ms = check_ms - start_ms;
		if (stop_ms > timeout_ms)
		{
			return -1;
		}
		sleep(0);
	}
}

//--------------------------------------------
static int serial_read_cmd_read_resp(HANDLE dev, size_t timeout_ms, uint8_t *resp_buf)
{
	struct timeval current_time;
	size_t start_ms;
	size_t check_ms;
	size_t buf_cnt = 0;
	uint16_t flash_size_pkt = 0;

	gettimeofday(&current_time, NULL);
	start_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;

	for (;;)
	{
		int res;

		res = serial_read(dev, &resp_buf[buf_cnt], 1);
		if (res > 0)
		{
			switch (buf_cnt)
			{
			case 0:
				if (resp_buf[0] != 0x49)
				{
					return -1;
				}
				break;
			case 1:
				if (resp_buf[1] != 0)
				{
					return -1;
				}
				break;
			case 7:
				flash_size_pkt = (uint16_t)resp_buf[7] << 8 | resp_buf[6];
				if (flash_size_pkt > READ_PACKET_MAX_DATA_SIZE)
				{
					return -1;
				}
				break;
			}
			if (flash_size_pkt > 0 && buf_cnt == 8 + flash_size_pkt)
			{
				if (sum8(resp_buf, 8 + flash_size_pkt) != resp_buf[8 + flash_size_pkt])
				{
					return -1;
				}
				return 0;
			}
			buf_cnt++;
		}
		if (res < 0)
		{
			return -1;
		}

		gettimeofday(&current_time, NULL);
		check_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
		size_t stop_ms = check_ms - start_ms;
		if (stop_ms > timeout_ms)
		{
			return -1;
		}
	}
}

//--------------------------------------------
static int serial_read_cmd_resp(HANDLE dev, size_t timeout_ms, uint8_t *resp_buf)
{
	struct timeval current_time;
	size_t start_ms;
	size_t check_ms;
	size_t buf_cnt = 0;

	gettimeofday(&current_time, NULL);
	start_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;

	for (;;)
	{
		int res;

		res = serial_read(dev, &resp_buf[buf_cnt], 1);
		if (res > 0)
		{
			switch (buf_cnt)
			{
			case 0:
				if (resp_buf[0] != 0x49)
				{
					return -1;
				}
				break;
			case 1:
				if (resp_buf[1] != 0)
				{
					return -1;
				}
				break;
			case 8:
				if (sum8(resp_buf, 8) != resp_buf[8])
				{
					return -1;
				}
				return 0;
			}
			buf_cnt++;
		}
		if (res < 0)
		{
			return -1;
		}

		gettimeofday(&current_time, NULL);
		check_ms = current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
		size_t stop_ms = check_ms - start_ms;
		if (stop_ms > timeout_ms)
		{
			return -1;
		}
	}
}

//--------------------------------------------
static void print_usage(void)
{
	printf("Usage:\n");
	printf("  hc32l10-serial-boot -p <serport> [-b] [-l <length>] [-f <file>]\n\n");
	printf("Mandatory arguments for input:\n");
	printf("  -p <serport>       serial port name\n");
	printf("Command arguments for input:\n");
	printf("  -b                 simply switches HC32L110 into serial bootloader mode, then you can use the original HDSC ISP\n");
	printf("  -r <file>          read flash memory to file\n");
	printf("  -w <file>          write flash memory from file\n");
	printf("  -e                 erase flash memory\n");
	printf("Command-specific input arguments:\n");
	printf("  -a <address>       data address in hexadecimal notation\n");
	printf("  -s <size>          data size in hexadecimal notation\n");
	printf("\nExamples:\n");
#ifdef _WIN32
	printf("  hc32l10-serial-boot -pCOM9 -b\n");
	printf("  hc32l10-serial-boot -pCOM9 -rflash.bin\n");
	printf("  hc32l10-serial-boot -pCOM9 -rflash.bin -a0x1000 -s0x100\n");
	printf("  hc32l10-serial-boot -pCOM9 -wflash.bin\n");
	printf("  hc32l10-serial-boot -pCOM9 -wflash.bin -a0x1000\n");
	printf("  hc32l10-serial-boot -pCOM9 -e\n");
	printf("  hc32l10-serial-boot -pCOM9 -e -a0x1000\n");
#else
	printf("  hc32l10-serial-boot -p/dev/ttyUSB0 -b\n");
	printf("  hc32l10-serial-boot -p/dev/ttyUSB0 -rflash.bin\n");
	printf("  hc32l10-serial-boot -p/dev/ttyUSB0 -rflash.bin -a0x1000 -s0x100\n");
	printf("  hc32l10-serial-boot -p/dev/ttyUSB0 -wflash.bin\n");
	printf("  hc32l10-serial-boot -p/dev/ttyUSB0 -wflash.bin -a0x1000\n");
	printf("  hc32l10-serial-boot -p/dev/ttyUSB0 -e\n");
	printf("  hc32l10-serial-boot -p/dev/ttyUSB0 -e -a0x1000\n");
#endif
}

//--------------------------------------------
typedef struct options
{
	int opt_p;
	int opt_b;
	int opt_e;
	int opt_r;
	int opt_w;
	int opt_a;
	int opt_s;
	char *opt_p_arg;
	char *opt_r_arg;
	char *opt_w_arg;
	char *opt_a_arg;
	char *opt_s_arg;
} options_t;

//--------------------------------------------
#define OPTIONS_CHECK_SUCCESS                     0
#define OPTIONS_CHECK_ERROR_USAGE                -1
#define OPTIONS_CHECK_ERROR_INCORRECT_ADDR       -2
#define OPTIONS_CHECK_ERROR_INCORRECT_SIZE       -3
#define OPTIONS_CHECK_ERROR_OPEN_FILE            -4
#define OPTIONS_CHECK_ERROR_EMPTY_FILE           -5
#define OPTIONS_CHECK_ERROR_TOO_BIG_FILE         -6

//--------------------------------------------
static int options_check(options_t *ts)
{
	// input options
	if (!ts->opt_p)
	{
		printf("The -p option is required.\n\n");
		print_usage();
		return OPTIONS_CHECK_ERROR_USAGE;
	}
	if (ts->opt_b)
	{
		if (ts->opt_e)
		{
			printf("Warning: The -e option is ignored with the -b option.\n\n");
		}
		if (ts->opt_r)
		{
			printf("Warning: The -r option is ignored with the -b option.\n\n");
		}
		if (ts->opt_w)
		{
			printf("Warning: The -w option is ignored with the -b option.\n\n");
		}
		if (ts->opt_a)
		{
			printf("Warning: The -a option is ignored with the -b option.\n\n");
		}
		if (ts->opt_s)
		{
			printf("Warning: The -s option is ignored with the -b option.\n\n");
		}
	}
	else
	{
		if ((ts->opt_e && ts->opt_r) || (ts->opt_e && ts->opt_w) || (ts->opt_r && ts->opt_w))
		{
			printf("Invalid options, you can not do several operations at the same time.\n\n");
			print_usage();
			return OPTIONS_CHECK_ERROR_USAGE;
		}
		if (ts->opt_r)
		{
			if ((ts->opt_a && !ts->opt_s) || (!ts->opt_a && ts->opt_s))
			{
				printf("Invalid options, both address and size must be specified to read the flash memory portion.\n\n");
				print_usage();
				return OPTIONS_CHECK_ERROR_USAGE;
			}
			if (ts->opt_a && ts->opt_s)
			{
				long value;
				char *endptr;

				errno = 0;
				value = strtol(ts->opt_a_arg, &endptr, 16);
				if (errno || *endptr != '\0' || value < 0 || value >= HC32L110_FLASH_SIZE)
				{
					printf("The -a option is wrong.\n\n");
					print_usage();
					return OPTIONS_CHECK_ERROR_INCORRECT_ADDR;
				}
				flash_addr = (uint32_t)value;

				errno = 0;
				value = strtol(ts->opt_s_arg, &endptr, 16);
				if (errno || *endptr != '\0' || value <= 0 || (flash_addr + value) > HC32L110_FLASH_SIZE)
				{
					printf("The -s option is wrong.\n\n");
					print_usage();
					return OPTIONS_CHECK_ERROR_INCORRECT_SIZE;
				}
				flash_size = (uint16_t)value;
			}
			else
			{
				flash_addr = 0;
				flash_size = HC32L110_FLASH_SIZE;
			}
			if ((file = fopen(ts->opt_r_arg, "wb")) == NULL)
			{
				printf("FATAL ERROR: Could not open file %s.\n", ts->opt_r_arg);
				return OPTIONS_CHECK_ERROR_OPEN_FILE;
			}
			printf("File %s is opened.\n", ts->opt_r_arg);
		}
		if (ts->opt_e)
		{
			if (ts->opt_s)
			{
				printf("Warning: The -s option is ignored with the -e option.\n\n");
			}
			if (ts->opt_a)
			{
				long value;
				char *endptr;

				errno = 0;
				value = strtol(ts->opt_a_arg, &endptr, 16);
				if (errno || *endptr != '\0' || value < 0 || value >= HC32L110_FLASH_SIZE)
				{
					printf("The -a option is wrong.\n\n");
					print_usage();
					return OPTIONS_CHECK_ERROR_INCORRECT_ADDR;
				}
				flash_addr = (uint32_t)value;
			}
			else
			{
				flash_addr = 0;
			}
		}
		if (ts->opt_w)
		{
			if (ts->opt_s)
			{
				printf("Warning: The -s option is ignored with the -w option.\n\n");
			}
			if (ts->opt_a)
			{
				long value;
				char *endptr;

				errno = 0;
				value = strtol(ts->opt_a_arg, &endptr, 16);
				if (errno || *endptr != '\0' || value < 0 || value >= HC32L110_FLASH_SIZE)
				{
					printf("The -a option is wrong.\n\n");
					print_usage();
					return OPTIONS_CHECK_ERROR_INCORRECT_ADDR;
				}
				flash_addr = (uint32_t)value;
			}
			else
			{
				flash_addr = 0;
			}
			if ((file = fopen(ts->opt_w_arg, "rb")) == NULL)
			{
				printf("FATAL ERROR: Could not open file %s.\n", ts->opt_w_arg);
				return OPTIONS_CHECK_ERROR_OPEN_FILE;
			}
			printf("File %s is opened.\n", ts->opt_w_arg);
			fseek(file, 0L, SEEK_END);
			long length = ftell(file);
			fseek(file, 0L, SEEK_SET);
			if (!length)
			{
				printf("File %s is empty.\n", ts->opt_w_arg);
				fclose(file);
				printf("File %s is closed.\n", ts->opt_w_arg);
				return OPTIONS_CHECK_ERROR_EMPTY_FILE;
			}
			if (flash_addr + length > HC32L110_FLASH_SIZE)
			{
				printf("File %s is longer than microcontroller flash size.\n", ts->opt_w_arg);
				fclose(file);
				printf("File %s is closed.\n", ts->opt_w_arg);
				return OPTIONS_CHECK_ERROR_TOO_BIG_FILE;
			}
			flash_size = (uint16_t)length;
		}
		if (!ts->opt_r && !ts->opt_e && !ts->opt_w)
		{
			ts->opt_b = 1;
		}
	}
	return OPTIONS_CHECK_SUCCESS;
}

//--------------------------------------------
int main(int argc, char *argv[])
{
	int option;
	options_t ts = { 0 };
	port_settings_t set = { 9600, 0 };
	static HANDLE rx_uart;

	while ((option = getopt(argc, argv, "p:br:ew:a:s:")) != -1)
	{
		switch (option)
		{
		case 'p':
			ts.opt_p = 1;
			ts.opt_p_arg = optarg;
			break;
		case 'b':
			ts.opt_b = 1;
			break;
		case 'r':
			ts.opt_r = 1;
			ts.opt_r_arg = optarg;
			break;
		case 'e':
			ts.opt_e = 1;
			break;
		case 'w':
			ts.opt_w = 1;
			ts.opt_w_arg = optarg;
			break;
		case 'a':
			ts.opt_a = 1;
			ts.opt_a_arg = optarg;
			break;
		case 's':
			ts.opt_s = 1;
			ts.opt_s_arg = optarg;
			break;
		default: // '?'
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

	if (options_check(&ts) < 0)
	{
		exit(EXIT_FAILURE);
	}

	if (serial_open(ts.opt_p_arg, &set, &rx_uart) < 0)
	{
		printf("ERROR: Could not open serial port. Not found or not accessible.\n");
		if (file)
		{
			fclose(file);
		}
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("%s", "Connection to serial port established.\n");
	}

	serial_flush(rx_uart);
	serial_set_rts(rx_uart);
	printf("Please wait. The HL32L110 is powered off for 5 second.\n");
	sleep(5000);
	serial_write(rx_uart, buf_connect, sizeof(buf_connect));
	serial_clr_rts(rx_uart);
	if (!serial_read_connect_ack(rx_uart, 20))
	{
		printf("Successfully connected to HL32L110.\n");
		sleep(200);
		serial_flush(rx_uart);
	}
	else
	{
		printf("ERROR: Could not connect to HL32L110.\n");
		goto cleanup;
	}

	if (ts.opt_b)
	{
		// just establish the connection with HL32L110
		printf("Disconnect the wire from the RTS pin of the USB2UART dongle and then run HDSC MCU programmer software.\n");
		goto cleanup;
	}

	// other options: load the flashloader firmware into the RAM
	serial_write(rx_uart, buf_upload, sizeof(buf_upload));
	if (serial_read_success_ack(rx_uart, 2000))
	{
		printf("ERROR: Connection error.\n");
		goto cleanup;
	}
	sleep(5);
	serial_write(rx_uart, buf_ramcode, sizeof(buf_ramcode));
	if (serial_read_success_ack(rx_uart, 5000))
	{
		printf("ERROR: Connection error.\n");
		goto cleanup;
	}
	sleep(5);
	serial_write(rx_uart, buf_execute, sizeof(buf_execute));
	if (serial_read_execute_ack(rx_uart, 2000))
	{
		printf("ERROR: Connection error.\n");
		goto cleanup;
	}
	printf("The flashloader firmware has been successfully loaded into the RAM.\n");
	sleep(10);

	if (ts.opt_r)
	{
		uint16_t flash_size_inc;
		uint32_t flash_addr_inc;

		printf("Read Flash memory to %s.\n", ts.opt_r_arg);
		for (flash_size_inc = 0, flash_addr_inc = flash_addr; flash_size_inc < flash_size; flash_addr_inc = flash_addr + flash_size_inc)
		{
			uint16_t flash_size_pkt = (flash_size - flash_size_inc > READ_PACKET_MAX_DATA_SIZE) ? READ_PACKET_MAX_DATA_SIZE : flash_size - flash_size_inc;
			buf_cmd[1] = 5;
			buf_cmd[2] = flash_addr_inc;
			buf_cmd[3] = flash_addr_inc >> 8;
			buf_cmd[4] = flash_addr_inc >> 16;
			buf_cmd[5] = flash_addr_inc >> 24;
			buf_cmd[6] = (uint8_t)flash_size_pkt;
			buf_cmd[7] = (uint8_t)(flash_size_pkt >> 8);
			buf_cmd[8] = sum8(buf_cmd, sizeof(buf_cmd) - 1);
			sleep(1);
			serial_write(rx_uart, buf_cmd, sizeof(buf_cmd));
			uint8_t resp_buf[9 + READ_PACKET_MAX_DATA_SIZE] = { 0 };
			if (serial_read_cmd_read_resp(rx_uart, 1000, resp_buf))
			{
				printf("ERROR: Connection error.\n");
				goto cleanup;
			}
			flash_size_inc += flash_size_pkt;
			fwrite(resp_buf + 8, flash_size_pkt, 1, file);
			fflush(file);
		}
		printf("Operation completed successfully.\n");
	}
	if (ts.opt_w)
	{
		uint16_t flash_size_inc;
		uint32_t flash_addr_inc;

		printf("Write Flash memory from %s.\n", ts.opt_w_arg);
		for (flash_size_inc = 0, flash_addr_inc = flash_addr; flash_size_inc < flash_size; flash_addr_inc = flash_addr + flash_size_inc)
		{
			uint16_t flash_size_pkt = (flash_size - flash_size_inc > WRITE_PACKET_MAX_DATA_SIZE) ? WRITE_PACKET_MAX_DATA_SIZE : flash_size - flash_size_inc;
			buf_cmd_write[2] = flash_addr_inc;
			buf_cmd_write[3] = flash_addr_inc >> 8;
			buf_cmd_write[4] = flash_addr_inc >> 16;
			buf_cmd_write[5] = flash_addr_inc >> 24;
			buf_cmd_write[6] = (uint8_t)flash_size_pkt;
			buf_cmd_write[7] = (uint8_t)(flash_size_pkt >> 8);
			fread(buf_cmd_write + 8, flash_size_pkt, 1, file);
			buf_cmd_write[8 + flash_size_pkt] = sum8(buf_cmd_write, 8 + flash_size_pkt);
			sleep(1);
			serial_write(rx_uart, buf_cmd_write, 8 + flash_size_pkt + 1);
			uint8_t resp_buf[9] = { 0 };
			if (serial_read_cmd_resp(rx_uart, 1000, resp_buf))
			{
				printf("ERROR: Connection error.\n");
				goto cleanup;
			}
			flash_size_inc += flash_size_pkt;
		}
	}
	if (ts.opt_e)
	{
		printf("Erase Flash memory.\n");
		if (flash_addr == 0)
		{
			// Chip erase
			buf_cmd[1] = 2;
		}
		else
		{
			// Sector erase
			buf_cmd[1] = 3;
			buf_cmd[2] = flash_addr;
			buf_cmd[3] = flash_addr >> 8;
			buf_cmd[4] = flash_addr >> 16;
			buf_cmd[5] = flash_addr >> 24;
		}
		buf_cmd[8] = sum8(buf_cmd, sizeof(buf_cmd) - 1);
		serial_write(rx_uart, buf_cmd, sizeof(buf_cmd));
		uint8_t resp_buf[9] = { 0 };
		if (serial_read_cmd_resp(rx_uart, 1000, resp_buf))
		{
			printf("ERROR: Connection error.\n");
			goto cleanup;
		}
	}

cleanup:
	serial_close(rx_uart);
	printf("Connection to the serial port closed.\n");

	if (file)
	{
		fclose(file);
	}

#if 0
	printf("Press the Enter key to exit.\n");
	getchar();
#endif

	exit(EXIT_SUCCESS);
}
