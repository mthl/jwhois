/* rwhois.c - RWhois queries
   Copyright (C) 2001-2002, 2007, 2015, 2016 Free Software Foundation, Inc.

   This file is part of GNU JWhois.

   GNU JWhois is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU JWhois is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU JWhois.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>
#include "system.h"

/* Specification.  */
#include "whois.h"

#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include "init.h"
#include "jconfig.h"
#include "utils.h"
#include "whois.h"

/* This register holds the capabilities of the server */
int rwhois_capab;

/* This register tells if we're currently parsing info  */
int info_on;

/* This says what level of recursion we're on */
int recursion_level;

/* This is filled in with the referrals found */
struct s_referrals {
  char *host;
  int port;
  char *autharea;
  struct s_referrals *next;
};

#define CAP_CLASS       0x000001
#define CAP_DIRECTIVE   0x000002
#define CAP_DISPLAY     0x000004
#define CAP_FORWARD     0x000008
#define CAP_HOLDCONNECT 0x000010
#define CAP_LIMIT       0x000020
#define CAP_NOTIFY      0x000040
#define CAP_QUIT        0x000080
#define CAP_REGISTER    0x000100
#define CAP_SCHEMA      0x000200
#define CAP_SECURITY    0x000400
#define CAP_SOA         0x000800
#define CAP_STATUS      0x001000
#define CAP_XFER        0x002000
#define CAP_X           0x004000

#define REP_OK       0x01
#define REP_ERROR    0x02
#define REP_INIT     0x03
#define REP_CONT     0x04
#define REP_REFERRAL 0x05

static struct
{
  const char *name;
  int cap;
} capabilities[] = 
{
  {"class", CAP_CLASS},
  {"directive", CAP_DIRECTIVE},
  {"display", CAP_DISPLAY},
  {"forward", CAP_FORWARD},
  {"holdconnect", CAP_HOLDCONNECT},
  {"limit", CAP_LIMIT},
  {"notify", CAP_NOTIFY},
  {"quit", CAP_QUIT},
  {"register", CAP_REGISTER},
  {"schema", CAP_SCHEMA},
  {"security", CAP_SECURITY},
  {"soa", CAP_SOA},
  {"status", CAP_STATUS},
  {"xfer", CAP_XFER},
  {"X", CAP_X},
  {NULL, 0}
};

int rwhois_read_line(FILE *, char *, char **);
int rwhois_insert_referral(const char *, struct s_referrals **);
int rwhois_parse_line(const char *, char **);


/*
 *  This function takes a filedescriptor as an argument, makes an rwhois
 *  query to that host:port. If successfull, it returns the result in the block
 *  of text pointed to by text.
 *
 *  Returns:   -1 Error
 *              0 Success
 */
