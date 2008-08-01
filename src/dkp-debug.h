/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __PK_DEBUG_H
#define __PK_DEBUG_H

#include <stdarg.h>
#include <glib.h>

G_BEGIN_DECLS

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
/**
 * dkp_debug:
 *
 * Non critical debugging
 */
#define dkp_debug(...) dkp_debug_real (__func__, __FILE__, __LINE__, __VA_ARGS__)

/**
 * dkp_warning:
 *
 * Important debugging
 */
#define dkp_warning(...) dkp_warning_real (__func__, __FILE__, __LINE__, __VA_ARGS__)

/**
 * dkp_error:
 *
 * Critical debugging, with exit
 */
#define dkp_error(...) dkp_error_real (__func__, __FILE__, __LINE__, __VA_ARGS__)

#elif defined(__GNUC__) && __GNUC__ >= 3
#define dkp_debug(...) dkp_debug_real (__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#define dkp_warning(...) dkp_warning_real (__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#define dkp_error(...) dkp_error_real (__FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#else
#define dkp_debug(...)
#define dkp_warning(...)
#define dkp_error(...)
#endif

void		dkp_debug_init			(gboolean	 debug);
gboolean	dkp_debug_enabled		(void);
void		dkp_debug_backtrace		(void);
void		dkp_debug_real			(const gchar	*func,
						 const gchar	*file,
						 int		 line,
						 const gchar	*format, ...) __attribute__((format (printf,4,5)));
void		dkp_warning_real			(const gchar	*func,
						 const gchar	*file,
						 int		 line,
						 const gchar	*format, ...) __attribute__((format (printf,4,5)));
void		dkp_error_real			(const gchar	*func,
						 const gchar	*file,
						 int		 line,
						 const gchar	*format, ...) __attribute__((format (printf,4,5)));

G_END_DECLS

#endif /* __PK_DEBUG_H */
