/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2004  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "tds.h"
#include "tdssrv.h"

static char software_version[] = "$Id: query.c,v 1.14 2005/03/29 09:57:57 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/**
 * Read a TDS5 tokenized query.
 */
char *
tds_get_query(TDSSOCKET * tds)
{
	static unsigned char *query;
	static size_t query_buflen = 0;
	int len;

	if (query_buflen == 0) {
		query_buflen = 1024;
		query = (unsigned char *) malloc(query_buflen);
	}
	tds_get_byte(tds);	/* 33 */
	len = tds_get_smallint(tds);	/* query size +1 */
	tds_get_n(tds, NULL, 3);
	if (len > query_buflen) {
		query_buflen = len;
		query = (unsigned char *) realloc(query, query_buflen);
	}
	tds_get_n(tds, query, len - 1);
	return (char *) query;
}

/**
 * Read a query, and return it as an ASCII string with a \0 terminator.  This
 * should work for TDS4, TDS5, and TDS7+ connections.  Also, it converts RPC
 * calls into stored procedure queries, and it skips CANCEL packets.  The query
 * string is returned in a static buffer which is overwritten each time this
 * function is called.
 * \param tds  The socket to read from.
 * \return A query string if successful, or NULL if we either can't read from
 * the socket or we read something that we can't handle.
 */
char *tds_get_generic_query(TDSSOCKET * tds)
{
	static char *query;
	static int query_buflen;
 	int token, byte;
	int len, more, i, j;

	for (;;) {
		/*
		 * Read a new packet.  We must explicitly read it,
		 * instead of letting functions such as tds_get_byte()
		 * to read it for us, so that we can examine the packet
		 * type via tds->in_flag.
		 */
		if (tds_read_packet(tds) < 0) {
			return NULL;
		}

		/* Queries can arrive in a couple different formats. */
		switch (tds->in_flag) {
		case 0x03: /* RPC packet */
		case 0x0f: /* TDS5 query packet */
			/* get the token */
			token = tds_get_byte(tds);
			switch (token) {
			case TDS_LANGUAGE_TOKEN:
				/* SQL query */
				len = tds_get_smallint(tds); /* query size +1 */
				tds_get_n(tds, NULL, 3);
				if (len > query_buflen) {
					query_buflen = len;
					query = (char *) realloc(query, query_buflen);
				}
				tds_get_n(tds, query, len - 1);
				query[len - 1] = '\0';
				return query;

			case TDS_DBRPC_TOKEN:
				/* RPC call -- make it look like a query */

				/* skip the overall length */
				(void)tds_get_smallint(tds);

				/* get the length of the stored procedure's name */
				len = tds_get_byte(tds) + 1;/* sproc name size +1 */
				if (len > query_buflen) {
					query_buflen = len;
					query = (char *) realloc(query, query_buflen);
				}

				/*
				 * Read the chars of the name.  Skip NUL
				 * bytes, as a cheap way to convert
				 * Unicode to ASCII.  (For TDS7+, the
				 * name is sent in Unicode.)
				 */
				for (i = j  = 0; i < len - 1; i++) {
					byte = tds_get_byte(tds);
					if (byte != '\0')
						query[j++] = byte;
				}
				query[j] = '\0';

				/* TODO: WE DON'T HANDLE PARAMETERS YET! */

				/* eat the rest of the packet */
				while (!tds->last_packet && tds_read_packet(tds) > 0) {
				}
				return query;

			default:
				/* unexpected token */

				/* eat the rest of the packet */
				while (!tds->last_packet && tds_read_packet(tds) > 0) {
				}
				return NULL;
			}
			break;

		case 0x01:
			/* TDS4 and TDS7+ fill the whole packet with a query */
			len = 0;
			do {
				/* If buffer needs to grow, then grow */
				more = tds->in_len - tds->in_pos;
				if ((size_t)(len + more + 1) > query_buflen)
				{
					query_buflen += 1024;
					query = (char *)realloc(query, query_buflen);
				}

				/*
				 * Pull new data into the query buffer.
				 * Ignore NUL bytes -- this is a cheap way
				 * to convert Unicode to Latin-1/ASCII.
				 */
				while (--more >= 0)
				{
					query[len] = tds_get_byte(tds);
					if (query[len] != '\0')
						len++;
				}

				/* if more then read it */
				more = !tds->last_packet;
				if (more && tds_read_packet(tds) < 0) {
					return NULL;
				}
			} while (more);

			/* add a NUL to mark the end */
			query[len] = '\0';
			return query;

		  case 0x06:
			/*
			 * ignore cancel requests -- if we're waiting
			 * for the next query then it's obviously too
			 * late to cancel the previous query.
			 */
			/* TODO it's not too late -- freddy77 */
			break;

		  default:
			/* not a query packet */
			return NULL;
		}
	}
}
