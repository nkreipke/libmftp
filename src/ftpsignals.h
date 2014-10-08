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


#ifndef libmftp_ftpsignals_h
#define libmftp_ftpsignals_h

#define FTP_SIGNAL_DATA_CONNECTION_OPEN_STARTING_TRANSFER 125
#define FTP_SIGNAL_ABOUT_TO_OPEN_DATA_CONNECTION 150

#define FTP_SIGNAL_COMMAND_OKAY 200
#define FTP_SIGNAL_FILE_STATUS 213
#define FTP_SIGNAL_SERVICE_READY 220
#define FTP_SIGNAL_GOODBYE 221
#define FTP_SIGNAL_TRANSFER_COMPLETE 226
#define FTP_SIGNAL_ENTERING_PASSIVE_MODE 227
#define FTP_SIGNAL_ENTERING_EXTENDED_PASSIVE_MODE 229
#define FTP_SIGNAL_LOGGED_IN 230
#define FTP_SIGNAL_TLS_SUCCESSFUL 234
#define FTP_SIGNAL_REQUESTED_ACTION_OKAY 250
#define FTP_SIGNAL_MKDIR_SUCCESS_OR_PWD 257

#define FTP_SIGNAL_PASSWORD_REQUIRED 331
#define FTP_SIGNAL_REQUEST_FURTHER_INFORMATION 350

#define FTP_SIGNAL_REQUESTED_ACTION_ABORTED 451

#define FTP_SIGNAL_NOT_LOGGED_IN 530
#define FTP_SIGNAL_FILE_ERROR 550 /* generally not found or permission denied */

#endif
