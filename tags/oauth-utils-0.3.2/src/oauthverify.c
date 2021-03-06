/* 
   oauthverify - command line oauth

   Copyright (C) 2008 Robin Gareus

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  

#include <termios.h>
#include <grp.h>
#include <pwd.h>
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <getopt.h>
#include "system.h"

#include "oauth_common.h"

#define EXIT_FAILURE 1

char *xmalloc ();
char *xrealloc ();
char *xstrdup ();

static void usage (int status);

/* The name the program was run with, stripped of any leading path. */
char *program_name;

/* getopt_long return codes */
enum {DUMMY_CODE=129
     ,NOWARN_CODE
     ,NULLCK_CODE
     ,NULLCS_CODE
     ,NULLTK_CODE
     ,NULLTS_CODE
};

/* Option flags and variables */

int no_warnings  = 0; /* --no-warn */ 
int want_quiet   = 0; /* --quiet, --silent */
int want_verbose = 0; /* --verbose */

int mode         = 1; ///< mode: 1=GET 2=POST; general operation-mode - bit coded 
                      //  bit0 (1)  : enable ?! (also see want_dry_run)
                      //  bit1 (2)  : HTTP POST enable (no GET) 
                      //  bit2 (4)  : (unused ; old oauthrequest() compat)
                      //  bit3 (8)  : -b base-string and exit
                      //  bit4 (16) : -B base-url and exit
                      //  bit5 (32) :  print secrets along with base-string(8)
                      //  bit6 (64) :  print signature after generating it. (unless want_verbose is set: it's printed anyway)
                      //  bit7 (128):  parse reply (request token, access token)
                      //  bit8 (256):  escape POST parameters with format_array(..)
                      //  bit9 (512):  curl-output
                      //
char *method;

int   method_set = 0;   // compare signature-method
int   oauth_argc = 0;
char **oauth_argv = NULL;
char *datafile   = NULL;
oauthparam op;

static struct option const long_options[] =
{
  {"quiet", no_argument, 0, 'q'},
  {"silent", no_argument, 0, 'q'},
  {"verbose", no_argument, 0, 'v'},
  {"no-warn", no_argument, 0, NOWARN_CODE},
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},

  {"consumer-key", required_argument, 0, 'c'},
  {"consumer-secret", required_argument, 0, 'C'},
  {"token-key", required_argument, 0, 't'},
  {"token-secret", required_argument, 0, 'T'},
  {"CK", required_argument, 0, 'c'},
  {"CS", required_argument, 0, 'C'},
  {"TK", required_argument, 0, 't'},
  {"TS", required_argument, 0, 'T'},
  {"erase-consumer-key", no_argument, 0, NULLCK_CODE},
  {"erase-consumer-secret", no_argument, 0, NULLCS_CODE},
  {"erase-token-key", no_argument, 0, NULLTK_CODE},
  {"erase-token-secret", no_argument, 0, NULLTS_CODE},
//{"signature-method", no_argument, 0, 'm'}, //  oauth signature method

  {"request", required_argument, 0, 'r'}, // HTTP request method (GET, POST)
  {"post", no_argument, 0, 'p'},
  {"data", required_argument, 0, 'd'},
  {"base-url", no_argument, 0, 'B'},
  {"base-string", no_argument, 0, 'b'},

  {"file", required_argument, 0, 'f'},
  {NULL, 0, NULL, 0}
};

/** Set all the option flags according to the switches specified.
 *  Return the index of the first non-option argument.  
 */
