/**
 * CleanRip - main.c
 * Copyright (C) 2010 emu_kidid
 *
 * Main driving code behind the disc ripper
 *
 * CleanRip homepage: http://code.google.com/p/cleanrip/
 * email address: emukidid@gmail.com
 *
 * ----------------------------------------------------------------------------------------------
 *
 * Wii - Themer - main.c
 * Copyright (C) 2013 scooby74029
 *
 * I would like to thank emu kidid for the great ground work he did without it i would not have
 * been able to make this program
 
 * Main driving code behind the theme installing tool
 *
 * Wii - Themer homepage: http://code.google.com/p/Wii-Themer/
 * email address: naythan.morey@gmail.com
 *
 *-----------------------------------------------------------------------------------------------
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/usbstorage.h>
#include <ogc/machine/processor.h>
#include <sdcard/wiisd_io.h>
#include <wiiuse/wpad.h>
#include <sys/dir.h>
#include <fat.h>
#include <dirent.h>
#include <network.h>
#include <ogcsys.h>
#include <sys/stat.h> //for mkdir

#include "FrameBufferMagic.h"
#include "IPLFontWrite.h"
#include "main.h"
#include "ios.h"
#include "gecko.h"
#include "network.h"
#include "rijndael.h"
#include "http.h"
#include "wpad.h"
#include "fat2.h"
#include "usbstorage.h"


/* global variables */ 
char mountPath[512];
static char wpadNeedScan = 0;
static char padNeedScan = 0;

int shutdown = 0;
int whichfb = 0;
unsigned int iosversion = 0;
int curios = 0;
int fail_install = 0;
dirent_t *ent = NULL;
dirent_t *nandfilelist;
int files = 0, ERROr = 0;
int Selected;
int type = 1;

CurthemeStats curthemestats;
Installthemestats installthemestats;
static char *SelectedTheme = NULL;

GXRModeObj *vmode = NULL;
u32 *xfb[2] = { NULL, NULL };

u8 CommonKey[16] = { 0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48,
    0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7 };

const DISC_INTERFACE* frontsd = &__io_wiisd;
const DISC_INTERFACE* usb = &__io_wiiums;

/* exception reloading */
void __exception_setreload(int);
/* controls */
u32 get_buttons_pressed() {
	WPADData *wiiPad;
	u32 buttons = 0;

	if (padNeedScan) {
		PAD_ScanPads();
		padNeedScan = 0;
	}
	if (wpadNeedScan) {
		WPAD_ScanPads();
		wpadNeedScan = 0;
	}

	u16 gcPad = PAD_ButtonsDown(0);
	wiiPad = WPAD_Data(0);

	if ((gcPad & PAD_BUTTON_B) || (wiiPad->btns_h & WPAD_BUTTON_B)) {
		buttons |= PAD_BUTTON_B;
	}

	if ((gcPad & PAD_BUTTON_A) || (wiiPad->btns_h & WPAD_BUTTON_A)) {
		buttons |= PAD_BUTTON_A;
	}

	if ((gcPad & PAD_BUTTON_LEFT) || (wiiPad->btns_h & WPAD_BUTTON_LEFT)) {
		buttons |= PAD_BUTTON_LEFT;
	}

	if ((gcPad & PAD_BUTTON_RIGHT) || (wiiPad->btns_h & WPAD_BUTTON_RIGHT)) {
		buttons |= PAD_BUTTON_RIGHT;
	}

	if ((gcPad & PAD_BUTTON_UP) || (wiiPad->btns_h & WPAD_BUTTON_UP)) {
		buttons |= PAD_BUTTON_UP;
	}

	if ((gcPad & PAD_BUTTON_DOWN) || (wiiPad->btns_h & WPAD_BUTTON_DOWN)) {
		buttons |= PAD_BUTTON_DOWN;
	}

	if ((gcPad & PAD_TRIGGER_Z) || (wiiPad->btns_h & WPAD_BUTTON_HOME)) {
		exit(0);
	}
	
	return buttons;
}

void wait_press_A() {
	// Draw the A button
	DrawAButton(265, 310);
	DrawFrameFinish();
	while ((get_buttons_pressed() & PAD_BUTTON_A))
		;
	while (!(get_buttons_pressed() & PAD_BUTTON_A))
		;
}

void wait_press_B(){
	DrawBButton(275, 310);
	DrawFrameFinish();
	while ((get_buttons_pressed() & (PAD_BUTTON_A | PAD_BUTTON_B)));
	while (1) {
		if (get_buttons_pressed() & PAD_BUTTON_B) {
			exit(0);
		}
	}
}
void wait_press_A_exit_B() {
	// Draw the A and B buttons
	DrawAButton(195, 310);
	DrawBButton(390, 310);
	DrawFrameFinish();
	while ((get_buttons_pressed() & (PAD_BUTTON_A | PAD_BUTTON_B)))
		;
	while (1) {
		while (!(get_buttons_pressed() & (PAD_BUTTON_A | PAD_BUTTON_B)))
			;
		if (get_buttons_pressed() & PAD_BUTTON_A) {
			break;
		} else if (get_buttons_pressed() & PAD_BUTTON_B) {
			exit(0);
		}
	}
}

static void InvalidatePADS() {
	padNeedScan = wpadNeedScan = 1;
}
/* start up the Wii */
static void Initialise() {
	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	PAD_Init();
	CONF_Init();
	WPAD_Init();
	WPAD_SetIdleTimeout(120);
	//WPAD_SetPowerButtonCallback((WPADShutdownCallback) ShutdownWii);
	//SYS_SetPowerCallback(ShutdownWii);

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	vmode = VIDEO_GetPreferredMode(NULL);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(vmode);

	// Allocate memory for the display in the uncached region
	xfb[0] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	xfb[1] = (u32 *) MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));
	VIDEO_ClearFrameBuffer(vmode, xfb[0], COLOR_BLACK);
	VIDEO_ClearFrameBuffer(vmode, xfb[1], COLOR_BLACK);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb[0]);

	VIDEO_SetPostRetraceCallback(InvalidatePADS);

	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	init_font();
	whichfb = 0;
	
}
/* show the disclaimer */
static void show_disclaimer() {
	DrawFrameStart();
	DrawEmptyBox(30, 150, vmode->fbWidth - 38, 350, COLOR_BLACK);
	WriteCentre(170, " Disclaimer : ");
	WriteCentre(200, "The author is not responsible for any");
	WriteCentre(225, " damages that could occur to your ");
	WriteCentre(250, "Wii by this program. Press Home button");
	WriteCentre(275, " anytime to Exit ... ");
	DrawFrameFinish();

	DrawFrameStart();
	DrawEmptyBox(30, 150, vmode->fbWidth - 38, 350, COLOR_BLACK);
	WriteCentre(170, " Disclaimer : ");
	WriteCentre(200, "The author is not responsible for any");
	WriteCentre(225, " damages that could occur to your ");
	WriteCentre(250, "Wii by this program. Press Home button");
	WriteCentre(275, " anytime to Exit ... ");
	WriteCentre(315, "Press  A to continue  B to Exit");
	sleep(5);
	wait_press_A_exit_B();
}

