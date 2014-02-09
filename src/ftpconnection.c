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
#include "ftpcommands.h"
#include "ftpsignals.h"

#define STANDARD_TIMEOUT 60
#define INTERNAL_TIMEOUT 1

int ftp_error = 0;

#define FTP_TLS_OK 0
#define FTP_TLS_NOTSUPPORTED 1
#define FTP_TLS_ERROR 2


int ftp_i_socket_connect(char *destination, unsigned int port, unsigned long timeout)
{
	struct addrinfo *info, hints;
	int sockfd;
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	char portstr[10];
	sprintf(portstr, "%i", port);

	if (getaddrinfo(destination, portstr, &hints, &info) != 0)
		return -1;

	for (; info; info = info->ai_next) {
		if ((sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) < 0)
			continue;
		if (connect(sockfd, info->ai_addr, info->ai_addrlen) < 0) {
			close(sockfd);
			continue;
		}
		struct timeval t;
		t.tv_sec = timeout;
		t.tv_usec = 0;
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval));
		return sockfd;
	}
	return -1;
}

int ftp_connect(ftp_connection *c, char *host, unsigned int port)
{
	c->_sockfd = ftp_i_socket_connect(host, port, INTERNAL_TIMEOUT);
	if (c->_sockfd < 0) {
		ftp_error = FTP_ECONNECTION;
		return 1;
	}

	ftp_i_strcpy_malloc(c->_host, host);
	c->_port = port;

	return 0;
}

#ifdef FTP_TLS_ENABLED

int ftp_i_tls_set_protection_level(ftp_connection *c)
{
	ftp_i_set_input_trigger(c, FTP_SIGNAL_COMMAND_OKAY);

	if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CPBSZ, FTP_CPBSZ_NULL, NULL, FTP_EUNEXPECTED, NULL) != FTP_OK)
		return FTP_TLS_ERROR;

	ftp_i_set_input_trigger(c, FTP_SIGNAL_COMMAND_OKAY);

	if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CPROT, FTP_CPROT_PRIVATE, NULL, FTP_EUNEXPECTED, NULL) != FTP_OK)
		return FTP_TLS_ERROR;

	return FTP_TLS_OK;
}

int ftp_i_tls_init(ftp_connection *c)
{
	ftp_i_set_input_trigger(c, FTP_SIGNAL_TLS_SUCCESSFUL);

	ftp_bool remote_error;

	c->_disable_input_thread = ftp_btrue;
	if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CAUTHTLS, NULL, NULL, 0, &remote_error) != FTP_OK) {
		if (remote_error)
			return FTP_TLS_NOTSUPPORTED;
		else
			return FTP_TLS_ERROR;
	}

	/* Temporarily turn off input thread for handshake. */
	if (ftp_i_release_input_thread(c) != 0)
		return FTP_TLS_ERROR;

	if (ftp_i_tls_connect(c->_sockfd, &c->_tls_info, NULL, &c->error) != FTP_OK)
		return FTP_TLS_ERROR;

	c->_disable_input_thread = ftp_bfalse;
	if (ftp_i_establish_input_thread(c) != 0)
		return FTP_TLS_ERROR;

	return ftp_i_tls_set_protection_level(c);
}

int ftp_i_tls_init_data_connection(ftp_connection *c)
{
	if (ftp_i_tls_connect(c->_data_connection, &c->_tls_info_dc, c->_tls_info, &c->error) != FTP_OK)
		return FTP_TLS_ERROR;

	return FTP_TLS_OK;
}

#endif

int ftp_i_init(ftp_connection *c, char *host, unsigned int port, ftp_security security)
{
	if (ftp_connect(c, host, port) == 0) {
		c->status = FTP_CONNECTING;

		ftp_i_set_input_trigger(c, FTP_SIGNAL_SERVICE_READY);

		if (ftp_i_establish_input_thread(c) != 0)
			return FTP_ETHREAD;

		if (ftp_i_wait_for_triggers(c) != FTP_OK)
			return c->error;

		if (ftp_i_last_signal_was_error(c))
			return FTP_ENOSERVICE;

#ifdef FTP_TLS_ENABLED
		if (security != ftp_security_none) {
			int tls = ftp_i_tls_init(c);

			if (tls == FTP_TLS_ERROR)
				return c->error;

			if (tls == FTP_TLS_NOTSUPPORTED && security == ftp_security_always)
				return FTP_ESECURITY;
		}
#endif

		c->status = FTP_UP;

		return 0;
	} else {
		return FTP_ECONNECTION;
	}
}

