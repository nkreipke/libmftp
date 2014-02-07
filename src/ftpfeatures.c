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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ftpfunctions.h"
#include "ftpsignals.h"
#include "ftpcommands.h"

ftp_status ftp_reload_cur_directory(ftp_connection *c)
{
	int r;
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return FTP_ERROR;
	}
	ftp_i_set_input_trigger(c, FTP_SIGNAL_MKDIR_SUCCESS_OR_PWD);
	c->_last_answer_lock_signal = FTP_SIGNAL_MKDIR_SUCCESS_OR_PWD;

	if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CPWD, NULL, NULL, FTP_EUNEXPECTED, NULL) != FTP_OK)
		return FTP_ERROR;

	r = ftp_i_set_pwd_information(ftp_i_managed_buffer_cbuf(c->_last_answer_buffer), &c->cur_directory);
	ftp_i_managed_buffer_free(c->_last_answer_buffer);

	if (r != 0) {
		c->error = r;
		return FTP_ERROR;
	}
	return FTP_OK;
}

ftp_status ftp_change_cur_directory(ftp_connection *c, char *path)
{
	if (strlen(path) + 5 > ANSWER_LEN) {
		c->error = FTP_ETOOLONG;
		return FTP_ERROR;
	}
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return FTP_ERROR;
	}
	char msg[ANSWER_LEN];
	sprintf(msg, FTP_CCWD " %s" FTP_CENDL, path);
	ftp_i_set_input_trigger(c, FTP_SIGNAL_REQUESTED_ACTION_OKAY);

	return ftp_i_send_command_and_wait_for_triggers(c, FTP_CCWD, path, NULL, FTP_EUNEXPECTED, NULL);
}

ftp_content_listing *ftp_contents_of_directory(ftp_connection *c, int *items_count)
{
	if (c->_data_connection != 0 || c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return NULL;
	}

	if (ftp_i_set_transfer_type(c, ftp_tt_ascii) != FTP_OK)
		return NULL;

	//buffer
	ftp_i_managed_buffer *buffer = ftp_i_managed_buffer_new();
	if (!buffer) {
		c->error = FTP_ECOULDNOTALLOCATE;
		return NULL;
	}
	//start

	if (ftp_i_establish_data_connection(c) != FTP_OK)
		return NULL;

	ftp_bool use_mlsd = c->_current_features->use_mlsd;

	//please forgive me for using goto.
again:
	ftp_i_set_input_trigger(c, FTP_SIGNAL_ABOUT_TO_OPEN_DATA_CONNECTION);
	ftp_i_set_input_trigger(c, FTP_SIGNAL_DATA_CONNECTION_OPEN_STARTING_TRANSFER);
	if (use_mlsd)
		ftp_send(c, FTP_CMLSD FTP_CENDL);
	else
		ftp_send(c, FTP_CLIST FTP_CENDL);
	if (ftp_i_wait_for_triggers(c) != FTP_OK) {
		ftp_i_close_data_connection(c);
		return NULL;
	}
	if (ftp_i_last_signal_was_error(c)) {
		if (use_mlsd) {
			//server may have not understood MLSD
			use_mlsd = ftp_bfalse;
			goto again;
		} else {
			c->error = FTP_EUNEXPECTED;
			ftp_i_close_data_connection(c);
			return NULL;
		}
	}

	//prepare data connection
	if (ftp_i_prepare_data_connection(c) != FTP_OK) {
		ftp_i_close_data_connection(c);
		return NULL;
	}

	//read buffered data
	char buf;
	ssize_t n;
	while ((n = ftp_i_read(c, 1, &buf, 1)) == 1) {
		if (ftp_i_managed_buffer_append(buffer, &buf, 1) != FTP_OK) {
			ftp_i_managed_buffer_free(buffer);
			ftp_i_close_data_connection(c);
			c->error = FTP_ECOULDNOTALLOCATE;
			return NULL;
		}
	}
	ftp_i_close_data_connection(c);
	if (n < 0) {
		ftp_i_managed_buffer_free(buffer);
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			errno = 0;
			c->error = FTP_ETIMEOUT;
		} else {
			c->error = FTP_ESOCKET;
		}
		return NULL;
	}

	ftp_content_listing *content = NULL;
	int itemscount, error;
	if (use_mlsd) {
		content = ftp_i_read_mlsd_answer(buffer, &itemscount, &error);
	} else {
		c->_current_features->use_mlsd = ftp_bfalse;
		content = ftp_i_read_list_answer(buffer, &itemscount, &error);
	}

	ftp_i_managed_buffer_free(buffer);

	if (!content) {
		ftp_i_connection_set_error(c, error);
		return NULL;
	}

	if (c->content_listing_filter)
		content = ftp_i_applyclfilter(content, &itemscount);


	if (items_count != NULL)
		*items_count = itemscount;

	return content;
}

