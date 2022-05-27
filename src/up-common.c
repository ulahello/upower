/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "up-common.h"
#include <glib.h>

char*
up_make_safe_string (char *text)
{
	guint i;
	guint idx = 0;

	/* no point checking */
	if (text == NULL)
		return NULL;

	if (g_utf8_validate (text, -1, NULL))
		return text;

	/* shunt up only safe chars */
	for (i=0; text[i] != '\0'; i++) {
		if (g_ascii_isprint (text[i])) {
			/* only copy if the address is going to change */
			if (idx != i)
				text[idx] = text[i];
			idx++;
		} else {
			g_debug ("invalid char: 0x%02X", text[i]);
		}
	}

	/* ensure null terminated */
	text[idx] = '\0';

	return text;
}
