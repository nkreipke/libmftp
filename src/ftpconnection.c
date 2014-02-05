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

	return 0;
}

#ifdef FTP_TLS_ENABLED

int ftp_connect_tls_privacy(ftp_connection *c)
{
	//this sets data connection protection to private
	ftp_i_set_input_trigger(c, FTP_SIGNAL_COMMAND_OKAY);
	ftp_send(c, FTP_CPBSZ " 0" FTP_CENDL);
	if (ftp_i_wait_for_triggers(c) != FTP_OK)
		return -1;
	if (ftp_i_last_signal_was_error(c))
		return -1;
	ftp_i_set_input_trigger(c, FTP_SIGNAL_COMMAND_OKAY);
	ftp_send(c, FTP_CPROT " P" FTP_CENDL);
	if (ftp_i_wait_for_triggers(c) != FTP_OK)
		return -1;
	if (ftp_i_last_signal_was_error(c))
		return -1;
	//data connection protection level is now 'private'
	return 0;
}

int ftp_connect_tls(ftp_connection *c)
{
	ftp_i_set_input_trigger(c, FTP_SIGNAL_TLS_SUCCESSFUL);
	ftp_send(c, FTP_CAUTHTLS FTP_CENDL);
	c->_disable_input_thread = ftp_btrue;
	if (ftp_i_wait_for_triggers(c) != FTP_OK) {
		//reset error (connection may be used nevertheless)
		c->error = 0;
		return -1;
	}
	if (ftp_i_last_signal_was_error(c))
		return -1;
	//auth tls successful!
	//temporarily turn off input thread:
	ftp_i_release_input_thread(c);
	//do handshake:
	if (ftp_i_tls_connect(c->_sockfd, &c->_tls_info, NULL, &c->error) != FTP_OK) {
		//reset error (connection may be used nevertheless)
		c->error = 0;
		return -1;
	}
	//open new input thread:
	c->_disable_input_thread = ftp_bfalse;
	ftp_i_establish_input_thread(c);
	//tls enabled!
	return ftp_connect_tls_privacy(c);
}

int ftp_connect_tls_data_connection(ftp_connection *c)
{
	//do a handshake on the data connection
	if (ftp_i_tls_connect(c->_data_connection, &c->_tls_info_dc, c->_tls_info, &c->error) != FTP_OK)
		return -1;
	FTP_LOG("SSL handshake on data connection successful.\n");
	return 0;
}

#endif

ftp_connection *ftp_open(char *host, unsigned int port, ftp_security sec)
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

	if (ftp_connect(c,host,port) == 0) {
		c->status = FTP_CONNECTING;
		ftp_i_set_input_trigger(c, FTP_SIGNAL_SERVICE_READY);
		ftp_i_establish_input_thread(c);
		if (ftp_i_wait_for_triggers(c) != FTP_OK) {
			ftp_error = c->error;
			ftp_close(c);
			return NULL;
		}
		if (ftp_i_signal_is_error(c->last_signal)) {
			ftp_error = FTP_ENOSERVICE;
			ftp_close(c);
			return NULL;
		}
		c->_port = port;

#ifdef FTP_TLS_ENABLED
		//check TLS
		if (sec != ftp_security_none) {
			int tls = ftp_connect_tls(c);
			if (tls != 0 && sec == ftp_security_always) {
				//tls connection not successful but security=always
				//abort connection
				ftp_error = FTP_ESECURITY;
				ftp_close(c);
				return NULL;
			}
		}
#endif

		c->status = FTP_UP;
		return c;
	}

	ftp_error = FTP_ECONNECTION;
	ftp_close(c);
	return NULL;
}