static int decode_switches (int argc, char **argv) {
  int c;

  while ((c = getopt_long (argc, argv, 
			   "h"	/* help */
			   "V" 	/* version */
			   "q"	/* quiet or silent */
			   "v"	/* verbose */
			   "b"  /* base-string*/
			   "B"  /* base-URL*/
			   "r:" /* HTTP Request method */
			   "p" 	/* post */
			   "d:" /* URL-query parameter data eg.
               * '-d name=daniel -d skill=lousy'->'name=daniel&skill=lousy' */
		     "m:" /* oauth signature Method */

			   "c:" /* consumer-key*/
			   "C:" /* consumer-secret */
			   "t:" /* token-key*/
			   "T:" /* token-secret */

			   "f:" /* read key/data file */
			   "e", /* erase token */
			   long_options, (int *) 0)) != EOF) {
    switch (c) {
      case 'q':		/* --quiet, --silent */
        want_quiet = 1;
        break;
      case 'v':		/* --verbose */
        want_verbose = 1;
        break;
      case NOWARN_CODE:	/* --no-warn */
        no_warnings = 1;
        break;
      case 'V':
        printf ("%s %s (%s)", PACKAGE, VERSION, OS);
        #ifdef LIBOAUTH_VERSION
        printf (" liboauth/%s", LIBOAUTH_VERSION);
        #endif
        printf ("\n");
        exit (0);

      case 'b':
        mode&=~(8|16);
        mode|=8;
        break;
      case 'B':
        mode&=~(8|16);
        mode|=16;
        break;
      case 'p':
        mode&=~(1|2);
        mode|=2;
        free(method); method=xstrdup("POST");
        break;
      case 'r':
        mode&=~(1|2|4);
        if (!strncasecmp(optarg,"GET",3)) {
          mode|=1;
          free(method); method=xstrdup("GET");
        } else if (!strncasecmp(optarg,"POST",4)) {
          mode|=2;
          free(method); method=xstrdup("POST");
        } else {
          int i;
          mode|=1; // XXX
          method=strdup(optarg); 
          for (i=0;i<strlen(method);i++) method[i]=toupper(method[i]);
        }
        break;
      case 't':
        if (op.t_key) free(op.t_key);
        op.t_key=xstrdup(optarg); 
        break;
      case 'T':
        if (op.t_secret) free(op.t_secret);
        op.t_secret=xstrdup(optarg); 
        break;
      case 'c':
        if (op.c_key) free(op.c_key);
        op.c_key=xstrdup(optarg); 
        break;
      case 'C':
        if (op.c_secret) free(op.c_secret);
        op.c_secret=xstrdup(optarg); 
        break;
      case 'd': 
        add_param_to_array(&oauth_argc, &oauth_argv,optarg);
        break;
      case 'f':
        read_keyfile(optarg, &op);
      case 'e':
        reset_oauth_token(&op);
        break;
      case NULLCK_CODE:	/* --erase-consumer-key */
        if (op.c_key) free(op.c_key);
        op.c_key=NULL;
        break;
      case NULLCS_CODE:	/* --erase-consumer-secret */
        if (op.c_secret) free(op.c_secret);
        op.c_secret=NULL;
        break;
      case NULLTK_CODE:	/* --erase-token-key */
        if (op.t_key) free(op.t_key);
        op.t_key=NULL;
        break;
      case NULLTS_CODE:	/* --erase-token-secret */
        if (op.t_secret) free(op.t_secret);
        op.t_secret=NULL;
        break;
      case 'm':
        if (parse_oauth_method(&op, optarg)) usage(1);
        method_set=1;
        break;
      case 'h':
        usage (0);

      default:
        usage (EXIT_FAILURE);
    }
  }
  return optind;
}


static void
usage (int status)
{
  printf (_("%s - \
command line utilities for oauth\n"), program_name);
  printf (_("Usage: %s [OPTION]... URL\n"), program_name);
  printf (_("\
Options:\n\
  -h, --help                  display this help and exit\n\
  -V, --version               output version information and exit\n\
  -q, --quiet, --silent       inhibit usual output\n\
  -v, --verbose               print more information\n\
  --no-warn                   dont print any warnings.\n\
  \n\
  -b, --base-string           print OAuth base-string and exit\n\
  -B, --base-url              print OAuth base-URL and exit\n\
"/*
  --curl                      format output as `curl` commandline\n\
*/"\
  -r, --request <type>        HTTP request type (HEAD, POST, GET [default],..)\n\
  -p, --post                  same as -r POST\n\
  -d, --data <key>[=<val>]    add url query parameters.\n\
  -m, --signature-method <m>  oauth signature method (PLAINTEXT,\n\
                              RSA-SHA1, HMAC-SHA1 [default])\n\
"/*
  -P,                         print URL-escaped POST parameters\n\ 
*/"\
  \n\
  -c, --CK, --consumer-key    <text> - require this consumer\n\
  -C, --CS, --consumer-secret <text> - set consumer secret\n\
  -t, --TK, --token-key       <text> - require this token\n\
  -T, --TS, --token-secret    <text> - set token secret\n\
  \n\
  -f, --file <filename>       read tokens and secrets from config-file\n\
  -e, --erase-tokens          clear [access|request] tokens.\n\
  --erase-consumer-key        unset consumer-key\n\
  --erase-consumer-secret     unset consumer-secret\n\
  --erase-token-key           unset token-key\n\
  --erase-token-secret        unset token-secret\n\
  \n\
"));
  exit (status);
}