ftp_bool ftp_item_exists_in_content_listing(ftp_content_listing *c, char *file_nm, ftp_content_listing **current)
{
	for (ftp_content_listing *cl = c; cl; cl = cl->next) {
		if (strcmp(cl->filename, file_nm) == 0) {
			if (current)
				*current = cl;
			return ftp_btrue;
		}
	}
	return ftp_bfalse;
}

ftp_file *ftp_fopen(ftp_connection *c, char *filenm, ftp_activity activity, unsigned long startpos)
{
	if (activity != FTP_READ && activity != FTP_WRITE) {
		c->error = FTP_EARGUMENTS;
		return NULL;
	}
	if (activity == FTP_READ && startpos == FTP_APPEND) {
		c->error = FTP_EARGUMENTS;
		return NULL;
	}
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return NULL;
	}
	ftp_file *f = (ftp_file*)malloc(sizeof(ftp_file));
	if (!f) {
		c->error = FTP_ECOULDNOTALLOCATE;
		return NULL;
	}
	f->eof = ftp_bfalse;
	f->parent = NULL;
	f->activity = activity;

	ftp_connection *fc;
	if ((fc = ftp_i_dequeue_usable_connection(c, c->file_transfer_second_connection, ftp_btrue)) == NULL) {
		c->error = FTP_ENOTREADY;
		ftp_i_free(f);
		return NULL;
	}

	f->c = fc;
	f->error = &(fc->error);

	if (ftp_i_set_transfer_type(fc, ftp_tt_binary) != FTP_OK ||
		ftp_i_establish_data_connection(fc) != FTP_OK) {
		if (fc != c) {
			c->error = fc->error;
			ftp_i_mark_as_unused(fc);
		}
		ftp_i_free(f);
		return NULL;
	}

	fc->_internal_error_signal = ftp_bfalse;
	if (activity == FTP_WRITE) {
		char command[500];
		if (startpos == FTP_APPEND) {
			sprintf(command, FTP_CAPPE " %s" FTP_CENDL,filenm);
		} else {
			sprintf(command, FTP_CSTOR " %s" FTP_CENDL,filenm);
			if (startpos > 0) {
				char retrcmd[100];
				sprintf(retrcmd, FTP_CREST " %li" FTP_CENDL, startpos);
				ftp_send(fc, retrcmd);
			}
		}
		ftp_i_set_input_trigger(fc, FTP_SIGNAL_ABOUT_TO_OPEN_DATA_CONNECTION);
		ftp_i_set_input_trigger(fc, FTP_SIGNAL_DATA_CONNECTION_OPEN_STARTING_TRANSFER);
		ftp_send(fc, command);
		if (ftp_i_wait_for_triggers(fc) != FTP_OK) {
			c->error = fc->error;
			ftp_i_close_data_connection(fc);
			free(f);
			return NULL;
		}
		if (ftp_i_last_signal_was_error(fc) || fc->_internal_error_signal) {
			c->error = fc->last_signal == FTP_SIGNAL_REQUESTED_ACTION_ABORTED ? FTP_ENOTPERMITTED : FTP_EUNEXPECTED;
			ftp_i_close_data_connection(fc);
			free(f);
			return NULL;
		}
	} else {
		if (startpos != 0) {
			char retrcmd[100];
			sprintf(retrcmd, FTP_CREST " %li" FTP_CENDL, startpos);
			ftp_send(fc, retrcmd);
		}
		char command[500];
		sprintf(command, FTP_CRETR " %s" FTP_CENDL,filenm);
		ftp_i_set_input_trigger(fc, FTP_SIGNAL_ABOUT_TO_OPEN_DATA_CONNECTION);
		ftp_i_set_input_trigger(fc, FTP_SIGNAL_DATA_CONNECTION_OPEN_STARTING_TRANSFER);
		ftp_send(fc, command);
		if (ftp_i_wait_for_triggers(fc) != FTP_OK) {
			c->error = fc->error;
			ftp_i_close_data_connection(fc);
			free(f);
			return NULL;
		}
		if (ftp_i_last_signal_was_error(fc) || fc->_internal_error_signal) {
			c->error = fc->last_signal == FTP_SIGNAL_REQUESTED_ACTION_ABORTED ? FTP_ENOTPERMITTED : FTP_EUNEXPECTED;
			ftp_i_close_data_connection(fc);
			free(f);
			return NULL;
		}
	}
	if (ftp_i_prepare_data_connection(fc) != FTP_OK) {
		ftp_i_close_data_connection(fc);
		free(f);
		return NULL;
	}
	return f;
}

