/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 * Copyright (C) 2002-2004, 2006  Brian S. Dean <bsd@bsdhome.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* $Id$ */

#ifndef jtagmkII_h
#define jtagmkII_h

int  jtagmkII_send(PROGRAMMER * pgm, unsigned char * data, size_t len);
int  jtagmkII_recv(PROGRAMMER * pgm, unsigned char **msg);
int  jtagmkII_open(PROGRAMMER * pgm, char * port);
void jtagmkII_close(PROGRAMMER * pgm);
int  jtagmkII_getsync(PROGRAMMER * pgm, int mode);
int  jtagmkII_getparm(PROGRAMMER * pgm, unsigned char parm,
		      unsigned char * value);

void jtagmkII_initpgm (PROGRAMMER * pgm);

#endif

