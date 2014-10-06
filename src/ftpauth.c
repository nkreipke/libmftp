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
#include "ftpinternal.h"
#include "ftpsignals.h"
#include "ftpcommands.h"

ftp_status ftp_i_store_auth(ftp_connection *c, char *user, char *pass)
{
	ftp_i_free(c->_mc_user);
	ftp_i_free(c->_mc_pass);

	ftp_i_strcpy_malloc(c->_mc_user, user);
	ftp_i_strcpy_malloc(c->_mc_pass, pass);

	if (!c->_mc_user || !c->_mc_pass) {
		ftp_i_connection_set_error(c, FTP_ECOULDNOTALLOCATE);
		return FTP_ERROR;
	}

	return FTP_OK;
}

ftp_status ftp_auth(ftp_connection *c, char *user, char *pass, ftp_bool allow_multiple_connections)
{
	if (user == NULL && pass == NULL && !allow_multiple_connections) {
		ftp_i_connection_set_error(c, FTP_EARGUMENTS);
		return FTP_ERROR;
	}

	if (!ftp_i_connection_is_ready(c)) {
		ftp_i_connection_set_error(c, FTP_ENOTREADY);
		return FTP_ERROR;
	}

	if (allow_multiple_connections) {
		if (user != NULL && pass != NULL)
			ftp_i_store_auth(c, user, pass);
		c->_mc_enabled = ftp_btrue;
	}

	if (user != NULL && pass != NULL) {
		ftp_i_set_input_trigger(c, FTP_SIGNAL_LOGGED_IN);
		ftp_i_set_input_trigger(c, FTP_SIGNAL_PASSWORD_REQUIRED);

		ftp_bool remote_error;
		if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CUSER, user, NULL, 0, &remote_error) != FTP_OK) {
			ftp_i_connection_set_error(c,
				(remote_error && c->last_signal == FTP_SIGNAL_NOT_LOGGED_IN) ? FTP_EWRONGAUTH : FTP_EUNEXPECTED);
			return FTP_ERROR;
		}

		if (c->last_signal == FTP_SIGNAL_LOGGED_IN) {
			/* No password required */
			return FTP_OK;
		} else if (c->last_signal == FTP_SIGNAL_PASSWORD_REQUIRED) {
			/* Password required */
			ftp_i_set_input_trigger(c, FTP_SIGNAL_LOGGED_IN);

			if (ftp_i_send_command_and_wait_for_triggers(c, FTP_CPASS, pass, NULL, 0, &remote_error) != FTP_OK) {
				ftp_i_connection_set_error(c,
					(remote_error && c->last_signal == FTP_SIGNAL_NOT_LOGGED_IN) ? FTP_EWRONGAUTH : FTP_EUNEXPECTED);
				return FTP_ERROR;
			}

			if (c->last_signal == FTP_SIGNAL_LOGGED_IN) {
				return FTP_OK;
			} else {
				ftp_i_connection_set_error(c, FTP_EUNEXPECTED);
				return FTP_ERROR;
			}
		} else {
			ftp_i_connection_set_error(c, FTP_EUNEXPECTED);
			return FTP_ERROR;
		}
	}

	return FTP_OK;
}