ftp_connection *ftp_open(char *host, unsigned int port, ftp_security security)
{
	ftp_error = 0;
	ftp_connection *c = calloc(1, sizeof(ftp_connection));
	if (!c) {
		ftp_error = FTP_ECOULDNOTALLOCATE;
		return NULL;
	}

	c->status = FTP_DOWN;
	c->timeout = STANDARD_TIMEOUT;
	c->_mc_enabled = ftp_bfalse;
	c->file_transfer_second_connection = ftp_btrue;
	c->content_listing_filter = ftp_btrue;
	c->__features.use_epsv = c->__features.use_mlsd = ftp_btrue;
	c->_current_features = &(c->__features);

	if ((ftp_error = ftp_i_init(c, host, port, security)) != 0) {
		ftp_close(c);
		return NULL;
	}

	return c;
}

ftp_status ftp_i_establish_data_connection(ftp_connection *c)
{
	int sockfd;

	int pasv_port = ftp_i_enter_pasv(c);
	if (pasv_port < 0)
		return FTP_ERROR;

	sockfd = ftp_i_socket_connect(c->_host, pasv_port, STANDARD_TIMEOUT);
	if (sockfd < 0) {
		ftp_i_connection_set_error(c, FTP_ECONNECTION);
		return FTP_ERROR;
	}

	c->_data_connection = sockfd;
	return FTP_OK;
}

ftp_status ftp_i_prepare_data_connection(ftp_connection *c)
{
#ifdef FTP_TLS_ENABLED
	if (c->_tls_info) {
		if (ftp_i_tls_init_data_connection(c) != FTP_TLS_OK)
			return FTP_ERROR;
	}
#endif
	return FTP_OK;
}

void ftp_i_close_data_connection(ftp_connection *c)
{
#ifdef FTP_TLS_ENABLED
	ftp_i_tls_disconnect(&c->_tls_info_dc);
#endif
	shutdown(c->_data_connection,SHUT_WR);
	close(c->_data_connection);
	c->_data_connection=0;
}

void ftp_i_close(ftp_connection *c)
{
	if (c->status != FTP_DOWN) {
		if (c->_data_connection)
			ftp_i_close_data_connection(c);
		c->_termination_signal = ftp_btrue;

		ftp_i_set_input_trigger(c, FTP_SIGNAL_GOODBYE);
		ftp_send(c, FTP_CQUIT FTP_CENDL);
		ftp_i_wait_for_triggers(c);
		/* No need for error handling as the connection will be closed anyways. */

		c->status = FTP_DOWN;
		close(c->_sockfd);
		ftp_i_release_input_thread(c);
	}

	ftp_i_managed_buffer_free(c->_last_answer_buffer);
	ftp_i_free(c->cur_directory);
	ftp_i_free(c->_mc_pass);
	ftp_i_free(c->_mc_user);
	ftp_i_free(c->_host);

#ifdef FTP_SERVER_VERBOSE
	ftp_i_managed_buffer_free(c->verbose_command_buffer);
#endif

#ifdef FTP_TLS_ENABLED
	ftp_i_tls_disconnect(&(c->_tls_info));
	ftp_i_tls_disconnect(&(c->_tls_info_dc));
#endif

	free(c);
}

void ftp_close(ftp_connection *c)
{
	if (c->_child) {
		FTP_LOG("Closing a queued connection.\n");
		ftp_close(c->_child);
	}

	ftp_i_close(c);
}

#ifdef FTP_TLS_ENABLED

