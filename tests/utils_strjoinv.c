/* Test of strjoinv function.
   Copyright (C) 2016 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Declaration.  */
#include "utils.h"

#include <string.h>
#include "macros.h"

int
main (void)
{
  const char *strv[] = { "foo", "bar", "baz" };
  const char *delim = " ";
  const char *expected_res = "foo bar baz";
  const char *res = strjoinv (delim, SIZEOF (strv), strv);
  printf ("expected: %s\n", expected_res);
  printf ("result: %s\n", res);
  ASSERT (STREQ (expected_res, res));

  return EXIT_SUCCESS;
}