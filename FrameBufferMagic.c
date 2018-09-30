/**
 * CleanRip - FrameBufferMagic.c
 * Copyright (C) 2010 emu_kidid
 *
 * Framebuffer routines for drawing
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <ogc/exi.h>
#include "FrameBufferMagic.h"
#include "IPLFontWrite.h"
#include "abutton.bmp.h"
#include "bbutton.bmp.h"
#include "backdrop.bmp.h"
#include "main.h"

extern unsigned int iosversion;

// internal helper funcs
char *typeToStr(int type) {
	switch (type) {
	case D_WARN:
		return "(Warning)";
	case D_INFO:
		return "(Info)";
	case D_FAIL:
		return "(Error!)";
	case D_PASS:
		return "(Success)";
	}
	return "";
}

void drawBitmap(const unsigned long* image, int scrX, int scrY, int w, int h) {
	int i, j;
	int current = 0;

	for (j = 0 + scrY; j < h + scrY; j++) {
		if ((j < vmode->xfbHeight) && (j < vmode->efbHeight) && (j
				< vmode->viHeight)) {
			for (i = 0; i < w / 2; i++) {
				xfb[whichfb][(vmode->fbWidth / 2) * j + i + scrX]
						= image[current];
				current++;
			}
		}
	}
}

void drawBitmapTransparency(const unsigned long* image, int scrX, int scrY,
		int w, int h, int transparency) {
	int i, j;
	int current = 0;

	scrX >>= 1;

	for (j = 0 + scrY; j < h + scrY; j++) {
		if ((j < vmode->xfbHeight) && (j < vmode->efbHeight) && (j
				< vmode->viHeight)) {
			for (i = 0; i < w / 2; i++) {
				if (transparency != image[current]) {
					xfb[whichfb][(vmode->fbWidth / 2) * j + i + scrX]
							= image[current];
				}
				current++;
			}
		}
	}
}

void DrawAButton(int x, int y) {
	drawBitmapTransparency(abutton_Bitmap, x, y, 36, 36, COLOR_WHITE);
}

void DrawBButton(int x, int y) {
	drawBitmapTransparency(bbutton_Bitmap, x, y, 36, 36, COLOR_WHITE);
}

void _DrawBackdrop() {
	char iosStr[256];
	drawBitmap(backdrop_Bitmap, 0, 0, 640, 480);
	sprintf(iosStr, "IOS %i", iosversion);
	WriteFont(260, 95, iosStr);
	sprintf(iosStr, "v%i.%i.%i", V_MAJOR,V_MID,V_MINOR);
	WriteFont(300, 50, iosStr);
}

void _DrawHLine(int x1, int x2, int y, int color) {
	int i;
	y = (vmode->fbWidth / 2) * y;
	x1 >>= 1;
	x2 >>= 1;
	for (i = x1; i <= x2; i++)
		xfb[whichfb][y + i] = color;
}

void _DrawVLine(int x, int y1, int y2, int color) {
	int i;
	x >>= 1;
	for (i = y1; i <= y2; i++)
		xfb[whichfb][x + (vmode->fbWidth * i) / 2] = color;
}

void _DrawBox(int x1, int y1, int x2, int y2, int color) {
	_DrawHLine(x1, x2, y1, color);
	_DrawHLine(x1, x2, y2, color);
	_DrawVLine(x1, y1, y2, color);
	_DrawVLine(x2, y1, y2, color);
}

void _DrawBoxFilled(int x1, int y1, int x2, int y2, int color) {
	int h;
	for (h = y1; h <= y2; h++)
		_DrawHLine(x1, x2, h, color);
}

// Externally accessible functions

// Call this when starting a screen
void DrawFrameStart() {
	whichfb ^= 1;
	_DrawBackdrop();
}

// Call this at the end of a screen
void DrawFrameFinish() {
	VIDEO_SetNextFramebuffer(xfb[whichfb]);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
}

void DrawProgressBar(int percent, char *message) {
	int x1 = ((back_framewidth / 2) - (PROGRESS_BOX_WIDTH / 2));
	int x2 = ((back_framewidth / 2) + (PROGRESS_BOX_WIDTH / 2));
	int y1 = ((back_frameheight / 2) - (PROGRESS_BOX_HEIGHT / 2));
	int y2 = ((back_frameheight / 2) + (PROGRESS_BOX_HEIGHT / 2));
	int i = 0, middleY, borderSize = 10;

	middleY = (((y2 - y1) / 2) - 12) + y1;

	if (middleY + 24 > y2) {
		middleY = y1 + 3;
	}
	//Draw Text and backfill
	for (i = (y1 + borderSize); i < (y2 - borderSize); i++) {
		_DrawHLine(x1 + borderSize, x2 - borderSize, i, BUTTON_COLOUR_INNER); //inner
	}
	WriteCentre(middleY, message);
	sprintf(txtbuffer, "%d%% percent complete", percent);
	WriteCentre(middleY + 30, txtbuffer);
	//Draw Borders
	for (i = 0; i < borderSize; i++) {
		_DrawHLine(x1 + (i * 1), x2 - (i * 1), (y1 + borderSize) - i,
				BUTTON_COLOUR_OUTER); //top
	}
	for (i = 0; i < borderSize; i++) {
		_DrawHLine(x1 + (i * 1), x2 - (i * 1), (y2 - borderSize) + i,
				BUTTON_COLOUR_OUTER); //bottom
	}
	for (i = 0; i < borderSize; i++) {
		_DrawVLine((x1 + borderSize) - (i * 1), y1 + (i * 1), y2 - (i * 1),
				BUTTON_COLOUR_OUTER); //left
	}
	for (i = 0; i < borderSize; i++) {
		_DrawVLine((x2 - borderSize) + (i * 1), y1 + (i * 1), y2 - (i * 1),
				BUTTON_COLOUR_OUTER); //right
	}
	int multiplier = (PROGRESS_BOX_WIDTH - 20) / 100;
	int progressBarWidth = multiplier * 100;
	DrawEmptyBox((back_framewidth / 2 - progressBarWidth / 2), y1 + 25,
			((back_framewidth / 2 - progressBarWidth / 2) + ((multiplier
					* (100)))), y1 + 45, PROGRESS_BOX_BARALL); //Progress Bar backing
	DrawEmptyBox((back_framewidth / 2 - progressBarWidth / 2), y1 + 25,
			((back_framewidth / 2 - progressBarWidth / 2) + ((multiplier
					* (percent)))), y1 + 45, PROGRESS_BOX_BAR); //Progress Bar
}

void DrawMessageBox(char *l1, char *l2, char *l3, char *l4) {
	DrawFrameStart();
	DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
	if(l1) {
		WriteCentre(190, l1);
	}
	if(l2) {
		WriteCentre(255, l2);
	}
	if(l3) {
		WriteCentre(280, l3);
	}
	if(l4) {
		WriteCentre(315, l4);
	}
	DrawFrameFinish();
}

void DrawRawFont(int x, int y, char *message) {
	WriteFont(x, y, message);
}

void DrawSelectableButton(int x1, int y1, int x2, int y2, char *message,
		int mode) {
	int i = 0, middleY, borderSize;

	borderSize = (mode == B_SELECTED) ? 6 : 4;
	middleY = (((y2 - y1) / 2) - 12) + y1;

	//determine length of the text ourselves if x2 == -1
	x1 = (x2 == -1) ? x1 + 2 : x1;
	x2 = (x2 == -1) ? GetTextSizeInPixels(message) + x1 + (borderSize * 2) + 6
			: x2;

	if (middleY + 24 > y2) {
		middleY = y1 + 3;
	}
	//Draw Text and backfill (if selected)
	if (mode == B_SELECTED) {
		for (i = (y1 + borderSize); i < (y2 - borderSize); i++) {
			_DrawHLine(x1 + borderSize, x2 - borderSize, i, BUTTON_COLOUR_INNER); //inner
		}
		WriteFontHL(x1 + borderSize + 3, middleY, x2 - borderSize - 8, middleY
				+ 24, message, blit_lookup);
	} else {
		WriteFontHL(x1 + borderSize + 3, middleY, x2 - borderSize - 8, middleY
				+ 24, message, blit_lookup_norm);
	}
	//Draw Borders
	for (i = 0; i < borderSize; i++) {
		_DrawHLine(x1 + (i * 1), x2 - (i * 1), (y1 + borderSize) - i,
				BUTTON_COLOUR_OUTER); //top
	}
	for (i = 0; i < borderSize; i++) {
		_DrawHLine(x1 + (i * 1), x2 - (i * 1), (y2 - borderSize) + i,
				BUTTON_COLOUR_OUTER); //bottom
	}
	for (i = 0; i < borderSize; i++) {
		_DrawVLine((x1 + borderSize) - (i * 1), y1 + (i * 1), y2 - (i * 1),
				BUTTON_COLOUR_OUTER); //left
	}
	for (i = 0; i < borderSize; i++) {
		_DrawVLine((x2 - borderSize) + (i * 1), y1 + (i * 1), y2 - (i * 1),
				BUTTON_COLOUR_OUTER); //right
	}
}
void DrawEmptyBox(int x1, int y1, int x2, int y2, int color) {
	int i = 0, middleY, borderSize;
	borderSize = (y2 - y1) <= 30 ? 3 : 10;
	x1 -= borderSize;
	x2 += borderSize;
	y1 -= borderSize;
	y2 += borderSize;
	middleY = (((y2 - y1) / 2) - 12) + y1;

	if (middleY + 24 > y2) {
		middleY = y1 + 3;
	}
	//Draw Text and backfill
	for (i = (y1 + borderSize); i < (y2 - borderSize); i++) {
		_DrawHLine(x1 + borderSize, x2 - borderSize, i, color); //inner
	}
	//Draw Borders
	for (i = 0; i < borderSize; i++) {
		_DrawHLine(x1 + (i * 1), x2 - (i * 1), (y1 + borderSize) - i,
				BUTTON_COLOUR_OUTER); //top
	}
	for (i = 0; i < borderSize; i++) {
		_DrawHLine(x1 + (i * 1), x2 - (i * 1), (y2 - borderSize) + i,
				BUTTON_COLOUR_OUTER); //bottom
	}
	for (i = 0; i < borderSize; i++) {
		_DrawVLine((x1 + borderSize) - (i * 1), y1 + (i * 1), y2 - (i * 1),
				BUTTON_COLOUR_OUTER); //left
	}
	for (i = 0; i < borderSize; i++) {
		_DrawVLine((x2 - borderSize) + (i * 1), y1 + (i * 1), y2 - (i * 1),
				BUTTON_COLOUR_OUTER); //right
	}
}

int DrawYesNoDialog(char *line1, char *line2) {
	int selection = 0;
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	while (1) {
		DrawFrameStart();
		DrawEmptyBox(30, 180, vmode->fbWidth - 38, 350, COLOR_BLACK);
		WriteCentre(215, line1);
		WriteCentre(255, line2);
		DrawSelectableButton(150, 310, -1, 340, "Yes", (selection) ? B_SELECTED : B_NOSELECT);
		DrawSelectableButton(410, 310, -1, 340, "No", (!selection) ? B_SELECTED : B_NOSELECT);
		DrawFrameFinish();
		while (!(get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)));
		u32 btns = get_buttons_pressed();
		if (btns & PAD_BUTTON_RIGHT)
			selection ^= 1;
		if (btns & PAD_BUTTON_LEFT)
			selection ^= 1;
		if (btns & PAD_BUTTON_A)
			break;
		while ((get_buttons_pressed() & (PAD_BUTTON_RIGHT | PAD_BUTTON_LEFT
				| PAD_BUTTON_B | PAD_BUTTON_A)));
	}
	while ((get_buttons_pressed() & PAD_BUTTON_A));
	return selection;
}