int
rwhois_query_internal (whois_query_t wq, char **text, struct s_referrals **referrals)
{
  int sockfd, ret, limit;
  FILE *f;
  char *reply, *tmpptr, *retptr;
  const char *presentation = "-rwhois V-1.5 " PACKAGE " " VERSION "\r\n";

  printf("[%s %s]\n", _("Querying"), wq->host);

  rwhois_capab = 0;
  info_on = 0;

  sockfd = make_connect(wq->host, wq->port);
  if (sockfd < 0)
    {
      printf(_("[Unable to connect to remote host]\n"));
      return -1;
    }

  add_text_to_buffer(text, create_string("[%s]\n", wq->host));

  f = fdopen(sockfd, "r+");
  if (!f)
    return -1;

  reply = malloc(MAXBUFSIZE);
  if (!reply)
    return -1;

  fprintf(f, "%s", presentation);
  do
    {
      ret = rwhois_read_line(f, reply, text);
    }
  while (ret != REP_INIT && ret != REP_ERROR && ret != REP_OK);

  if (ret == REP_ERROR)
    printf(_("[RWHOIS: Protocol error while sending -rwhois option]\n"));

  if (arguments->verbose > 1)
    {
      printf("[RWHOIS: Server capabilities (%x):", rwhois_capab);
      ret = 0;
      while (capabilities[ret].cap != 0)
	{
	  if (rwhois_capab & capabilities[ret].cap)
	    {
	      if (ret % 8 == 0)
		printf("]\n[       ");
	      printf("%s ", capabilities[ret].name);
	    }
	  ret++;
	}
      printf("]\n");
    }

  if (arguments->rwhois_display)
    tmpptr = arguments->rwhois_display;
  else
    tmpptr = (char *)get_whois_server_option (wq->host, "rwhois-display");

  if (tmpptr)
    {
      if (rwhois_capab & CAP_DISPLAY)
	{
	  if (arguments->verbose > 1)
	    printf("[RWHOIS: Setting display to %s]\n", tmpptr);

	  fprintf(f, "-display %s\r\n", tmpptr);

	  do
	    {
	      ret = rwhois_read_line(f, reply, text);
	    }
	  while (ret != REP_OK && ret != REP_ERROR);
	}
      else
	if (arguments->verbose)
	  printf("[RWHOIS: %s]\n",
		 _("Server does not support display command"));
    }

  if (arguments->rwhois_limit)
    limit = arguments->rwhois_limit;
  else
    {
      tmpptr = (char *)get_whois_server_option(wq->host, "rwhois-limit");
      if (tmpptr)
	{
	  limit = strtol(tmpptr, &retptr, 10);
	  if (*retptr != '\0')
	    {
	      printf("[RWHOIS: %s (%s)]\n",
		     _("Invalid limit in configuration file"),
		     tmpptr);
	    }
	}
      else
	limit = 0;
    }

  if (limit)
    {
      if (rwhois_capab & CAP_LIMIT)
	{
	  if (arguments->verbose > 1)
	    printf("[RWHOIS: Setting limit to %d]\n", limit);

	  fprintf(f, "-limit %d\r\n", limit);

	  do
	    {
	      ret = rwhois_read_line(f, reply, text);
	    }
	  while (ret != REP_OK && ret != REP_ERROR);
	}
      else
        {
          if (arguments->verbose)
            printf ("[RWHOIS: %s]\n", _("Server does not support limit"));
        }
    }

  if (arguments->verbose > 1)
    printf("[RWHOIS: Sending query \"%s\"]\n", wq->query);

  fprintf(f, "%s\r\n", wq->query);

  do
    {
      ret = rwhois_read_line(f, reply, text);
      if (ret == REP_REFERRAL)
	rwhois_insert_referral(reply, referrals);
    }
  while (ret != REP_OK && ret != REP_ERROR);

  fprintf(f, "-quit\r\n");
  do
    {
      ret = rwhois_read_line(f, reply, text);
    }
  while (ret != REP_OK && ret != REP_ERROR);
  
  fclose(f);
  return 0;
}

/*
 *  This function accepts a referral reply in reply and populates
 *  the refferals structure passed to it with the correct values.
 */
int
rwhois_insert_referral(const char *reply, struct s_referrals **referrals)
{
  struct s_referrals *s;
  char *tmpptr, *ret = NULL;
  int len;

  if (!STRNCASEEQ (strchr (reply, ' ') + 1, "rwhois://", 9))
    {
      if (arguments->verbose)
	printf("[RWHOIS: %s: %s]\n", _("Unknown referral"), strchr(reply, ' ')+1);

      return -1;
    }
  if (!*referrals)
    {
      *referrals = xmalloc (sizeof (struct s_referrals));
      (*referrals)->next = NULL;
      s = *referrals;
    }
  else
    {
      s = xmalloc (sizeof (struct s_referrals));
      s->next = *referrals;
      *referrals = s;
    }

  len = strrchr(reply, ':')-strchr(reply, ' ')-10;
  s->host = xmalloc (len + 1);
  strncpy(s->host, strchr(reply, ' ')+10, len);
  s->host[len] = '\0';

  len = strrchr(reply, '/')-strrchr(reply, ':')-1;
  tmpptr = xmalloc (len + 1);
  strncpy(tmpptr, strrchr(reply, ':')+1, len);
  tmpptr[len] = '\0';

  s->port = strtol(tmpptr, &ret, 10);
  if (*ret != '\0')
    {
      *referrals = s->next;
      return -1;
    }

  len = strlen(reply)-(strrchr(reply, '=')-reply)-1;

  tmpptr = xmalloc (len + 1);
  strncpy(tmpptr, strrchr(reply, '=')+1, len);
  s->autharea = tmpptr;
  tmpptr[len] = '\0';

  if (arguments->verbose > 1)
    printf("[RWHOIS: Referral to %s:%d (autharea=%s)]\n",
	   s->host, s->port, s->autharea);

  return 0;
}