void ftp_fclose(ftp_file *file)
{
	if (!file)
		return;
	if (file->c->_data_connection != 0)
		ftp_i_close_data_connection(file->c);

	ftp_i_mark_as_unused(file->c);

	free(file);
}

size_t ftp_fwrite(const void *buf, size_t size, size_t count, ftp_file *f)
{
	size_t data_count = 0;
	if (!f || f->c->_data_connection == 0)
		return 0;
	if (f->activity != FTP_WRITE) {
		*(f->error) = FTP_EARGUMENTS;
		return 0;
	}
	//upload chunks
	while (data_count < count) {
		ssize_t r = ftp_i_write(f->c, 1, buf + (data_count * size), size);
		if (r < 0) {
			ftp_i_close_data_connection(f->c);
			*(f->error) = FTP_EUNEXPECTED;
			return data_count;
		}
		if (r != size)
			return data_count;
		data_count++;
	}
	return data_count;
}

size_t ftp_fread(void *buf, size_t size, size_t count, ftp_file *f)
{
	size_t data_count = 0;
	if (!f || f->c->_data_connection == 0)
		return 0;
	if (f->activity != FTP_READ) {
		*(f->error) = FTP_EARGUMENTS;
		return 0;
	}
	//download chunks
	while (data_count < count) {
		ssize_t r = ftp_i_read(f->c, 1, buf + (data_count * size), size);
		if (r < 0) {
			ftp_i_close_data_connection(f->c);
			*(f->error) = FTP_EUNEXPECTED;
			return data_count;
		}
		if (r != size) {
			if (r == 0) {
				//server closed data connection (end of file)
				ftp_i_close_data_connection(f->c);
				f->eof = ftp_btrue;
			}
			return data_count;
		}
		data_count++;
	}
	return data_count;
}

ftp_status ftp_size_legacy(ftp_connection *c, char *filenm, size_t *size)
{
	ftp_content_listing *content = ftp_contents_of_directory(c, NULL);
	if (!content)
		return FTP_ERROR;

	ftp_content_listing *current = NULL;
	if (!ftp_item_exists_in_content_listing(content, filenm, &current)) {
		c->error = FTP_ENOTFOUND;
		return FTP_ERROR;
	}

	if (!current) {
		c->error = FTP_EUNEXPECTED;
		return FTP_ERROR;
	}

	*size = current->facts.size;
	ftp_free(content);
	return FTP_OK;
}

