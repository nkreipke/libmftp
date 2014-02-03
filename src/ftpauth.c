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
#include "ftpfunctions.h"
#include "ftpsignals.h"
#include "ftpcommands.h"

ftp_status ftp_i_store_auth(ftp_connection *c, char *user, char *pass)
{
    ftp_i_free(c->_mc_user);
    ftp_i_free(c->_mc_pass);
    c->_mc_user = (char*)malloc(sizeof(char) * (strlen(user) + 1));
    c->_mc_pass = (char*)malloc(sizeof(char) * (strlen(pass) + 1));
    if (!c->_mc_user || !c->_mc_pass) {
        c->error = FTP_ECOULDNOTALLOCATE;
        return FTP_ERROR;
    }
    strcpy(c->_mc_user, user);
    strcpy(c->_mc_pass, pass);
    return FTP_OK;
}

ftp_status ftp_auth(ftp_connection *c, char *user, char *pass, ftp_bool allow_multiple_connections)
{
    if (user == NULL && pass == NULL && !allow_multiple_connections) {
        c->error = FTP_EARGUMENTS;
        return FTP_ERROR;
    }
    if (c->status != FTP_UP) {
        c->error = FTP_ENOTREADY;
        return FTP_ERROR;
    }
    if (allow_multiple_connections) {
        if (user != NULL && pass != NULL)
            ftp_i_store_auth(c, user, pass);
        c->_mc_enabled = ftp_btrue;
    }
    if (user != NULL && pass != NULL) {
        char cmd[500];
        int r;
        sprintf(cmd, FTP_CUSER " %s" FTP_CENDL, user);
        ftp_i_set_input_trigger(c, FTP_SIGNAL_LOGGED_IN);
        ftp_i_set_input_trigger(c, FTP_SIGNAL_PASSWORD_REQUIRED);
        ftp_send(c, cmd);
        if (ftp_i_wait_for_triggers(c) != FTP_OK)
            return FTP_ERROR;
        if (ftp_i_signal_is_error(c->last_signal))
            if (c->last_signal == FTP_SIGNAL_NOT_LOGGED_IN) {
                c->error = FTP_EWRONGAUTH;
                return FTP_ERROR;
            } else {
                c->error = FTP_EUNEXPECTED;
                return FTP_ERROR;
            }
        if (c->last_signal == FTP_SIGNAL_LOGGED_IN) {
            return FTP_OK;
        }
        else if (c->last_signal == FTP_SIGNAL_PASSWORD_REQUIRED) {
            sprintf(cmd, FTP_CPASS " %s" FTP_CENDL, pass);
            ftp_i_set_input_trigger(c, FTP_SIGNAL_LOGGED_IN);
            ftp_send(c, cmd);
            if (ftp_i_wait_for_triggers(c) != FTP_OK)
                return FTP_ERROR;
            if (ftp_i_signal_is_error(c->last_signal)) {
                if (c->last_signal == FTP_SIGNAL_NOT_LOGGED_IN) {
                    c->error = FTP_EWRONGAUTH;
                    return FTP_ERROR;
                } else {
                    c->error = FTP_EUNEXPECTED;
                    return FTP_ERROR;
                }
            }
            if (c->last_signal == FTP_SIGNAL_LOGGED_IN) {
                return FTP_OK;
            } else {
                c->error = FTP_EUNEXPECTED;
                return FTP_ERROR;
            }
        } else {
            c->error = FTP_EUNEXPECTED;
            return FTP_ERROR;
        }
    }
    return FTP_OK;
}