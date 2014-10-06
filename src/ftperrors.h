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


#ifndef libmftp_ftperrors_h
#define libmftp_ftperrors_h

typedef unsigned char ftp_error;


#define FTP_ESOCKET                 1
#define FTP_ECOULDNOTALLOCATE       2
#define FTP_ECOULDNOTOPENSOCKET     3
#define FTP_EHOST                   4
#define FTP_ECONNECTION             5
#define FTP_ENOSERVICE              6
#define FTP_EWRONGAUTH              7
#define FTP_ESECURITY               8
#define FTP_ETHREAD                 9

#define FTP_ENOTREADY              10
#define FTP_ETIMEOUT               11

#define FTP_EWRITE                 20

#define FTP_EUNEXPECTED           100
#define FTP_ETOOLONG              101
#define FTP_ENOTPERMITTED         102
#define FTP_ENOTFOUND             103
#define FTP_ENOTFOUND_OR_NOTEMPTY 104
#define FTP_EINVALID              105
#define FTP_ESERVERCAPABILITIES   106

#define FTP_EALREADY              110
#define FTP_EARGUMENTS            112
#define FTP_ENOTSUPPORTED         118


#define FTP_ETLS_COULDNOTINIT     200
#define FTP_ETLS_CERTIFICATE      201

#endif