/* Initialise the sd or usb */
static int initialise_device(int type) {
	int ret = 0;
	gprintf("\n\nstarting device startup \n\n");
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	if (type == TYPE_SD) {
		WriteCentre(255, "Mounting SD ....");
	} else {
		WriteCentre(255, "Mounting USB ....");
	}
	DrawFrameFinish();
	sleep(3);
	
	ret = fatMountSimple("fat", type == TYPE_USB ? usb : frontsd);
	sprintf(mountPath, "fat:/");
	if (ret != 1) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		if(type == TYPE_SD)
			sprintf(txtbuffer, "Error Mounting SD ....[%i}", ret);
		else
			sprintf(txtbuffer, "Error Mounting USB ....[%i]", ret);
		WriteCentre(255, txtbuffer);
		WriteCentre(315, "Press  A to continue  B to Exit");
		wait_press_A_exit_B();
	}
	else{
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		if (type == TYPE_SD) {
		WriteCentre(255, "Mounting SD .... Complete ");
		} else {
		WriteCentre(255, "Mounting USB .... Complete ");
		}
		DrawFrameFinish();
		sleep(3);
	}
	
	return ret;
}

/*the user must specify the device type */
static int device_type() {

	u32 btns = 0;
	
	gprintf("\n\nIn devicetype now \n\n");
	
	while (btns != 8) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(215, "Please select the device type");
		DrawSelectableButton(130, 290, -1, 340, " SD ",
				(type == TYPE_SD) ? B_SELECTED : B_NOSELECT);
		DrawSelectableButton(430, 290, -1, 340, " USB ",
				(type == TYPE_USB) ? B_SELECTED : B_NOSELECT);
		DrawFrameFinish();
		while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
			| PAD_BUTTON_B | PAD_BUTTON_A)));
		btns = wpad_waitbuttons();
		if (btns == 256)
		{
			if(type == 1)
				type = 2;
			else
				type = 1;
		}
		if (btns == 512)
			if(type == 2)
				type = 1;
			else
				type = 2;
		if (btns == 8)
			break;
		if(btns == 128)
			exit(0);
	}
	
	return type;
}
/* print the theme list  -----------------------------------------------------------------------    */
static char *print_themelist() {

	int k = 0, selected = 1, ret = -1;
	
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	
	while(1) {
		gprintf("k = %d files =%d\n", k, files);
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(200, "Please select the Theme to Install ...");
		//sprintf(txtbuffer, ent[k].name);
		DrawSelectableButton(130, 280, -1, 340, ent[k].name,
			(selected >= 1) ? B_SELECTED : B_NOSELECT);
		DrawFrameFinish();
		
		u32 btns = wpad_waitbuttons();
		if (btns == 256){
			if(k >= files - 1)
				k = 0;
			else
				k += 1;
			
		}
		if (btns == 512){
			if(k <= 0)
				k = files - 1;
			else
				k -= 1;
		}
		
		if (btns == 8){
			selected ^= 0;
			break;
		}
		if(btns == 128)
			exit(0);
		if(btns == 4096)
		{
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			WriteCentre(200, "DownLoading Original .app File .......");
			DrawFrameFinish();
			sleep(3);
			ret = downloadapp();
			if(ret != 0)
			{
				DrawFrameStart();
				DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
				WriteCentre(200, "DownLoading Original .app File ....... Failed !");
				DrawFrameFinish();
				sleep(3);
			}
			else
			{
				DrawFrameStart();
				DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
				WriteCentre(200, "DownLoading Original .app File ....... Complete !");
				DrawFrameFinish();
				sleep(3);
			}
		}
	}
	Selected = k;
	return ent[k].name;
}

/* check for theme folder*/
static int themefoldercheck(int type) {

	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	
	sprintf(txtbuffer, "Checking for Theme folder ...");
	WriteCentre(255, txtbuffer);
	DrawFrameFinish();
	sleep(2);
	
	sprintf(mountPath, "fat:/themes");
	DIR *mydir;
	mydir = opendir(mountPath);
		
	if (!mydir) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		
		sprintf(txtbuffer, "Checking for Theme folder ... Failed");
		WriteCentre(250, txtbuffer);
		sprintf(txtbuffer, "Theme folder should be named 'themes' ...");
		WriteCentre(275, txtbuffer);
		sprintf(txtbuffer, "Press  B to exit ...");
		WriteCentre(315, txtbuffer);
		wait_press_B();
	}
	else{
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(txtbuffer, "Checking for Theme folder ... Complete");
		WriteCentre(255, txtbuffer);
		DrawFrameFinish();
	} 
	sleep(2);
	return 1;
}

