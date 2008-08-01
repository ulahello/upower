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

/**
 * SECTION:dkp-debug
 * @short_description: Debugging functions
 *
 * This file contains functions that can be used for debugging.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <execinfo.h>

#include "dkp-debug.h"

#define CONSOLE_RESET		0
#define CONSOLE_BLACK 		30
#define CONSOLE_RED		31
#define CONSOLE_GREEN		32
#define CONSOLE_YELLOW		33
#define CONSOLE_BLUE		34
#define CONSOLE_MAGENTA		35
#define CONSOLE_CYAN		36
#define CONSOLE_WHITE		37

#define PK_LOG_FILE		PK_LOG_DIR "/PackageKit"

static gboolean do_verbose = FALSE;	/* if we should print out debugging */
static gboolean is_console = FALSE;

/**
 * dkp_set_console_mode:
 **/
static void
dkp_set_console_mode (guint console_code)
{
	gchar command[13];

	/* don't put extra commands into logs */
	if (!is_console) {
		return;
	}
	/* Command is the control command to the terminal */
	g_snprintf (command, 13, "%c[%dm", 0x1B, console_code);
	printf ("%s", command);
}

/**
 * dkp_debug_backtrace:
 **/
void
dkp_debug_backtrace (void)
{
	void *call_stack[512];
	int  call_stack_size;
	char **symbols;
	int i = 1;

	call_stack_size = backtrace (call_stack, G_N_ELEMENTS (call_stack));
	symbols = backtrace_symbols (call_stack, call_stack_size);
	if (symbols != NULL) {
		dkp_set_console_mode (CONSOLE_RED);
		g_print ("Traceback:\n");
		while (i < call_stack_size) {
			g_print ("\t%s\n", symbols[i]);
			i++;
		}
		dkp_set_console_mode (CONSOLE_RESET);
		free (symbols);
	}
}

/**
 * dkp_print_line:
 **/
static void
dkp_print_line (const gchar *func, const gchar *file, const int line, const gchar *buffer, guint color)
{
	gchar *str_time;
	gchar *header;
	time_t the_time;
	GThread *thread;

	time (&the_time);
	str_time = g_new0 (gchar, 255);
	strftime (str_time, 254, "%H:%M:%S", localtime (&the_time));
	thread = g_thread_self ();

	/* generate header text */
	header = g_strdup_printf ("TI:%s\tTH:%p\tFI:%s\tFN:%s,%d", str_time, thread, file, func, line);
	g_free (str_time);

	/* always in light green */
	dkp_set_console_mode (CONSOLE_GREEN);
	printf ("%s\n", header);

	/* different colours according to the severity */
	dkp_set_console_mode (color);
	printf (" - %s\n", buffer);
	dkp_set_console_mode (CONSOLE_RESET);

	/* flush this output, as we need to debug */
	fflush (stdout);

	g_free (header);
}

/**
 * dkp_debug_real:
 **/
void
dkp_debug_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	if (do_verbose == FALSE) {
		return;
	}

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	dkp_print_line (func, file, line, buffer, CONSOLE_BLUE);

	g_free(buffer);
}

/**
 * dkp_warning_real:
 **/
void
dkp_warning_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	if (do_verbose == FALSE) {
		return;
	}

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	/* do extra stuff for a warning */
	if (!is_console) {
		printf ("*** WARNING ***\n");
	}
	dkp_print_line (func, file, line, buffer, CONSOLE_RED);

	g_free(buffer);

	/* we want to fix this! */
	dkp_debug_backtrace ();
}

/**
 * dkp_error_real:
 **/
void
dkp_error_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	/* do extra stuff for a warning */
	if (!is_console) {
		printf ("*** ERROR ***\n");
	}
	dkp_print_line (func, file, line, buffer, CONSOLE_RED);
	g_free(buffer);

	/* we want to fix this! */
	dkp_debug_backtrace ();

	exit (1);
}

/**
 * dkp_debug_enabled:
 *
 * Returns: TRUE if we have debugging enabled
 **/
gboolean
dkp_debug_enabled (void)
{
	return do_verbose;
}

/**
 * dkp_debug_init:
 * @debug: If we should print out verbose debugging
 **/
void
dkp_debug_init (gboolean debug)
{
	do_verbose = debug;
	/* check if we are on console */
	if (isatty (fileno (stdout)) == 1) {
		is_console = TRUE;
	}
	dkp_debug ("Verbose debugging %i (on console %i)", do_verbose, is_console);
}

