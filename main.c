/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 * Copyright (C) 2000, 2001, 2002, 2003  Brian S. Dean <bsd@bsdhome.com>
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

/*
 * Code to program an Atmel AVR AT90S device using the parallel port.
 *
 * Pin definitions can be changed via a config file.  Below is the
 * default pin configuration in the absence of a config definition
 * which lists "default" as one of its ids.
 *
 *  Parallel Port      Programmer Function
 *  -------------      -----------------------------
 *       Pins 2-5  ->  Vcc (see note below)
 *       Pin    7  ->  AVR /RESET
 *       Pin    8  ->  AVR SCK  (clock input)
 *       Pin    9  ->  AVR MOSI (instruction in)
 *       Pin   10  <-  AVR MISO (data out)
 *       Pin   18      Signal Ground
 *
 * Additionally, the following connections can be made to enable
 * additional features, however, to enable these features use the
 * pin configuration id "alf" ("-c alf" on the command line):
 *
 *  Parallel Port      Programmer Function
 *  -------------      -----------------------------
 *       Pin    1      STATUS LED, active low (program or verify error)
 *       Pin    6  ->  /ENABLE ('367 bus driver)
 *       Pin   14      STATUS LED, active low (ready)
 *       Pin   16      STATUS LED, active low (programming)
 *       Pin   17      STATUS LED, active low (verifying)
 *
 *  Pin 6 can be tied to the enable line of a 74HC367 in order to
 *  isolate and buffer the data to and from the PC parallel port.
 *  This is useful for connecting to a device in-circuit, and keeps
 *  the state of the parallel port pins from interfering with the
 *  normal operation of the target system.  When programming is
 *  complete, this pin is driven high, causing to pins of the '367 to
 *  float.
 *
 *  NOTE on Vcc connection: make sure your parallel port can supply an
 *  adequate amount of current to power your device.  6-10 mA per pin
 *  is common for parallel port signal lines, but is not guaranteed,
 *  especially for notebook computers.  For additional power, use
 *  multiple pins tied together with Schottky diodes.  If in doubt,
 *  don't risk damaging your parallel port, use an external power
 *  supply.
 * 
 */

#include "ac_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#include "avr.h"
#include "config.h"
#include "fileio.h"
#include "par.h"
#include "pindefs.h"
#include "ppi.h"
#include "term.h"


#define DEFAULT_PARALLEL "/dev/ppi0"
#define DEFAULT_SERIAL   "/dev/cuaa0"

char * version      = "3.1.0";

int    verbose;     /* verbose output */
char * progname;
char   progbuf[PATH_MAX]; /* temporary buffer of spaces the same
                             length as progname; used for lining up
                             multiline messages */

PROGRAMMER * pgm = NULL;

PROGRAMMER * compiled_in_pgm;

/*
 * global options
 */
int do_cycles;   /* track erase-rewrite cycles */


/*
 * usage message
 */
void usage(void)
{
  fprintf(stderr,
          "\nUsage: %s -p partno [-e] [-E exitspec[,exitspec]] [-f format] "
          "[-F]\n"
          "      %s[-i filename] [-m memtype] [-o filename] [-P port] "
          "[-t]\n"
          "      %s[-c programmer] [-C config-file] [-v [-v]] [-n]\n\n",
          progname, progbuf, progbuf);

}


/*
 * parse the -E string
 */
int getexitspecs(char *s, int *set, int *clr)
{
  char *cp;

  while ((cp = strtok(s, ","))) {
    if (strcmp(cp, "reset") == 0) {
      *clr |= par_getpinmask(pgm->pinno[PIN_AVR_RESET]);
    }
    else if (strcmp(cp, "noreset") == 0) {
      *set |= par_getpinmask(pgm->pinno[PIN_AVR_RESET]);
    }
    else if (strcmp(cp, "vcc") == 0) { 
      if (pgm->pinno[PPI_AVR_VCC])
        *set |= pgm->pinno[PPI_AVR_VCC];
    }
    else if (strcmp(cp, "novcc") == 0) {
      if (pgm->pinno[PPI_AVR_VCC])
        *clr |= pgm->pinno[PPI_AVR_VCC];
    }
    else {
      return -1;
    }
    s = 0; /* strtok() should be called with the actual string only once */
  }

  return 0;
}