/*
 *  This function is the main loop for rwhois queries. It is the only one
 *  called from other files. It calls the internal function above to make
 *  an rwhois query and then follows recursions.
 *
 *  Returns:   -1 Error
 *              0 Success
 */
int
rwhois_query (whois_query_t wq, char **text)
{
  struct s_referrals *referrals, *s;
  struct s_referrals *authareas, *a;
  int followed, ret, iret;

  referrals = NULL;
  iret = rwhois_query_internal(wq, text, &referrals);

  if (referrals)
    {
      authareas = NULL;
      
      s = referrals;
      while (s)
	{
	  followed = 0;
	  if (authareas)
	    {
	      a = authareas;
	      while (a)
		{
                  if (STRCASEEQ (s->autharea, a->autharea))
		    followed = 1;
		  a = a->next;
		}
	    }
	  if (!followed)
	    {
	      wq->host = referrals->host;
	      wq->port = referrals->port;
              if (arguments->verbose)
		printf("[RWHOIS: %s %s:%d (autharea=%s)]\n",
		       _("Following referral to"),
		       wq->host, wq->port, referrals->autharea);

	      ret = rwhois_query(wq, text);
	      if (ret != -1)
		{
		  a = xmalloc (sizeof (struct s_referrals));
		  a->autharea = s->autharea;
		  a->next = NULL;
		  authareas->next = a;
		  authareas = a;
		}
	    }
	  s = s->next;
	}
    }

  return iret;
}

/*
 *  This reads input from a file descriptor and stores the contents
 *  in the indicated pointer.
 */
int
rwhois_read_line(FILE *f, char *ptr, char **text)
{
  if (feof(f))
    {
      printf(_("[Host terminated connection prematurely]\n"));
      exit (EXIT_FAILURE);
    }
  
  if (!fgets(ptr, MAXBUFSIZE-1, f))
      return REP_ERROR;

  if (!ptr)
    {
      return REP_ERROR;
    }
  return rwhois_parse_line(ptr, text);
}

/*
 *  This parses the reply sent by the server.
 */
int
rwhois_parse_line(const char *reply, char **text)
{
  char *tmpptr;

  tmpptr = (char *)strchr(reply, '\n');
  if (tmpptr)
    *tmpptr = '\0';
  
  if (info_on && !STRNCASEEQ (reply, "%info", 5))
    {
      add_text_to_buffer(text, create_string("%s\n", reply));
      return REP_CONT;
    }

  if (STRNCASEEQ (reply, "%rwhois", 7))
    {
      char *capab = (char *)strchr(reply, ':');
      if (!capab)
	return REP_ERROR;
      capab++; /* skip past first : */
      tmpptr = (char *)strchr(capab, ':');
      if (!tmpptr)
	return REP_ERROR;
      *tmpptr = '\0';
      sscanf (capab, "%d", &rwhois_capab);
      return REP_INIT;
    }

  if (STRNCASEEQ (reply, "%ok", 3))
    return REP_OK;

  if (STRNCASEEQ (reply, "%error", 6))
    {
      tmpptr = (char *)strchr(reply, ' ');
      if (!tmpptr)
	return REP_ERROR;
      add_text_to_buffer(text, create_string("%s\n", tmpptr+1));
      return REP_ERROR;
    }

  if (STRNCASEEQ (reply, "%referral", 9))
    {
      return REP_REFERRAL;
    }

  if (STRNCASEEQ (reply, "%info on", 8))
    {
      info_on = 1;
      return REP_CONT;
    }

  if (STRNCASEEQ (reply, "%info off", 9))
    {
      info_on = 0;
      return REP_CONT;
    }

  if (STRNCASEEQ (reply, "%", 1))
    {
      tmpptr = (char *) strchr (reply, ' ');
      if (!tmpptr)
        return REP_ERROR;
      *tmpptr = '\0';
      if (arguments->verbose)
        printf ("[RWHOIS: %s: %s]\n", _("Unhandled reply"),
                reply + 1);

      return REP_CONT;
    }

  add_text_to_buffer(text, create_string("%s\n", reply));
  return REP_CONT;
}
