/*
    This file is part of jwhois
    Copyright (C) 1999  Jonas �berg

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef _JCONFIG_H
#define _JCONFIG_H

struct jconfig {
	char	*domain;
	char	*key;
	char	*value;
	struct jconfig	*next;
};

void jconfig_set(void);
struct jconfig *jconfig_next(char *);
void jconfig_end(void);

int jconfig_add(char *, char *, char *);
void jconfig_free(void);

#endif