/* retrieve the theme list */
s32 filelist_retrieve(int type){
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	
	sprintf(txtbuffer, "Retrieving Theme list  ...");
	WriteCentre(255, txtbuffer);
	DrawFrameFinish();
	sleep(2);
	
	sprintf(mountPath, "fat:/themes");
	
	DIR *mydir;
	mydir = opendir(mountPath);
    struct dirent *entry = NULL;
	
    while((entry = readdir(mydir))) // If we get EOF, the expression is 0 and
                                     // the loop stops. 
    {
		if(strncmp(entry->d_name, ".", 1) != 0 && strncmp(entry->d_name, "..", 2) != 0)
		files += 1;
    }
	closedir(mydir);
	ent = malloc(sizeof(dirent_t) * files);
	files = 0;
	mydir = opendir(mountPath);
	while((entry = readdir(mydir)))
	{
		gprintf("start next while \n");
		if(strncmp(entry->d_name, ".", 1) != 0 && strncmp(entry->d_name, "..", 2) != 0)
		{	
			strcpy(ent[files].name, entry->d_name);
			gprintf("ent[files].name %s \n",ent[files].name);
			files +=1;
		}
	}
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	
	sprintf(txtbuffer, "Retrieving Theme list ... Complete ");
	WriteCentre(255, txtbuffer);
	DrawFrameFinish();
	sleep(2);
	
	return 1;
	
}
/* check current theme version */
u32 GetSysMenuVersion() {
    //Get sysversion from TMD
    u64 TitleID = 0x0000000100000002LL;
    u32 tmd_size;
    s32 r = ES_GetTMDViewSize(TitleID, &tmd_size);
    if(r<0)
    {
        gprintf("error getting TMD views Size. error %d\n",r);
        return 0;
    }

    tmd_view *rTMD = (tmd_view*)memalign( 32, (tmd_size+31)&(~31) );
    if( rTMD == NULL )
    {
        gprintf("error making memory for tmd views\n");
        return 0;
    }
    memset(rTMD,0, (tmd_size+31)&(~31) );
    r = ES_GetTMDView(TitleID, (u8*)rTMD, tmd_size);
    if(r<0)
    {
        gprintf("error getting TMD views. error %d\n",r);
        free( rTMD );
        return 0;
    }
    u32 version = rTMD->title_version;
    if(rTMD)
    {
        free(rTMD);
    }
    return version;
}

/* retieve the current theme .app name */
char *appversion(u32 num) {
	switch(num){
		case 0:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)85;
			return "00000042.app";// usa
		}
		break;
		case 1:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)85;
			return "00000072.app";
		}
		break;
		case 2:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)85;
			return "0000007b.app";
		}
		break;
		case 3:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)85;
			return "00000087.app";
		}
		break;
		case 4:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)85;
			return "00000097.app";// usa
		}
		break;
		case 5:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)69;
			return "00000045.app";// pal
		}
		break;
		case 6:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)69;
			return "00000075.app";
		}
		break;
		case 7:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)69;
			return "0000007e.app";
		}
		break;
		case 8:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)69;
			return "0000008a.app";
		}
		break;
		case 9:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)69;
			return "0000009a.app";// pal
		}
		break;
		case 10:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)74;
			return "00000040.app";// jpn
		}
		break;
		case 11:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)74;
			return "00000070.app";
		}
		break;
		case 12:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)74;
			return "00000078.app";
		}
		break;
		case 13:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)74;
			return "00000084.app";
		}
		break;
		case 14:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)74;
			return "00000094.app";// jpn
		}
		break;
		case 15:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)75;
			return "0000008d.app";// kor
		}
		break;
		case 16:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)75;
			return "00000081.app";
		}
		break;
		case 17:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)75;
			return "0000009d.app";// kor
		}
		break;
		default: return "UNK";
		break;
	}
}

/* compare file names */
s32 __FileCmp(const void *a, const void *b){
	dirent_t *hdr1 = (dirent_t *)a;
	dirent_t *hdr2 = (dirent_t *)b;
	
	if (hdr1->type == hdr2->type){
		return strcmp(hdr1->name, hdr2->name);
	}else{
		return 0;
	}
}
/* read nand directory */
s32 getdir(char *path, dirent_t **entry, u32 *cnt){
	s32 res;
	u32 num = 0;

	int i, j, k;
	
	res = ISFS_ReadDir(path, NULL, &num);
	if(res != ISFS_OK){
		gprintf("Error: could not get dir entry count! (result: %d)\n", res);
		return -1;
	}

	char ebuf[ISFS_MAXPATH + 1];

	char *nbuf = malloc((ISFS_MAXPATH + 1) * num);
	if(nbuf == NULL){
		gprintf("ERROR: could not allocate buffer for name list!\n");
		return -2;
	}

	res = ISFS_ReadDir(path, nbuf, &num);
	DCFlushRange(nbuf,13*num); //quick fix for cache problems?
	if(res != ISFS_OK){
		gprintf("ERROR: could not get name list! (result: %d)\n", res);
		free(nbuf);
		return -3;
	}
	
	*cnt = num;
	
	*entry = malloc(sizeof(dirent_t) * num);
	if(*entry == NULL){
		gprintf("Error: could not allocate buffer\n");
		free(nbuf);
		return -4;
	}

	for(i = 0, k = 0; i < num; i++){	    
		for(j = 0; nbuf[k] != 0; j++, k++)
			ebuf[j] = nbuf[k];
		ebuf[j] = 0;
		k++;

		strcpy((*entry)[i].name, ebuf);
		gprintf("Name of file (%s)\n",(*entry)[i].name);
	}
	
	qsort(*entry, *cnt, sizeof(dirent_t), __FileCmp);
	
	free(nbuf);
	
	return 0;
}
/* retrieve current theme version/region */
int checknandapp() {
	gprintf("check nandapp():\n");
	switch(Versionsys) {
		case 288:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)74;
		}
		break;
		case 289:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)85;
		}
		break;
		case 290:{
			curthemestats.version = 32;
			curthemestats.region = (u8*)69;
		}
		break;
		case 416:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)74;
		}
		break;
		case 417:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)85;
		}
		break;
		case 418:{
			curthemestats.version = 40;
			curthemestats.region = (u8*)69;

		}
		break;
		case 448:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)74;
		}
		break;
		case 449:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)85;
		}
		break;
		case 450:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)69;
		}
		break;
		case 454:{
			curthemestats.version = 41;
			curthemestats.region = (u8*)75;
		}
		break;
		case 480:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)74;
		}
		break;
		case 481:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)85;
		}
		break;
		case 482:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)69;
		}
		break;
		case 486:{
			curthemestats.version = 42;
			curthemestats.region = (u8*)75;
		}
		break;
		case 512:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)74;
		}
		break;
		case 513:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)85;
		}
		break;
		case 514:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)69;
		}
		break;
		case 518:{
			curthemestats.version = 43;
			curthemestats.region = (u8*)75;
		}
		break;
		default:{
			s32 rtn;
			u32 nandfilesizetmp,j;
			char *CHECK;
			
			gprintf("versionsys(%d) \n",Versionsys);
			if(Versionsys > 518){
				rtn = getdir("/title/00000001/00000002/content",&nandfilelist,&nandfilesizetmp);
				gprintf("rtn(%d) \n",rtn);
				int k,done;
				for(k = 0;k <nandfilesizetmp ;){
					gprintf("name = %s \n",nandfilelist[k]);
					for(j = 0;j < KNOWNSYSMENUAPPS;){
						done = 0;
						CHECK = appversion(j);
						gprintf("check =%s \n",CHECK);
						if(strcmp(nandfilelist[k].name,CHECK) == 0){
							gprintf("equal \n");
							done = 1;
							break;
						}
						gprintf("no match \n");
						j++;
					}
					if(done == 1)
						break;
					k++;
				}
			}
			
			gprintf("cur .version(%d) .region(%c) \n",curthemestats.version,curthemestats.region);
			//free(buffer);
		}
		break;
	}
	
	return Versionsys;
}