int main (int argc, char **argv) {
  int i;
  int exitval=0;
  char *sign = NULL;
  char *wanted_sign = NULL;
  int myargc=0;
  char **myargv = NULL;

  // initialize 

  program_name = argv[0];
  memset(&op,0,sizeof(oauthparam));
  reset_oauth_param(&op);
  method=xstrdup("GET");

  // parse command line

  i = decode_switches (argc, argv);

  if (i>=argc) usage(1);
  op.url=xstrdup(argv[i++]);
  if (argc>i) usage(EXIT_FAILURE);

  // do the work.
 
  if (!wanted_sign) {
    // search op.url for 'oauth_signature' 
    char *start, *end;
    if ((start=strstr(op.url,"oauth_signature="))) {
      start+=16;
      if (!(end=strchr(start,'&'))) {
        end=start+strlen(start);
      }
      char *tmp=xmalloc((end-start+1)*sizeof(char));
      strncpy(tmp,start,(end-start));
      tmp[(end-start)]=0;
      wanted_sign=url_unescape(tmp);
      free(tmp);
    }
  }
  if (!wanted_sign) {
    // search oauth_argv for 'oauth_signature' 
    int ii;
    for (ii=0;ii<oauth_argc;ii++) {
      if (!strncmp(oauth_argv[ii],"oauth_signature=",16)) {
        wanted_sign=xstrdup(&(oauth_argv[ii][16]));
        break;
      }
    }
  }
  if (!wanted_sign) {
    if (!no_warnings)
      fprintf(stderr, "Warning: Can not find any signature to verify.\n");
    exit(1);
  }


  // recalculate signature.
  url_to_array(&myargc, &myargv, mode, op.url);
  append_parameters(&myargc, &myargv, oauth_argc, oauth_argv);
  sign=process_array(myargc, myargv, method, mode, &op);

  // check if all required oauth params are present
	if (!oauth_param_exists(myargv, myargc,"oauth_nonce")) {
    exitval|=256;
    if (!want_quiet) fprintf(stderr, "Note: missing require parameter 'oauth_nonce'.\n");
  }
	if (!oauth_param_exists(myargv, myargc,"oauth_timestamp")) {
    exitval|=256;
    if (!want_quiet) fprintf(stderr, "Note: missing require parameter 'oauth_timestamp'.\n");
  }
	if (!oauth_param_exists(myargv, myargc,"oauth_consumer_key")) {
    exitval|=256;
    if (!want_quiet) fprintf(stderr, "Note: missing require parameter 'oauth_consumer_key'.\n");
  }
	if (!oauth_param_exists(myargv, myargc,"oauth_signature_method")) {
    exitval|=256;
    if (!want_quiet) fprintf(stderr, "Note: missing require parameter 'oauth_signature_method'.\n");
  }

  if (1) { // require op.c_key to match consumer_key and similar op.t_key and method
    OAuthMethod signature_method = op.signature_method; // remember wanted-method
    int ii;
    int flags=0;
    for (ii=0;ii<myargc;ii++) {
      if (!strncmp(myargv[ii],"oauth_consumer_key=",19)) {
        if (!op.c_key) continue;
        if (strcmp(&(myargv[ii][19]),op.c_key)) {
          exitval|=4;
          if (!want_quiet) 
            fprintf(stderr, "Note: consumer key mismatch.\n");
        } else flags|=1;
      }
      if (!strncmp(myargv[ii],"oauth_token=",12)) {
        if (!op.t_key) continue;
        if (strcmp(&(myargv[ii][12]),op.t_key)) {
          exitval|=8;
          if (!want_quiet) 
            fprintf(stderr, "Note: token mismatch.\n");
        } else flags|=2;
      }
      if (!strncmp(myargv[ii],"oauth_signature_method=",23)) {
        if(parse_oauth_method(&op, &(myargv[ii][23]))) {
          if (!want_quiet && !no_warnings) 
            fprintf(stderr, "Warning: Can not parse signature method.\n");
          exitval|=2;
        }
      }
    }
    if (flags != ((op.t_key?2:0)|(op.c_key?1:0))) {
      if ((exitval&12)==0 && !want_quiet) 
        fprintf(stderr, "Note: required token not found\n");
      exitval|=16;
    }
    if (method_set && signature_method != op.signature_method) {
      if (exitval==0 && !want_quiet) 
        fprintf(stderr, "Note: signature method differs from requirement.\n");
      exitval|=32;
    }
  }

  // compare wanted_sign and sign .. 
  if (!sign) {
    exitval|=64;
    if (!no_warnings && !want_quiet) 
      fprintf(stderr,"WARNING: could not generate oAuth signature.\n");
  } else {
    if (want_verbose) {
      fprintf(stderr, "wanted: '%s'\n", wanted_sign);
      fprintf(stderr, "got:    '%s'\n", sign);
    }
    if (strcmp(sign, wanted_sign)) {
      exitval|=128;
      if (!want_quiet && !no_warnings) 
        fprintf(stderr, "WARNING: signatures mismatch.\n");
    } else if (!want_quiet || want_verbose) fprintf(stderr, "good signature.\n");
  }

  if (exitval==0 && !want_quiet) {
    if (sign) add_kv_to_array(&myargc, &myargv, "oauth_signature", sign);
    format_array(2, myargc, myargv);
  }

  if (sign) free(sign);
  free(method);
  free_array(myargc,myargv);
  free_array(oauth_argc, oauth_argv);
  return (exitval);
}

/* vim: set sw=2 ts=2 sts=2 et : */