int read_config(char * file)
{
  FILE * f;

  f = fopen(file, "r");
  if (f == NULL) {
    fprintf(stderr, "%s: can't open config file \"%s\": %s\n",
            progname, file, strerror(errno));
    return -1;
  }

  infile = file;
  yyin   = f;

  yyparse();

  fclose(f);

  return 0;
}




void pinconfig_display(char * p)
{
  fprintf(stderr, "%sProgrammer Type: %s\n", p, pgm->type);

  pgm->display(pgm, p);
}



void verify_pin_assigned(int pin, char * desc)
{
  if (pgm->pinno[pin] == 0) {
    fprintf(stderr, "%s: error: no pin has been assigned for %s\n",
            progname, desc);
    exit(1);
  }
}



PROGRAMMER * locate_pinconfig(LISTID programmers, char * configid)
{
  LNODEID ln1, ln2;
  PROGRAMMER * p = NULL;
  char * id;
  int found;

  found = 0;

  for (ln1=lfirst(programmers); ln1 && !found; ln1=lnext(ln1)) {
    p = ldata(ln1);
    for (ln2=lfirst(p->id); ln2 && !found; ln2=lnext(ln2)) {
      id = ldata(ln2);
      if (strcasecmp(configid, id) == 0)
        found = 1;
    }  
  }

  if (found)
    return p;

  return NULL;
}


AVRPART * locate_part(LISTID parts, char * partdesc)
{
  LNODEID ln1;
  AVRPART * p = NULL;
  int found;

  found = 0;

  for (ln1=lfirst(parts); ln1 && !found; ln1=lnext(ln1)) {
    p = ldata(ln1);
    if ((strcasecmp(partdesc, p->id) == 0) ||
        (strcasecmp(partdesc, p->desc) == 0))
      found = 1;
  }

  if (found)
    return p;

  return NULL;
}


void list_parts(FILE * f, char * prefix, LISTID parts)
{
  LNODEID ln1;
  AVRPART * p;

  for (ln1=lfirst(parts); ln1; ln1=lnext(ln1)) {
    p = ldata(ln1);
    fprintf(f, "%s%-4s = %s\n", prefix, p->id, p->desc);
  }

  return;
}



/*
 * main routine
 */