ssize_t ftp_i_write(ftp_connection *c, int cid, const void *buf, size_t len)
{
	if (cid == 0) {
		//write to control connection
		if (c->_tls_info)
			return ftp_i_tls_write(c->_tls_info, buf, len);
		else
			return write(c->_sockfd, buf, len);
	} else {
		//write to data connection
		if (c->_tls_info_dc)
			return ftp_i_tls_write(c->_tls_info_dc, buf, len);
		else
			return write(c->_data_connection, buf, len);
	}
}

ssize_t ftp_i_read(ftp_connection *c, int cid, void *buf, size_t len)
{
	if (cid == 0) {
		//read from control connection
		if (c->_tls_info)
			return ftp_i_tls_read(c->_tls_info, buf, len);
		else
			return read(c->_sockfd, buf, len);
	} else {
		//read from data connection
		if (c->_tls_info_dc)
			return ftp_i_tls_read(c->_tls_info_dc, buf, len);
		else
			return read(c->_data_connection, buf, len);
	}
}

#else

ssize_t ftp_i_write(ftp_connection *c, int cid, const void *buf, size_t len)
{
	return write(cid == 0 ? c->_sockfd : c->_data_connection, buf, len);
}
ssize_t ftp_i_read(ftp_connection *c, int cid, void *buf, size_t len)
{
	return read(cid == 0 ? c->_sockfd : c->_data_connection, buf, len);
}

#endif

ftp_status ftp_send(ftp_connection *c, char *signal)
{
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return FTP_ERROR;
	}

#ifdef FTP_SERVER_VERBOSE
	if (!c->verbose_command_buffer)
		c->verbose_command_buffer = ftp_i_managed_buffer_new();

	ftp_i_managed_buffer_append_str(c->verbose_command_buffer, signal);

	if (ftp_i_managed_buffer_contains_str(c->verbose_command_buffer, FTP_CENDL, ftp_bfalse)) {
		/* Command buffer contains complete message */
		printf("# [client->server] ");

		if (c->_temporary)
			printf("(TMP) ");

		if (ftp_i_managed_buffer_contains_str(c->verbose_command_buffer, FTP_CPASS, ftp_btrue))
			printf("PASS ****\n");
		else
			ftp_i_managed_buffer_print(c->verbose_command_buffer, ftp_btrue);

		ftp_i_managed_buffer_free(c->verbose_command_buffer);
		c->verbose_command_buffer = NULL;
	}
#endif

	unsigned long len = strlen(signal);
	ssize_t n = ftp_i_write(c, 0, signal, len);
	if (n < 0) {
		c->error = FTP_EWRITE;
		return FTP_ERROR;
	}
	return FTP_OK;
}

ftp_status ftp_i_send_command_and_wait_for_triggers(ftp_connection *c, char *command, char *arg1, char *arg2, int error, ftp_bool *remote_err)
{
	if (ftp_send(c, command) != FTP_OK)
		return FTP_ERROR;
	if (arg1) {
		if (ftp_send(c, " ") != FTP_OK ||
			ftp_send(c, arg1) != FTP_OK)
			return FTP_ERROR;
		if (arg2) {
			if (ftp_send(c, " ") != FTP_OK ||
				ftp_send(c, arg2) != FTP_OK)
				return FTP_ERROR;
		}
	}
	if (ftp_send(c, FTP_CENDL) != FTP_OK)
		return FTP_ERROR;

	if (ftp_i_wait_for_triggers(c) != FTP_OK) {
		if (remote_err)
			*remote_err = ftp_bfalse;
		return FTP_ERROR;
	}
	if (ftp_i_last_signal_was_error(c)) {
		if (remote_err)
			*remote_err = ftp_btrue;
		if (error > 0)
			ftp_i_connection_set_error(c, error);
		return FTP_ERROR;
	}

	return FTP_OK;
}

