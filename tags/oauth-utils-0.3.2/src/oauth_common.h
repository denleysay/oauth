/* 
   oauth_common - command line oauth

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

*/
#include <oauth.h>

typedef struct {
  char *url;      //< the url to sign
  char *c_key;    //< consumer key
  char *c_secret; //< consumer secret (or NULL)
  char *t_key;    //< token key (or NULL)
  char *t_secret; //< token secret (or NULL)
  OAuthMethod signature_method; //< enum 
//char *request_method;   //< GET, POST, PUT, 
//char *rsa_pub;   //< public key -- used for signing
//char *rsa_key;   //< secret key -- used for verify
} oauthparam;

// common oauth functions
char *oauthsign (int mode, char *method, oauthparam *op);
char *oauthsign_ext (int mode, char *method, oauthparam *op, int optargc, char **optargv, int *saveargcp, char ***saveargvp);
//int oauthsign_alt (int mode, oauthparam *op);

// HTTP API
int parse_reply(const char *reply, char **token, char **secret, int *flags);
//int oauthrequest (int mode, oauthparam *op); // outdated
char *oauthrequest_ext (int mode, oauthparam *op, int oauthargc, char **oauthargv, char *sign);

// mid-level oauth-parameter API
void format_array(int mode, int argc, char **argv);
void format_array_curl(int mode, int argc, char **argv);
void free_array(int argc, char **argv);
char *process_array(int argc, char **argv, char *method, int mode, oauthparam *op);
#if 0 // private
int url_to_array(int *argcp, char ***argvp, int mode, char *url);
void add_param_to_array(int *argcp, char ***argvp, char *addparam);
void add_kv_to_array(int *argcp, char ***argvp, char *key, char *val);
#endif

// other common functions
int parse_oauth_method(oauthparam *op, char *value);
void reset_oauth_param(oauthparam *op);
void reset_oauth_token(oauthparam *op);

// keyfile.c
int read_keyfile(char *fn, oauthparam *op);
int save_keyfile(char *fn, oauthparam *op);

char *url_unescape(const char *string); ///< wrapper around oauth_url_unescape();