/* retrieve size of install file */
u32 filesize(FILE *file) {
	u32 curpos, endpos;
	
	if(file == NULL)
		return 0;
	
	curpos = ftell(file);
	fseek(file, 0, 2);
	endpos = ftell(file);
	fseek(file, curpos, 0);
	
	return endpos;
}

/* install theme to nand */
s32 InstallFile(FILE * fp) {

	char *data;
	s32 ret, nandfile, ios = 2;
	int length = 0,numchunks, cursize, i;
	char filename[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	u32 newtmdsize ATTRIBUTE_ALIGN(32);
	u64 newtitleid ATTRIBUTE_ALIGN(32);
	signed_blob *newtmd;
	tmd_content *newtmdc, *newtmdcontent = NULL;
	int percent = 0, m;
	
	gprintf("install file \n");
	ISFS_Initialize();
	
	DrawFrameStart();
	DrawProgressBar(percent, "Installing Menu Files ... ");
	DrawFrameFinish();
	sleep(3);
	
	newtitleid = 0x0000000100000000LL + ios;
		
	ES_GetStoredTMDSize(newtitleid, &newtmdsize);
	newtmd = (signed_blob *) memalign(32, newtmdsize);
	memset(newtmd, 0, newtmdsize);
	ES_GetStoredTMD(newtitleid, newtmd, newtmdsize);
	newtmdc = TMD_CONTENTS((tmd *) SIGNATURE_PAYLOAD(newtmd));
		
	for(i = 0; i < ((tmd *) SIGNATURE_PAYLOAD(newtmd))->num_contents; i++){
		if(newtmdc[i].index == 1)
		{
			newtmdcontent = &newtmdc[i];
				
			if(newtmdc[i].type & 0x8000) { //Shared content! This is the hard part :P.
				return -1;
			}
			else  { //Not shared content, easy
				sprintf(filename, "/title/00000001/%08x/content/%08x.app", ios, newtmdcontent->cid);
			}
			break;
		}
		else if(i == (((tmd *) SIGNATURE_PAYLOAD(newtmd))->num_contents) - 1)
		{
			return -1;
		}
	}

	free(newtmd);
	
	nandfile = ISFS_Open(filename, ISFS_OPEN_RW);
	ISFS_Seek(nandfile, 0, SEEK_SET);
	
	length = filesize(fp);
	numchunks = length/CHUNKS + ((length % CHUNKS != 0) ? 1 : 0);
	
	DrawFrameStart();
	sprintf(txtbuffer, "Total parts: %d ... ", numchunks);
	DrawProgressBar(percent, txtbuffer);
	DrawFrameFinish();
	sleep(3);
	
	for(m = 0; m < numchunks; m++){
		data = memalign(32, CHUNKS);
		if(data == NULL){
			gprintf("[-] Error allocating memory!\n\n");
			return -1;
		}
		
		DrawFrameStart();
		sprintf(txtbuffer, "Installing part %d ... ", (m + 1));
		percent = (m*100)/numchunks;
		//percent = (float)m / (float)numchunks * 100.0;
		DrawProgressBar(percent, txtbuffer);
		DrawFrameFinish();
		sleep(3);
		gprintf("percent (%d) \n", percent);
		ret = fread(data, 1, CHUNKS, fp);
		if (ret < 0) {
			gprintf("[-] Error reading from SD! (ret = %d)\n\n", ret);
			return -1;
		}
		else{
			cursize = ret;
		}

		ret = ISFS_Write(nandfile, data, cursize);
		if(ret < 0){
			gprintf("[-] Error writing to NAND! (ret = %d)\n\n", ret);
			return ret;
		}
		free(data);
	}
	
	percent = 100;
	DrawFrameStart();
	sprintf(txtbuffer, "Installation Complete ... ");
	DrawProgressBar(percent, txtbuffer);
	DrawFrameFinish();
	sleep(3);
	
	ISFS_Close(nandfile);
	ISFS_Deinitialize();
	
	return 0;
}

/* retrieve current theme region */
int getcurrentregion() {
	int ret = 0;
	
	if(curthemestats.region == (u8*)69)
		ret = 1;
	else if(curthemestats.region == (u8*)74)
		ret = 2;
	else if(curthemestats.region == (u8*)75)
		ret = 3;
	else if(curthemestats.region == (u8*)85)
		ret = 4;
		
	return ret;
}

/* retrieve install theme region */
int getinstallregion() {
	int ret = 0;
	
	if(installthemestats.region == (u8*)69)
		ret = 1;
	else if(installthemestats.region == (u8*)74)
		ret = 2;
	else if(installthemestats.region == (u8*)75)
		ret = 3;
	else if(installthemestats.region == (u8*)85)
		ret = 4;
		
	return ret;
}

/* check theme attributes size/region/version */
void check_theme(int Selected) {
	
	int rtn = 0;
	char filepath[512];
	FILE *fp = NULL;
    u32 length,i;
    u8 *data;
	gprintf("selected theme %s \n", ent[Selected].name);
	sprintf(filepath,"fat:/themes/%s", ent[Selected].name); 
	
	fp = fopen(filepath, "rb");
    if (!fp){
		gprintf("[+] File Open Error not on SD!\n");
    }
	
    length = filesize(fp);
    data = malloc(length);
    memset(data,0,length);
    fread(data,1,length,fp);

    if(length <= 0)
    {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(txtbuffer, "Error opening file 0 bytes ....");
		WriteCentre(255, txtbuffer);
		WriteCentre(315, "Press  B to exit");
		wait_press_B();
        return;
    }
	else{
        for(i = 0; i < length;){
            if(data[i] == 83){
                if(data[i+6] == 52) { // 4 
                    if(data[i+8] == 48) { // 0
                            if(data[i+28] == 85) { // usa
								installthemestats.version = 40;
                                installthemestats.region = (u8*)85;
                                break;
                            }
                            else if(data[i+28] == 74) { //jap
                                installthemestats.version = 40;
                                installthemestats.region = (u8*)74;
                                break;
                            }
                            else if(data[i+28] == 69) { // pal
                                installthemestats.version = 40;
                                installthemestats.region = (u8*)69;
                                break;
                            }
                        }
                        else{
                            if(data[i+8] == 49) { // 4.1
                                if(data[i+31] == 85) { // usa
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)85;
                                    break;
                                }
                                else if(data[i+31] == 74) { //jap
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)74;
                                    break;
                                }
                                else if(data[i+31] == 69) { // pal
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)69;
                                    break;
                                }
                                else if(data[i+31] == 75) { // kor
                                    installthemestats.version = 41;
                                    installthemestats.region = (u8*)75;
                                    break;
                                }
                            }
                            else {
                                if(data[i+8] == 50) { // 4.2
                                    if(data[i+28] == 85) { // usa
                                        installthemestats.version = 42;
                                        installthemestats.region = (u8*)85;
                                        break;
                                    }
                                    else if(data[i+28] == 74) { // jap
                                        installthemestats.version = 42;
                                        installthemestats.region = (u8*)74;
                                        break;
                                    }
                                    else if(data[i+28] == 69) { // pal
                                        installthemestats.version = 42;
                                        installthemestats.region = (u8*)69;
                                        break;
                                    }
                                    else if(data[i+28] == 75) { // kor
                                        installthemestats.version = 42;
                                        installthemestats.region = (u8*)75;
                                        break;
                                    }
                                }
                                else {
                                    if(data[i+8] == 51) {// 4.3
                                        if(data[i+28] == 85) { // usa
                                            installthemestats.version = 43;
                                            installthemestats.region = (u8*)85;
                                            break;
                                        }
                                        else if(data[i+28] == 74) { //jap
                                            installthemestats.version = 42;
                                            installthemestats.region = (u8*)74;
                                            break;
                                        }
                                        else if(data[i+28] == 69) {// pal
                                            installthemestats.version = 43;
                                            installthemestats.region = (u8*)69;
                                            break;
                                        }
                                        else if(data[i+28] == 75) { // kor
                                            installthemestats.version = 43;
                                            installthemestats.region = (u8*)75;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                else {
                    if(data[i+6] == 51) {  // 3
                        if(data[i+8] == 50) { // 2
                            if(data[i+28] == 85) { // usa
                                installthemestats.version = 32;
                                installthemestats.region = (u8*)85;
                                break;
                            }
                            else {
                                if(data[i+28] == 69) { // pal
                                    installthemestats.version = 32;
                                    installthemestats.region = (u8*)69;
                                    break;
                                }
                                else {
                                    if(data[i+28] == 74) { // jap
                                        installthemestats.version = 32;
                                        installthemestats.region = (u8*)74;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            i++;
        }
    }   
	
	/* Theme Compatiblity Check */
	int current;
	current = getcurrentregion();
	int install;
	install = getinstallregion();
		
	gprintf("current(%d) install(%d) \n",current,install);
    gprintf("install theme .version(%d) .region(%c) \n",installthemestats.version,installthemestats.region);
    gprintf("cur theme .version(%d) .region(%c) \n",curthemestats.version,curthemestats.region);
		
    if(curthemestats.version != installthemestats.version) {
		fail_install = 1;
		ERROr = -1455;
		
    }
    else if(current != install) {
		fail_install = 1;
		ERROr = -1355;
    }
	
    free(data);
    fclose(fp);
    if(ERROr == 0) {
	    fp = fopen(filepath, "rb");
	    if (!fp) {
		   gprintf("[+] File Open Error !\n");
	    }
		
		/* Install */
	    rtn = InstallFile(fp);
		if(rtn != 0){
			fail_install = 1;
			ERROr = rtn;
		}
	    /* Close file */
	    if (fp)
		   fclose(fp);
	}
}

/* check for custom system menu version */
u32 checkcustom(u32 m) {
	s32 rtn;
	u32 nandfilesizetmp,j;
	
	char *CHECK;
			
	gprintf("versionsys(%d) \n",Versionsys);
	if(m > 518){
		rtn = getdir("/title/00000001/00000002/content",&nandfilelist,&nandfilesizetmp);
		gprintf("rtn(%d) \n",rtn);
		int k;
		for(k = 0;k < nandfilesizetmp ;){
			gprintf("name = %s \n",nandfilelist[k]);
			for(j = 0;j < KNOWNSYSMENUAPPS;){
				CHECK = appversion(j);
				gprintf("check =%s \n",CHECK);
				if(strcmp(nandfilelist[k].name,CHECK) == 0){
					gprintf("equal j = %d \n",j);
					gprintf("check = %s \n",CHECK);
					switch(j){
						case 0: return 289;
						break;
						case 1: return 417;
						break;
						case 2: return 449;
						break;
						case 3: return 481;
						break;
						case 4: return 513;
						break;
						case 5: return 290;
						break;
						case 6: return 418;
						break;
						case 7: return 450;
						break;
						case 8: return 482;
						break;
						case 9: return 514;
						break;
						case 10: return 288;
						break;
						case 11: return 416;
						break;
						case 12: return 448;
						break;
						case 13: return 480;
						break;
						case 14: return 512;
						break;
						case 15: return 454;
						break;
						case 16: return 486;
						break;
						case 17: return 518;
						break;
						
					}
				
				}
				gprintf("no match \n");
				j++;
			}
			k++;
		}
	}
	return 0;
}
/* for downloading .app file */
char *getappname(u32 Versionsys){
	switch(Versionsys){
		case 289: return "00000042";// usa
		break;
		case 417: return "00000072";
		break;
		case 449: return "0000007b";
		break;
		case 481: return "00000087";
		break;
		case 513: return "00000097";// usa
		break;
		case 290: return "00000045";// pal
		break;
		case 418: return "00000075";
		break;
		case 450: return "0000007e";
		break;
		case 482: return "0000008a";
		break;
		case 514: return "0000009a";// pal
		break;
		case 288: return "00000040";// jpn
		break;
		case 416: return "00000070";
		break;
		case 448: return "00000078";
		break;
		case 480: return "00000084";
		break;
		case 512: return "00000094";// jpn
		break;
		case 486: return "0000008d";// kor
		break;
		case 454: return "00000081";
		break;
		case 518: return "0000009d";// kor
		break;
		default: return "UNK";
		break;
	}
}
const char *getPath(int ind){
    switch(ind)
    {
    case 0:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/cetk";
        break;
    case 1:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/tmd.";
        break;
    case 2:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000003f";
        break;
    case 3:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000042";
        break;
    case 4:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000045";
        break;
    case 5:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000006f";
        break;
    case 6:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000072";
        break;
    case 7:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000075";
        break;
    case 8:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000078";
        break;
    case 9:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000007b";
        break;
    case 10:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000007e";
        break;
    case 11:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000081";
        break;
    case 12:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000084";
        break;
    case 13:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000087";
        break;
    case 14:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000008a";
        break;
    case 15:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000008d";
        break;
    case 16:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000094";
        break;
    case 17:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/00000097";
        break;
    case 18:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000009a";
        break;
    case 19:
        return "http://nus.cdn.shop.wii.com/ccs/download/0000000100000002/0000009d";
        break;
    default:
        return "ERROR";
        break;
    }
}
int getslot(int num){
    switch(num)
    {
    case 288:
        return 2;
        break;
    case 289:
        return 3;
        break;
    case 290:
        return 4;
        break;
    case 416:
        return 5;
        break;
    case 417:
        return 6;
        break;
    case 418:
        return 7;
        break;
    case 448:
        return 8;
        break;
    case 449:
        return 9;
        break;
    case 450:
        return 10;
        break;
    case 454:
        return 11;
        break;
    case 480:
        return 12;
        break;
    case 481:
        return 13;
        break;
    case 482:
        return 14;
        break;
    case 486:
        return 15;
        break;
    case 512:
        return 16;
        break;
    case 513:
        return 17;
        break;
    case 514:
        return 18;
        break;
    case 518:
        return 19;
        break;
    default:
        return -1;
        break;
    }
}


char *getsavename(u32 idx) {
    switch(idx)
    {
    case 289:
        return "42";// usa
        break;
    case 417:
        return "72";
        break;
    case 449:
        return "7b";
        break;
    case 481:
        return "87";
        break;
    case 513:
        return "97";// usa
        break;
    case 290:
        return "45";// pal
        break;
    case 418:
        return "75";
        break;
    case 450:
        return "7e";
        break;
    case 482:
        return "8a";
        break;
    case 514:
        return "9a";// pal
        break;
    case 288:
        return "40";// jpn
        break;
    case 416:
        return "70";
        break;
    case 448:
        return "78";
        break;
    case 480:
        return "84";
        break;
    case 512:
        return "94";// jpn
        break;
    case 486:
        return "8d";// kor
        break;
    case 454:
        return "81";
        break;
    case 518:
        return "9d";// kor
        break;
    default:
        return "UNKNOWN";
        break;
    }
}
/* decryption of .app files */
void get_title_key(signed_blob *s_tik, u8 *key) {
    static u8 iv[16] ATTRIBUTE_ALIGN(0x20);
    static u8 keyin[16] ATTRIBUTE_ALIGN(0x20);
    static u8 keyout[16] ATTRIBUTE_ALIGN(0x20);

    const tik *p_tik;
    p_tik = (tik*) SIGNATURE_PAYLOAD(s_tik);
    u8 *enc_key = (u8 *) &p_tik->cipher_title_key;
    memcpy(keyin, enc_key, sizeof keyin);
    memset(keyout, 0, sizeof keyout);
    memset(iv, 0, sizeof iv);
    memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);

    aes_set_key(CommonKey);
    aes_decrypt(iv, keyin, keyout, sizeof keyin);

    memcpy(key, keyout, sizeof keyout);
}
static void decrypt_buffer(u16 index, u8 *source, u8 *dest, u32 len) {
    static u8 iv[16];
    memset(iv, 0, 16);
    memcpy(iv, &index, 2);
    aes_decrypt(iv, source, dest, len);
}

/* download original .app file */
int downloadapp() {
	
	s32 rtn;
    int dvers;
    int ret;
    int counter;
    char *savepath = (char*)memalign(32,256);
    char *oldapp = (char*)memalign(32,256);
	
	dvers = GetSysMenuVersion();
    gprintf("dvers =%d \n",dvers);
    if(dvers > 518) { 
		dvers = checkcustom(dvers);
	}
	
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	
	sprintf(txtbuffer, "Downloading %s for ",getappname(dvers));
	WriteCentre(250, txtbuffer);
	sprintf(txtbuffer, "System Menu v%d ",dvers);
	WriteCentre(275, txtbuffer);
	DrawFrameFinish();
	sleep(3);
	
	gprintf("fatmakedir \n");
    Fat_MakeDir("fat:/tmp");

    signed_blob *s_tik = NULL;
    signed_blob *s_tmd = NULL;
	
    DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
    gprintf("Initializing  Network ....");
	sprintf(txtbuffer, "Initializing  Network .... ");
	WriteCentre(255, txtbuffer);
	DrawFrameFinish();
	sleep(2);
	
	 for(;;) {
        ret=net_init();
        if(ret<0 && ret!=-EAGAIN) {
            gprintf("Failed !!\n");
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Initializing  Network .... Failed");
			WriteCentre(255, txtbuffer);
			DrawFrameFinish();
			sleep(2);
        }
		if(ret==0) {
            gprintf("Complete !! \n\n");
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Initializing  Network .... Complete");
			WriteCentre(255, txtbuffer);
			DrawFrameFinish();
			sleep(2);
            break;
        }
    }
	
	for(counter = 0; counter < 3; counter++) {	
        char *path = (char*)memalign(32,256);
        int aa = getslot(dvers);


        if(counter == 0) {
            sprintf(path,"%s",getPath(counter));
            gprintf("Dowloading %s ....",path);
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Dowloading files from NUS ... ");
			WriteCentre(250, txtbuffer);
			DrawFrameFinish();
			sleep(2);
        }
        if(counter == 1) {
            sprintf(path,"%s%d",getPath(counter),dvers);
            gprintf("Dowloading %s ....",path);
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Dowloading files from NUS ... ");
			WriteCentre(250, txtbuffer);
			DrawFrameFinish();
			sleep(2);
        }
        if(counter == 2) {
            sprintf(path,"%s",getPath(aa));
            gprintf("Dowloading %s ....",path);
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Dowloading files from NUS ... ");
			WriteCentre(250, txtbuffer);
			DrawFrameFinish();
			sleep(2);
        }
        u32 outlen=0;
        u32 http_status=0;

        ret = http_request(path, 1<<31);
        if(ret == 0 ) {
            free(path);
            gprintf("download failed !! ret(%d)\n",ret);
            gprintf("Failed !! ret(%d)\n",ret);
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Dowloading files from NUS ... ");
			WriteCentre(250, txtbuffer);
			sprintf(txtbuffer, "Failed ");
			WriteCentre(275, txtbuffer);
			DrawFrameFinish();
			sleep(2);
        }
        else {
            gprintf("Complete !! \n\n");
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Dowloading files from NUS ... ");
			WriteCentre(250, txtbuffer);
			sprintf(txtbuffer, "Complete ");
			WriteCentre(275, txtbuffer);
			DrawFrameFinish();
			sleep(2);
		}
        free(path);

        u8* outbuf = (u8*)malloc(outlen);

        if(counter == 0) {
            ret = http_get_result(&http_status, (u8 **)&s_tik, &outlen);
        }
        if(counter == 1) {
            ret = http_get_result(&http_status, (u8 **)&s_tmd, &outlen);
        }
        if(counter == 2) {
            ret = http_get_result(&http_status, &outbuf, &outlen);
        }

        gprintf("ret(%d) get result \n",ret);
        gprintf("Decrypting files ....");
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(txtbuffer, "Decrypting files .... ");
		WriteCentre(255, txtbuffer);
		DrawFrameFinish();
		sleep(2);
		
        /*set aes key */
        u8 key[16];
        u16 index;
        get_title_key(s_tik, key);
        aes_set_key(key);

        u8* outbuf2 = (u8*)malloc(outlen);


        if(counter == 2) {
            if(outlen>0) { //suficientes bytes

                index = 01;

                //then decrypt buffer

                decrypt_buffer(index,outbuf,outbuf2,outlen);

                sprintf(savepath,"fat:/tmp/000000%s.app",getsavename(dvers));
                ret = Fat_SaveFile(savepath, (void *)&outbuf2,outlen);
            }
        }
        gprintf("Complete !! \n\n");
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(txtbuffer, "Decrypting files .... Complete ");
		WriteCentre(255, txtbuffer);
		DrawFrameFinish();
		sleep(2);
        if(outbuf!=NULL)
            free(outbuf);

        
    }
	net_deinit();
	
	sleep(2);
	
    sprintf(oldapp,"/title/00000001/00000002/%s.app",getappname(dvers));
	
	if(Fat_CheckFile(savepath))
		ISFS_Delete(oldapp);

    FILE *f = NULL;

    f = fopen(savepath,"rb");
    if(!f){
        gprintf("could not open %s \n",savepath);
    }
    gprintf("\nInstalling %s.app ....",getappname(dvers));
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	sprintf(txtbuffer, "Installing %s.app ....",getappname(dvers));
	WriteCentre(255, txtbuffer);
	DrawFrameFinish();
	sleep(2);
	
    /* Install */
    rtn = InstallFile(f);
	if(rtn < 0){
		if(f)
			fclose(f);
	}
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	sprintf(txtbuffer, "Theme installed ... Successfully ");
	WriteCentre(255, txtbuffer);
	DrawFrameFinish();
	sleep(2);
	
    /* Close file */
    if(f) {
        fclose(f);
	}
	int error;
	
	error = remove(savepath);
	
    if (error != 0) {
        gprintf("    ERROR!");
    } 
	else {
        gprintf("    Successfully deleted!");
    }
	
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	sprintf(txtbuffer, "Returning to the System Menu ...");
	WriteCentre(255, txtbuffer);
	DrawFrameFinish();
	sleep(4);
	
	/* Return to the Wii system menu */
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	
	return 0;
}
/*  Get List of installed IOS  */
s32 __u8Cmp(const void *a, const void *b)
{
    return *(u8 *)a-*(u8 *)b;
}
u8 *get_ioslist(u32 *cnt)
{
    u64 *buf = 0;
    s32 i, res;
    u32 tcnt = 0, icnt;
    u8 *ioses = NULL;

    //Get stored IOS versions.
    res = ES_GetNumTitles(&tcnt);
    if(res < 0)
    {
        printf("\nES_GetNumTitles: Error! (result = %d)\n", res);
        return 0;
    }
    buf = memalign(32, sizeof(u64) * tcnt);
    res = ES_GetTitles(buf, tcnt);
    if(res < 0)
    {
        printf("\nES_GetTitles: Error! (result = %d)\n", res);
        if (buf) free(buf);
        return 0;
    }

    icnt = 0;
    for(i = 0; i < tcnt; i++)
    {
        if(*((u32 *)(&(buf[i]))) == 1 && (u32)buf[i] > 2 && (u32)buf[i] < 0x100)
        {
            icnt++;
            ioses = (u8 *)realloc(ioses, sizeof(u8) * icnt);
            ioses[icnt - 1] = (u8)buf[i];
        }
    }

    ioses = (u8 *)malloc(sizeof(u8) * icnt);
    icnt = 0;

    for(i = 0; i < tcnt; i++)
    {
        if(*((u32 *)(&(buf[i]))) == 1 && (u32)buf[i] > 2 && (u32)buf[i] < 0x100)
        {
            icnt++;
            ioses[icnt - 1] = (u8)buf[i];
        }
    }
    free(buf);
    qsort(ioses, icnt, 1, __u8Cmp);

    *cnt = icnt;
    return ioses;
}

int ios_selectionmenu(int default_ios)
{
    u32 btns = 0; // = get_buttons_pressed();
    int selection = 0;
    u32 ioscount;
    u8 *list = get_ioslist(&ioscount);
    char The_Ios[5];
	
    int i;
    for (i=0; i<ioscount; i++)
    {
        // Default to default_ios if found, else the loaded IOS
        if (list[i] == default_ios)
        {
            selection = i;
            break;
        }
        if (list[i] == IOS_GetVersion())
        {
            selection = i;
        }
	   gprintf("\n\nlist[i] (%i) (%d) \n\n",list[i],i);
    }
	//while ((get_buttons_pressed() & PAD_BUTTON_A));
	
	while (btns != 8)
	{
		gprintf("\n\nStarting drawframestart() .....btns(%u) pad_a(%i) right(%i)\n\n",btns,PAD_BUTTON_A,PAD_BUTTON_RIGHT);
		DrawFrameStart();
		DrawEmptyBox(30, 130, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(The_Ios, "%i",list[selection]);
		WriteCentre(150, "Please Choose IOS ...");
		DrawSelectableButton(200, 250, -1, 340, The_Ios,
			(selection >= 1) ? B_SELECTED : B_NOSELECT);
		DrawFrameFinish();
		
		btns = wpad_waitbuttons();
		
        if (btns == 256)//|| buttons == PAD_BUTTON_LEFT)
        {
            if (selection > 0)
            {
                selection--;
            }
            else
            {
                selection = ioscount - 1;
            }
        }
        if (btns == 512)//|| buttons == PAD_BUTTON_RIGHT)
        {
            if (selection < ioscount - 1	)
            {
                selection++;
            }
            else
            {
                selection = 0;
            }
        }
		if (btns == 8) //|| buttons == PAD_BUTTON_A) break;
			break;
    }
    
    return list[selection];
}

/* main body of program */
int main(int argc, char **argv) {
	
	int ret;
	char msgbuf[1024];
	
	if(InitGecko()){
		USBGeckoOutput();
		gprintf("USBgecko output started ...\n");
	}
	
	__exception_setreload(5);
	gprintf("exception setreload 5 seconds ... \n");
	
	iosversion = IOS_GetVersion();
	gprintf("iosversion [%d] ... \n",iosversion);
	
	Initialise();
	gprintf("initialize() complete \n");
	
	 gprintf("show disclaimer ...");
	show_disclaimer();
	gprintf("finished \n");
	
	curios = ios_selectionmenu(249);
	gprintf("curios = %d \n\n", curios);
	
	Wpad_Disconnect();
	DrawFrameStart();
	DrawEmptyBox(30, 130, vmode->fbWidth - 38, 350, COLOR_BLACK);
	sprintf(msgbuf, "Reloading To IOS ... %i", curios);
	WriteCentre(215, msgbuf);
	DrawFrameFinish();
	sleep(2);
	ret = IOS_ReloadIOS(curios);
	gprintf("ret = %d \n\n", ret);
	if(ret == 0)
	{
		DrawFrameStart();
		DrawEmptyBox(30, 130, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(msgbuf, "Reloading To IOS ... %i Complete !", curios);
		WriteCentre(215, msgbuf);
		DrawFrameFinish();
	}
	else
	{
		DrawFrameStart();
		DrawEmptyBox(30, 130, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(msgbuf, "Reloading To IOS ... %i Failed !", curios);
		WriteCentre(215, msgbuf);
		DrawFrameFinish();
	}
	sleep(2);
	PAD_Init();
	WPAD_Init();
	WPAD_SetIdleTimeout(120);
	
	iosversion = curios;
	
	while (1) {
		ret = -1;
		gprintf("ret [%d] \n",ret);
		while (ret != 1) {
			gprintf("going into devtype\n\n");
			type = device_type();
			gprintf(" type [%d] ... \n",type);
			ret = initialise_device(type);
			gprintf("ret [%d] .. device ... \n",ret);
		} 
		gprintf("ret [%d] .. device ... \n",ret);
		
		ret = -1;
		ret = themefoldercheck(type);
		gprintf("ret [%d] .. themefolder check ... \n",ret);
		
		ret = filelist_retrieve(type);
		Versionsys = GetSysMenuVersion();
		gprintf("Versionsys [%d] \n", Versionsys);
		ret = checknandapp();
		gprintf("ret checknandapp() [%d] \n", ret);
		gprintf("cur .version(%d) .region(%c) \n",curthemestats.version,curthemestats.region);
		gprintf(" print themelist() \n");
		
		SelectedTheme = print_themelist();
		gprintf("SelectedTheme = %s \n", SelectedTheme);
		
		check_theme(Selected);
		gprintf("\n\n fail_install(%i) \n\n",fail_install);
		if(fail_install == 1) {
			gprintf("\n\n ERROr[%i] \n\n", ERROr);
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Theme Install ... Failed ! error[%i]",ERROr);
			WriteCentre(255, txtbuffer);
			DrawFrameFinish();
			sleep(3);
			if(ERROr == -1455) {
				DrawFrameStart();
				DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
				sprintf(txtbuffer, "Install Theme Version does not match ....");
				WriteCentre(200, txtbuffer);
				sprintf(txtbuffer, "current Theme Version . Exiting ....");
				WriteCentre(255, txtbuffer);
				sleep(3);
			}
			if(ERROr == -1355) {
				DrawFrameStart();
				DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
				sprintf(txtbuffer, "Install Theme Region does not match ....");
				WriteCentre(200, txtbuffer);
				sprintf(txtbuffer, "current Theme Region . Exiting ....");
				WriteCentre(255, txtbuffer);
				sleep(3);
			}
		}
		else {
			DrawFrameStart();
			DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
			sprintf(txtbuffer, "Theme Install ... Successful");
			WriteCentre(255, txtbuffer);
			DrawFrameFinish();
			sleep(3);
		}
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		sprintf(txtbuffer, "Returning to the System Menu ... ");
		WriteCentre(255, txtbuffer);
		DrawFrameFinish();
		sleep(3);
		/* Return to the Wii system menu */
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	}
	/* should never get here */
	
	return 0;
}