ftp_status ftp_size(ftp_connection *c, char *filenm, size_t *size)
{
//FIXME: learn server SIZE behavior (current_features)
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return FTP_ERROR;
	}
	if (size == NULL) {
		c->error = FTP_EARGUMENTS;
		return FTP_ERROR;
	}

	if (ftp_i_set_transfer_type(c, ftp_tt_binary) != FTP_OK)
		return FTP_ERROR;

	ftp_i_set_input_trigger(c, FTP_SIGNAL_FILE_STATUS);
	c->_last_answer_lock_signal = FTP_SIGNAL_FILE_STATUS;

	ftp_bool remote_error;
	if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CSIZE, filenm, NULL, 0, &remote_error) != FTP_OK) {
		if (!remote_error)
			return FTP_ERROR;

		/* Maybe this server does not support the SIZE command.
		 * We will try to get the size from a directory listing */
		FTP_WARN("Server does not support SIZE command, falling back to legacy content listing mode.\n");
		return ftp_size_legacy(c, filenm, size);
	}

	char *answer = ftp_i_managed_buffer_cbuf(c->_last_answer_buffer);
	int i;
	char sizstr[15];
	for (i = 0; (*(answer+i) != '\r' && *(answer+i) != '\n' && *(answer+i) != '\0'); i++)
		sizstr[i] = *(answer+i);
	sizstr[i] = '\0';
	*size = strtoul(sizstr, (char**)NULL, 10);

	ftp_i_managed_buffer_free(c->_last_answer_buffer);
	return FTP_OK;
}

ftp_status ftp_rename(ftp_connection *c, char *oldfn, char *newfn)
{
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return FTP_ERROR;
	}

	ftp_i_set_input_trigger(c, FTP_SIGNAL_REQUEST_FURTHER_INFORMATION);

	ftp_bool remote_error;
	if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CRNFR, oldfn, NULL, 0, &remote_error) != FTP_OK) {
		if (remote_error)
			ftp_i_connection_set_error(c, (c->last_signal == FTP_SIGNAL_FILE_ERROR ? FTP_ENOTFOUND : FTP_EUNEXPECTED));
		return FTP_ERROR;
	}

	ftp_i_set_input_trigger(c, FTP_SIGNAL_REQUESTED_ACTION_OKAY);

	return ftp_i_send_command_and_wait_for_triggers(c, FTP_CRNTO, newfn, NULL, FTP_EUNEXPECTED, NULL);
}

ftp_status ftp_delete(ftp_connection *c, char *fnm, ftp_bool is_folder)
{
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return FTP_ERROR;
	}

	ftp_i_set_input_trigger(c, FTP_SIGNAL_REQUESTED_ACTION_OKAY);

	ftp_bool remote_error;
	if (ftp_i_send_command_and_wait_for_triggers(c, (is_folder ? FTP_CRMD : FTP_CDELE), fnm, NULL, 0, &remote_error) != FTP_OK) {
		if (remote_error) {
			ftp_i_connection_set_error(c, (c->last_signal == FTP_SIGNAL_FILE_ERROR ?
					(is_folder ? FTP_ENOTFOUND_OR_NOTEMPTY : FTP_ENOTFOUND) :
					FTP_EUNEXPECTED));
		}
		return FTP_ERROR;
	}

	return FTP_OK;
}

ftp_status ftp_chmod(ftp_connection *c, char *fnm, unsigned int mode)
{
	char mode_string[5];

	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return FTP_ERROR;
	}

	if (mode > 777)
		return FTP_EARGUMENTS;

	sprintf(mode_string, "%u", mode);

	ftp_i_set_input_trigger(c, FTP_SIGNAL_COMMAND_OKAY);

	return ftp_i_send_command_and_wait_for_triggers(c, FTP_CUNIX_CHMOD, mode_string, fnm, FTP_EUNEXPECTED, NULL);
}

ftp_status ftp_create_folder(ftp_connection *c, char *fnm)
{
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return FTP_ERROR;
	}

	ftp_i_set_input_trigger(c, FTP_SIGNAL_MKDIR_SUCCESS_OR_PWD);

	return ftp_i_send_command_and_wait_for_triggers(c, FTP_CMKD, fnm, NULL, FTP_EUNEXPECTED, NULL);
}

ftp_status ftp_noop(ftp_connection *c, ftp_bool wfresponse)
{
	if (wfresponse)
		ftp_i_set_input_trigger(c, FTP_SIGNAL_COMMAND_OKAY);
	ftp_send(c, FTP_CNOOP FTP_CENDL);
	if (wfresponse) {
		if (ftp_i_wait_for_triggers(c) != FTP_OK)
			return FTP_ERROR;
		if (ftp_i_last_signal_was_error(c)) {
			c->error = FTP_EUNEXPECTED;
			return FTP_ERROR;
		}
	}
	return FTP_OK;
}