ftp_status ftp_i_read_data_connection_into_buffer(ftp_connection *c, ftp_i_managed_buffer *buf)
{
	char chr;
	ssize_t n;
	while ((n = ftp_i_read(c, 1, &chr, 1)) == 1) {
		if (ftp_i_managed_buffer_append(buf, &chr, 1) != FTP_OK) {
			ftp_i_connection_set_error(c, FTP_ECOULDNOTALLOCATE);
			return FTP_ERROR;
		}
	}

	if (n < 0) {
		if (ftp_i_is_timed_out(errno)) {
			errno = 0;
			ftp_i_connection_set_error(c, FTP_ETIMEOUT);
		} else {
			ftp_i_connection_set_error(c, FTP_ESOCKET);
		}
		return FTP_ERROR;
	}

	return FTP_OK;
}

ftp_status ftp_i_set_transfer_type(ftp_connection *c, ftp_transfer_type tt)
{
	if (tt == ftp_tt_undefined)
		FTP_WARN("BUG: called with tt = undefined\n");
	if (c->_transfer_type == tt)
		return FTP_OK;

	ftp_i_set_input_trigger(c, FTP_SIGNAL_COMMAND_OKAY);

	ftp_status success;

	switch (tt) {
	case ftp_tt_binary:
		success = ftp_i_send_command_and_wait_for_triggers(c, FTP_CTYPE, FTP_CTYPE_BINARY, NULL, FTP_ESERVERCAPABILITIES, NULL);
		break;
	default:
		success = ftp_i_send_command_and_wait_for_triggers(c, FTP_CTYPE, FTP_CTYPE_ASCII, NULL, FTP_ESERVERCAPABILITIES, NULL);
		break;
	}

	if (success == FTP_ERROR)
		return FTP_ERROR;

	c->_transfer_type = tt;
	return FTP_OK;
}

int ftp_i_enter_pasv_old(ftp_connection *c)
{
	int r;
	ftp_i_set_input_trigger(c, FTP_SIGNAL_ENTERING_PASSIVE_MODE);
	c->_last_answer_lock_signal = FTP_SIGNAL_ENTERING_PASSIVE_MODE;

	if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CPASV, NULL, NULL, FTP_EUNEXPECTED, NULL) != FTP_OK)
		return -1;

	char answer[1200];
	r = ftp_i_textfrombrackets(ftp_i_managed_buffer_cbuf(c->_last_answer_buffer), answer, 1200);
	if (r != 0) {
		c->error = r;
		return -1;
	}
	ftp_i_managed_buffer_free(c->_last_answer_buffer);
	c->_last_answer_buffer = NULL;
	unsigned int pasv_values[6];
	r = ftp_i_values_from_comma_separated_string(answer, pasv_values, 6);
	if (r != 6) {
		c->error = FTP_EUNEXPECTED;
		return -1;
	}
	c->_current_features->use_epsv = ftp_bfalse;
	return (256 * pasv_values[4]) + pasv_values[5];
}

int ftp_i_enter_pasv(ftp_connection *c)
{
	if (c->status != FTP_UP) {
		c->error = FTP_ENOTREADY;
		return -1;
	}
	if (!c->_current_features->use_epsv)
		return ftp_i_enter_pasv_old(c);

	ftp_i_set_input_trigger(c, FTP_SIGNAL_ENTERING_EXTENDED_PASSIVE_MODE);
	c->_last_answer_lock_signal = FTP_SIGNAL_ENTERING_EXTENDED_PASSIVE_MODE;

	ftp_bool remote_error;
	if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CEPSV, NULL, NULL, 0, &remote_error) != FTP_OK) {
		if (!remote_error)
			return -1;
		/* Server may not support EPSV */
		return ftp_i_enter_pasv_old(c);
	}

	//epasv syntax: 229 foo (|||port|)
	char ex[1200];
	int r = ftp_i_textfrombrackets(ftp_i_managed_buffer_cbuf(c->_last_answer_buffer), ex, 1200);
	if (r != 0) {
		c->error = r;
		return -1;
	}
	ftp_i_managed_buffer_free(c->_last_answer_buffer);
	c->_last_answer_buffer = NULL;
	ftp_i_ex_answer answer = ftp_i_interpret_ex_answer(ex, &r);
	if (r != 0) {
		c->error = r;
		return -1;
	}

	return answer.tcp_port;
}