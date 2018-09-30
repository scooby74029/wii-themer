/**
 * CleanRip - main.h
 * Copyright (C) 2010 emu_kidid
 *
 * CleanRip homepage: http://code.google.com/p/cleanrip/
 * email address: emukidid@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#ifndef MAIN_H
#define MAIN_H
#include <gccore.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <ogcsys.h>


#define TYPE_USB          2
#define TYPE_SD           1

// Version info
#define V_MAJOR			2
#define V_MID			  	0
#define V_MINOR			0

#define KNOWNSYSMENUAPPS  17

/* Constant  for file instal*/
#define BLOCK_SIZE	0x1000
#define CHUNKS 1000000

/* Macros */
#define round_up(x,n)	(-(-(x) & -(n)))

/*** 2D Video Globals ***/
extern GXRModeObj *vmode;	/*** Graphics Mode Object ***/
extern u32 *xfb[2];			/*** Framebuffers ***/
extern int whichfb;			/*** Frame buffer toggle ***/
extern unsigned int iosversion;

u32 get_buttons_pressed();
u32 Versionsys;


typedef struct _dirent{
	char name[ISFS_MAXPATH + 1];
	int type;
	u32 ownerID;
	u16 groupID;
	u8 attributes;
	u8 ownerperm;
	u8 groupperm;
	u8 otherperm;
} dirent_t;

typedef struct{
	char *name;
	u8 *region;
	u32 version;
	int type;
	u32 size;
}CurthemeStats;

typedef struct{
	char *name;
	u8 *region;
	u32 version;
	int type;
	u32 size;
}Installthemestats;



#endif

