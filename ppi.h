/*
 * Copyright 2000  Brian S. Dean <bsd@bsdhome.com>
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BRIAN S. DEAN ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL BRIAN S. DEAN BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 */

/* $Id$ */

#ifndef __ppi_h__
#define __ppi_h__

/*
 * PPI registers
 */
enum {
  PPIDATA,
  PPICTRL,
  PPISTATUS
};

int ppi_getops    (int reg, unsigned long * get, unsigned long * set);

int ppi_set       (int fd, int reg, int bit);

int ppi_clr       (int fd, int reg, int bit);

int ppi_get       (int fd, int reg, int bit);

int ppi_toggle    (int fd, int reg, int bit);

int ppi_getall    (int fd, int reg);

int ppi_setall    (int fd, int reg, int val);

int ppi_pulse     (int fd, int reg, int bit);

int ppi_setpin    (int fd, int pin, int value);

int ppi_getpin    (int fd, int pin);

int ppi_pulsepin  (int fd, int pin);

int ppi_getpinmask(int pin);

int ppi_getpinreg (int pin);

int ppi_sense     (int fd);

#endif