int main(int argc, char * argv [])
{
  int              rc;          /* general return code checking */
  int              exitrc;      /* exit code for main() */
  int              i;           /* general loop counter */
  int              ch;          /* options flag */
  int              size;        /* size of memory region */
  int              len;         /* length for various strings */
  struct avrpart * p;           /* which avr part we are programming */
  struct avrpart * v;           /* used for verify */
  int              readorwrite; /* true if a chip read/write op was selected */
  int              ppidata;	/* cached value of the ppi data register */
  int              vsize=-1;    /* number of bytes to verify */
  AVRMEM         * sig;         /* signature data */

  /* options / operating mode variables */
  char *  memtype;     /* "flash", "eeprom", etc */
  int     doread;      /* 1=reading AVR, 0=writing AVR */
  int     erase;       /* 1=erase chip, 0=don't */
  char  * outputf;     /* output file name */
  char  * inputf;      /* input file name */
  int     ovsigck;     /* 1=override sig check, 0=don't */
  char  * port;        /* device port (/dev/xxx) */
  int     terminal;    /* 1=enter terminal mode, 0=don't */
  FILEFMT filefmt;     /* FMT_AUTO, FMT_IHEX, FMT_SREC, FMT_RBIN */
  int     nowrite;     /* don't actually write anything to the chip */
  int     verify;      /* perform a verify operation */
  int     ppisetbits;  /* bits to set in ppi data register at exit */
  int     ppiclrbits;  /* bits to clear in ppi data register at exit */
  char  * exitspecs;   /* exit specs string from command line */
  char  * pinconfig;   /* programmer id */
  char  * partdesc;    /* part id */
  char    configfile[PATH_MAX]; /* pin configuration file */
  int     cycles;      /* erase-rewrite cycles */
  int     set_cycles;  /* value to set the erase-rewrite cycles to */
  char  * e;           /* for strtol() error checking */

  progname = rindex(argv[0],'/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  init_config();

  partdesc      = NULL;
  readorwrite   = 0;
  port          = DEFAULT_PARALLEL;
  outputf       = NULL;
  inputf        = NULL;
  doread        = 1;
  memtype       = "flash";
  erase         = 0;
  p             = NULL;
  ovsigck       = 0;
  terminal      = 0;
  filefmt       = FMT_AUTO;
  nowrite       = 0;
  verify        = 1;        /* on by default */
  ppisetbits    = 0;
  ppiclrbits    = 0;
  exitspecs     = NULL;
  pgm           = NULL;
  pinconfig     = "avrdude"; /* compiled-in default */
  verbose       = 0;
  do_cycles     = 0;
  set_cycles    = -1;

  strcpy(configfile, CONFIG_DIR);
  i = strlen(configfile);
  if (i && (configfile[i-1] != '/'))
    strcat(configfile, "/");
  strcat(configfile, "avrdude.conf");

  /*
   * initialize compiled-in default programmer 
   */
  compiled_in_pgm = pgm_new();
  pgm = compiled_in_pgm;
  par_initpgm(pgm);
  ladd(pgm->id, dup_string("avrdude"));
  strcpy(pgm->desc, "avrdude compiled-in default");
  pgm->pinno[PPI_AVR_VCC]   = 0x0f;  /* ppi pins 2-5, data reg bits 0-3 */
  pgm->pinno[PPI_AVR_BUFF]  =  0;
  pgm->pinno[PIN_AVR_RESET] =  7;
  pgm->pinno[PIN_AVR_SCK]   =  8;
  pgm->pinno[PIN_AVR_MOSI]  =  9;
  pgm->pinno[PIN_AVR_MISO]  = 10;
  pgm->pinno[PIN_LED_ERR]   =  0;
  pgm->pinno[PIN_LED_RDY]   =  0;
  pgm->pinno[PIN_LED_PGM]   =  0;
  pgm->pinno[PIN_LED_VFY]   =  0;

  len = strlen(progname) + 2;
  for (i=0; i<len; i++)
    progbuf[i] = ' ';
  progbuf[i] = 0;

  /*
   * check for no arguments
   */
  if (argc == 1) {
    usage();
    return 0;
  }


  /*
   * process command line arguments
   */
  while ((ch = getopt(argc,argv,"?c:C:eE:f:Fi:m:no:p:P:tvVyY:")) != -1) {

    switch (ch) {
      case 'c': /* pin configuration */
        pinconfig = optarg;
        if (strcmp(pinconfig, "stk500") == 0) {
          if (port == DEFAULT_PARALLEL) {
            port = DEFAULT_SERIAL;
          }
        }
        break;

      case 'C': /* pin configuration file */
        strncpy(configfile, optarg, PATH_MAX);
        configfile[PATH_MAX-1] = 0;
        break;

      case 'm': /* select memory type to operate on */
        if ((strcasecmp(optarg,"e")==0)||(strcasecmp(optarg,"eeprom")==0)) {
          memtype = "eeprom";
        }
        else if ((strcasecmp(optarg,"f")==0)||
                 (strcasecmp(optarg,"flash")==0)) {
          memtype = "flash";
        }
        else {
          memtype = optarg;
        }
        readorwrite = 1;
        break;

      case 'F': /* override invalid signature check */
        ovsigck = 1;
        break;

      case 'n':
        nowrite = 1;
        break;

      case 'o': /* specify output file */
        if (inputf || terminal) {
          fprintf(stderr,"%s: -i, -o, and -t are incompatible\n\n", progname);
          return 1;
        }
        doread = 1;
        outputf = optarg;
        if (filefmt == FMT_AUTO)
          filefmt = FMT_RBIN;
        break;

      case 'p' : /* specify AVR part */
        partdesc = optarg;
        break;

      case 'e': /* perform a chip erase */
        erase = 1;
        break;

      case 'E':
        exitspecs = optarg;
	break;

      case 'i': /* specify input file */
        if (outputf || terminal) {
          fprintf(stderr,"%s: -o, -i, and -t are incompatible\n\n", progname);
          return 1;
        }
        doread = 0;
        inputf = optarg;
        break;

      case 'f':   /* specify file format */
        if (strlen(optarg) != 1) {
          fprintf(stderr, "%s: invalid file format \"%s\"\n",
                  progname, optarg);
          usage();
          exit(1);
        }
        switch (optarg[0]) {
          case 'a' : filefmt = FMT_AUTO; break;
          case 'i' : filefmt = FMT_IHEX; break;
          case 'r' : filefmt = FMT_RBIN; break;
          case 's' : filefmt = FMT_SREC; break;
            break;
          default :
            fprintf(stderr, "%s: invalid file format \"%s\"\n\n",
                    progname, optarg);
            usage();
            exit(1);
        }
        break;

      case 't': /* enter terminal mode */
        if (!((inputf == NULL)||(outputf == NULL))) {
          fprintf(stderr, 
                  "%s: terminal mode is not compatible with -i or -o\n\n",
                  progname);
          usage();
          exit(1);
        }
        terminal = 1;
        break;

      case 'P':
        port = optarg;
        break;

      case 'v':
        verbose++;
        break;

      case 'V':
        verify = 0;
        break;

      case 'y':
        do_cycles = 1;
        break;

      case 'Y':
        set_cycles = strtol(optarg, &e, 0);
        if ((e == optarg) || (*e != 0)) {
          fprintf(stderr, "%s: invalid cycle count '%s'\n",
                  progname, optarg);
          exit(1);
        }
        do_cycles = 1;
        break;

      case '?': /* help */
        usage();
        exit(0);
        break;

      default:
        fprintf(stderr, "%s: invalid option -%c\n\n", progname, ch);
        usage();
        exit(1);
        break;
    }

  }

  if (verbose) {
    /*
     * Print out an identifying string so folks can tell what version
     * they are running
     */
    fprintf(stderr, 
            "\n%s: Version %s\n"
            "%sCopyright (c) 2000-2003 Brian Dean, bsd@bsdhome.com\n\n", 
            progname, version, progbuf);
  }

  rc = read_config(configfile);
  if (rc) {
    fprintf(stderr, "%s: error reading \"%s\" configuration from \"%s\"\n",
            progname, pinconfig, configfile);
    exit(1);
  }

  if (strcmp(pinconfig, "avrdude") == 0) {
    pgm = locate_pinconfig(programmers, "default");
    if (pgm == NULL) {
      /* no default config listed, use the compile-in default */
      pgm = compiled_in_pgm;
    }
  }
  else {
    pgm = locate_pinconfig(programmers, pinconfig);
    if (pgm == NULL) {
      fprintf(stderr, 
              "%s: Can't find programmer id \"%s\"\n",
              progname, pinconfig);
      fprintf(stderr,"\n");
      exit(1);
    }
  }

  if (partdesc == NULL) {
    fprintf(stderr, 
            "%s: No AVR part has been specified, use \"-p Part\"\n\n"
            "  Valid Parts are:\n\n",
            progname);
    list_parts(stderr, "    ", part_list);
    fprintf(stderr, "\n");
    fprintf(stderr, "(These come from the config file \"%s\")\n", configfile);
    fprintf(stderr, "\n");
    exit(1);
  }


  p = locate_part(part_list, partdesc);
  if (p == NULL) {
    fprintf(stderr, 
            "%s: AVR Part \"%s\" not found.  Valid parts are:\n\n",
            progname, partdesc);
    list_parts(stderr, "    ", part_list);
    fprintf(stderr, "\n");
    fprintf(stderr, "(These come from the config file \"%s\")\n", configfile);
    fprintf(stderr, "\n");
    exit(1);
  }


  if (exitspecs != NULL) {
    if (strcmp(pgm->type, "PPI") != 0) {
      fprintf(stderr, 
              "%s: WARNING: -E option is only valid with \"PPI\" "
              "programmer types\n",
              progname);
      exitspecs = NULL;
    }
    else if (getexitspecs(exitspecs, &ppisetbits, &ppiclrbits) < 0) {
      usage();
      exit(1);
    }
  }


  /* 
   * set up seperate instances of the avr part, one for use in
   * programming, one for use in verifying.  These are separate
   * because they need separate flash and eeprom buffer space 
   */
  p = avr_dup_part(p);
  v = avr_dup_part(p);

  if (strcmp(pgm->type, "PPI") == 0) {
    verify_pin_assigned(PIN_AVR_RESET, "AVR RESET");
    verify_pin_assigned(PIN_AVR_SCK,   "AVR SCK");
    verify_pin_assigned(PIN_AVR_MISO,  "AVR MISO");
    verify_pin_assigned(PIN_AVR_MOSI,  "AVR MOSI");
  }

  /*
   * open the programmer
   */
  pgm->open(pgm, port);

  if (verbose) {
    avr_display(stderr, p, progbuf, verbose);
    fprintf(stderr, "\n");
    pinconfig_display(progbuf);
  }

  fprintf(stderr, "\n");

  exitrc = 0;

  /*
   * allow the programmer to save its state
   */
  rc = pgm->save(pgm);
  if (rc < 0) {
    exitrc = 1;
    ppidata = 0; /* clear all bits at exit */
    goto main_exit;
  }

  if (strcmp(pgm->type, "PPI") == 0) {
    pgm->ppidata &= ~ppiclrbits;
    pgm->ppidata |= ppisetbits;
  }

  /*
   * enable the programmer
   */
  pgm->enable(pgm);

  /*
   * turn off all the status leds
   */
  pgm->rdy_led(pgm, OFF);
  pgm->err_led(pgm, OFF);
  pgm->pgm_led(pgm, OFF);
  pgm->vfy_led(pgm, OFF);

  /*
   * initialize the chip in preperation for accepting commands
   */
  rc = pgm->initialize(pgm, p);
  if (rc < 0) {
    fprintf(stderr, "%s: initialization failed, rc=%d\n", progname, rc);
    exitrc = 1;
    goto main_exit;
  }

  /* indicate ready */
  pgm->rdy_led(pgm, ON);

  fprintf(stderr, 
            "%s: AVR device initialized and ready to accept instructions\n",
            progname);

  /*
   * Let's read the signature bytes to make sure there is at least a
   * chip on the other end that is responding correctly.  A check
   * against 0xffffffff should ensure that the signature bytes are
   * valid.  
   */
  rc = avr_signature(pgm, p);
  if (rc != 0) {
    fprintf(stderr, "%s: error reading signature data, rc=%d\n",
            progname, rc);
    exit(1);
  }

  sig = avr_locate_mem(p, "signature");
  if (sig == NULL) {
    fprintf(stderr,
            "%s: WARNING: signature data not defined for device \"%s\"\n",
            progname, p->desc);
  }

  if (sig != NULL) {
    int ff;

    fprintf(stderr, "%s: Device signature = 0x", progname);
    ff = 1;
    for (i=0; i<sig->size; i++) {
      fprintf(stderr, "%02x", sig->buf[i]);
      if (sig->buf[i] != 0xff)
        ff = 0;
    }
    fprintf(stderr, "\n");

    if (ff) {
      fprintf(stderr, 
              "%s: Yikes!  Invalid device signature.\n", progname);
      if (!ovsigck) {
        fprintf(stderr, "%sDouble check connections and try again, "
                "or use -F to override\n"
                "%sthis check.\n\n",
                progbuf, progbuf);
        exitrc = 1;
        goto main_exit;
      }
    }
  }

  if (set_cycles != -1) {
    rc = avr_get_cycle_count(pgm, p, &cycles);
    if (rc == 0) {
      /*
       * only attempt to update the cycle counter if we can actually
       * read the old value
       */
      cycles = set_cycles;
      fprintf(stderr, "%s: setting erase-rewrite cycle count to %d\n", 
              progname, cycles);
      rc = avr_put_cycle_count(pgm, p, cycles);
      if (rc < 0) {
        fprintf(stderr, 
                "%s: WARNING: failed to update the erase-rewrite cycle "
                "counter\n",
                progname);
      }
    }
  }

  if (erase) {
    /*
     * erase the chip's flash and eeprom memories, this is required
     * before the chip can accept new programming
     */
    fprintf(stderr, "%s: erasing chip\n", progname);
    pgm->chip_erase(pgm, p);
    fprintf(stderr, "%s: done.\n", progname);
  }
  else if (set_cycles == -1) {
    /*
     * The erase routine displays this same information, so don't
     * repeat it if an erase was done.  Also, don't display this if we
     * set the cycle count (due to -Y).
     *
     * see if the cycle count in the last four bytes of eeprom seems
     * reasonable 
     */
    rc = avr_get_cycle_count(pgm, p, &cycles);
    if ((rc >= 0) && (cycles != 0xffffffff)) {
      fprintf(stderr,
              "%s: current erase-rewrite cycle count is %d%s\n",
              progname, cycles, 
              do_cycles ? "" : " (if being tracked)");
    }
  }



  if (!terminal && ((inputf==NULL) && (outputf==NULL))) {
    /*
     * Check here to see if any other operations were selected and
     * generate an error message because if they were, we need either
     * an input or an output file, but one was not selected.
     * Otherwise, we just shut down.  
     */
    if (readorwrite) {
      fprintf(stderr, "%s: you must specify an input or an output file\n",
              progname);
      exitrc = 1;
    }
    goto main_exit;
  }

  if (terminal) {
    /*
     * terminal mode
     */
    exitrc = terminal_mode(pgm, p);
  }
  else if (doread) {
    /*
     * read out the specified device memory and write it to a file 
     */
    fprintf(stderr, "%s: reading %s memory:\n", 
            progname, memtype);
    rc = avr_read(pgm, p, memtype, 0, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: failed to read all of %s memory, rc=%d\n", 
              progname, memtype, rc);
      exitrc = 1;
      goto main_exit;
    }
    size = rc;

    fprintf(stderr, "%s: writing output file \"%s\"\n",
            progname, outputf);
    rc = fileio(FIO_WRITE, outputf, filefmt, p, memtype, size);
    if (rc < 0) {
      fprintf(stderr, "%s: terminating\n", progname);
      exitrc = 1;
      goto main_exit;
    }

  }
  else {
    /*
     * write the selected device memory using data from a file; first
     * read the data from the specified file
     */
    fprintf(stderr, "%s: reading input file \"%s\"\n",
            progname, inputf);
    rc = fileio(FIO_READ, inputf, filefmt, p, memtype, -1);
    if (rc < 0) {
      fprintf(stderr, "%s: terminating\n", progname);
      exitrc = 1;
      goto main_exit;
    }
    size = rc;

    /*
     * write the buffer contents to the selected memory type
     */
    fprintf(stderr, "%s: writing %s (%d bytes):\n", 
            progname, memtype, size);

    if (!nowrite) {
      rc = avr_write(pgm, p, memtype, size, 1);
    }
    else {
      /* 
       * test mode, don't actually write to the chip, output the buffer
       * to stdout in intel hex instead 
       */
      rc = fileio(FIO_WRITE, "-", FMT_IHEX, p, memtype, size);
    }

    if (rc < 0) {
      fprintf(stderr, "%s: failed to write %s memory, rc=%d\n", 
                progname, memtype, rc);
      exitrc = 1;
      goto main_exit;
    }

    vsize = rc;

    fprintf(stderr, "%s: %d bytes of %s written\n", progname, 
            vsize, memtype);

  }

  if (!doread && verify) {
    /* 
     * verify that the in memory file (p->mem[AVR_M_FLASH|AVR_M_EEPROM])
     * is the same as what is on the chip 
     */
    pgm->vfy_led(pgm, ON);

    fprintf(stderr, "%s: verifying %s memory against %s:\n", 
            progname, memtype, inputf);
    fprintf(stderr, "%s: reading on-chip %s data:\n", 
            progname, memtype);
    rc = avr_read(pgm, v, memtype, vsize, 1);
    if (rc < 0) {
      fprintf(stderr, "%s: failed to read all of %s memory, rc=%d\n", 
              progname, memtype, rc);
      pgm->err_led(pgm, ON);
      exitrc = 1;
      goto main_exit;
    }

    fprintf(stderr, "%s: verifying ...\n", progname);
    rc = avr_verify(p, v, memtype, vsize);
    if (rc < 0) {
      fprintf(stderr, "%s: verification error; content mismatch\n", 
              progname);
      pgm->err_led(pgm, ON);
      exitrc = 1;
      goto main_exit;
    }
    
    fprintf(stderr, "%s: %d bytes of %s verified\n", 
            progname, rc, memtype);

    pgm->vfy_led(pgm, OFF);
  }



 main_exit:

  /*
   * program complete
   */

  pgm->powerdown(pgm);

  /*
   * restore programmer state
   */
  pgm->restore(pgm);

  pgm->disable(pgm);

  pgm->rdy_led(pgm, OFF);

  pgm->close(pgm);

  fprintf(stderr, "\n%s done.  Thank you.\n\n", progname);

  return exitrc;
}