void ftp_i_close(ftp_connection *c)
{
	if (c->status != FTP_DOWN) {
		if (c->_data_connection)
			ftp_i_close_data_connection(c);
		c->_termination_signal = ftp_btrue;
		//be nice and say goodbye to server
		ftp_i_set_input_trigger(c, FTP_SIGNAL_GOODBYE);
		ftp_send(c, FTP_CQUIT FTP_CENDL);
		ftp_i_wait_for_triggers(c);
		//no need for error handling as the connection will be closed anyways.
		c->status = FTP_DOWN;
		close(c->_sockfd);
		ftp_i_release_input_thread(c);
	}
	ftp_i_free(c->_last_answer_buffer);
	ftp_i_free(c->cur_directory);
	ftp_i_free(c->_mc_pass);
	ftp_i_free(c->_mc_user);
	ftp_i_free(c->_host);
#ifdef FTP_TLS_ENABLED
	ftp_i_tls_disconnect(&(c->_tls_info));
	ftp_i_tls_disconnect(&(c->_tls_info_dc));
#endif
//    ftp_i_free(c->host);
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
	printf("# [client->server] ");
	if (c->_temporary)
		printf("(TMP) ");
	if (*signal == 'P' && *(signal+1) == 'A' && *(signal+2) == 'S' && *(signal+3) == 'S')
		//obviously we do not print our password
		printf("PASS ****\n");
	else
		printf("%s",signal);
#endif

	unsigned long len = strlen(signal);
	ssize_t n = ftp_i_write(c, 0, signal, len);
	if (n < 0) {
		c->error = FTP_EWRITE;
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
	switch (tt) {
	case ftp_tt_binary:
		ftp_send(c, FTP_CTYPE FTP_CTYPE_BINARY FTP_CENDL);
		break;
	default:
		ftp_send(c, FTP_CTYPE FTP_CTYPE_ASCII FTP_CENDL);
		break;
	}

	if (ftp_i_wait_for_triggers(c) != FTP_OK)
		return FTP_ERROR;
	if (ftp_i_last_signal_was_error(c)) {
		ftp_i_connection_set_error(c, FTP_ESERVERCAPABILITIES);
		return FTP_ERROR;
	}

	c->_transfer_type = tt;
	return FTP_OK;
}

int ftp_i_enter_pasv_old(ftp_connection *c)
{
	int r;
	ftp_i_set_input_trigger(c, FTP_SIGNAL_ENTERING_PASSIVE_MODE);
	c->_last_answer_lock_signal = FTP_SIGNAL_ENTERING_PASSIVE_MODE;
	ftp_send(c, FTP_CPASV FTP_CENDL);
	if (ftp_i_wait_for_triggers(c) != FTP_OK)
		return -1;
	if (ftp_i_last_signal_was_error(c)) {
		c->error = FTP_EUNEXPECTED;
		return -1;
	}
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
	ftp_send(c, FTP_CEPSV FTP_CENDL);
	if (ftp_i_wait_for_triggers(c) != FTP_OK) {
		return -1;
	}
	if (ftp_i_last_signal_was_error(c))
		//maybe the server does not support epsv
		//try standard pasv
		return ftp_i_enter_pasv_old(c);

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

ftp_status ftp_i_establish_data_connection(ftp_connection *c)
{
	int sockfd;
	//enter pasv
	int pasv_port = ftp_i_enter_pasv(c);
	if (pasv_port < 0)
		return FTP_ERROR;
	//open socket
	sockfd = ftp_i_socket_connect(c->_host, pasv_port, STANDARD_TIMEOUT);
	if (sockfd < 0) {
		ftp_i_connection_set_error(c, FTP_ECONNECTION);
		return FTP_ERROR;
	}

	FTP_LOG("data connection socket file descriptor is %i\n",sockfd);
	c->_data_connection = sockfd;
	return FTP_OK;
}

ftp_status ftp_i_prepare_data_connection(ftp_connection *c)
{
#ifdef FTP_TLS_ENABLED
	if (c->_tls_info) {
		//enable tls for this connection
		if (ftp_connect_tls_data_connection(c) != 0) {
			c->error = FTP_ESECURITY;
			return FTP_ERROR;
		}
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