/*   libmftp
 *
 *   Copyright (c) 2014 nkreipke
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */


#include <stdio.h>
#include "ftpinternal.h"

#define FTP_MAX_TEMP_CONNECTIONS_HELD_OPEN 1


static inline ftp_connection *ftp_i_get_youngest(ftp_connection *c)
{
	ftp_connection *youngest = c;
	while (youngest->_child)
		youngest = youngest->_child;
	return youngest;
}

static inline ftp_connection *ftp_i_get_oldest(ftp_connection *c)
{
	ftp_connection *oldest = c;
	while (oldest->_parent)
		oldest = oldest->_parent;
	return oldest;
}

void ftp_i_add_connection_to_queue(ftp_connection *parent, ftp_connection *child)
{
	ftp_connection *youngest = ftp_i_get_youngest(parent);
	youngest->_child = child;
	child->_parent = youngest;
}

ftp_connection *ftp_i_remove_connection_from_queue(ftp_connection *c)
{
	if (!c->_parent)
		FTP_ERR("BUG: Trying to remove root connection from queue.\n");

	ftp_connection *child = c->_child;
	c->_parent->_child = child;
	if (child)
		child->_parent = c->_parent;

	ftp_i_close(c);

	return child;
}

void ftp_i_queue_try_free(ftp_connection *c, int count)
{
	FTP_LOG("Trying to free %i queued connections.\n", count);
	ftp_connection *current = ftp_i_get_oldest(c)->_child;
	if (!current)
		return;

	while (count > 0 && current) {
		if (ftp_i_connection_is_ready(current) &&
			!current->_data_connection) {
			current = ftp_i_remove_connection_from_queue(current);
			count--;
			FTP_LOG("Removed unused connection from queue.\n");
		} else {
			current = current->_child;
		}
	}
}

ftp_connection *ftp_i_generate_simultaneous_connection(ftp_connection *parent)
{
	if (!parent->_mc_enabled)
		return NULL;

	ftp_connection *child;
	if ((child = ftp_open(parent->_host, parent->_port, ftp_i_open_getsecurity(parent))) == NULL)
		return NULL;
	child->_temporary = ftp_btrue;
	child->_current_features = parent->_current_features;

	if (parent->_mc_user && parent->_mc_pass &&
		ftp_auth(child, parent->_mc_user, parent->_mc_pass, ftp_bfalse) != FTP_OK) {
		ftp_i_close(child);
		return NULL;
	};

	char *cur_directory = ftp_get_cur_directory(parent);
	if (!cur_directory ||
		ftp_change_cur_directory(child, cur_directory) != FTP_OK) {
		ftp_i_close(child);
		return NULL;
	}

	return child;
}

ftp_connection *ftp_i_dequeue_usable_connection(ftp_connection *c, ftp_bool no_main_connection, ftp_bool needs_free_data_connection)
{
	ftp_connection *oldest = ftp_i_get_oldest(c);

	ftp_connection *usable = oldest;
	while (usable &&
		((no_main_connection && !usable->_temporary) ||
		(needs_free_data_connection && usable->_data_connection) ||
		!ftp_i_connection_is_ready(usable)))
		usable = usable->_child;

	if (!usable) {
		FTP_LOG("Establishing new temp connection as no usable connection is available.\n");
		if (!(usable = ftp_i_generate_simultaneous_connection(oldest)))
			return NULL;
		ftp_i_add_connection_to_queue(oldest, usable);
	}

	return usable;
}

void ftp_i_mark_as_unused(ftp_connection *c)
{
	if (!c->_temporary)
		return;

	ftp_connection *oldest = ftp_i_get_oldest(c);
	ftp_connection *youngest = oldest;
	int cnt = 0;
	while (youngest->_child) {
		youngest = youngest->_child;
		cnt++;
	}

	if (cnt > FTP_MAX_TEMP_CONNECTIONS_HELD_OPEN)
		ftp_i_queue_try_free(oldest, cnt - FTP_MAX_TEMP_CONNECTIONS_HELD_OPEN);
}