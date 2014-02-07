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


#ifndef libmftp_ftpcommands_h
#define libmftp_ftpcommands_h

#include "libmftp.h"

FTP_I_BEGIN_DECLS

// Send FTP command: ftp_send(ftpConnection, command)
ftp_status ftp_send(ftp_connection *, char *);
// Commands need to be terminated with FTP_CENDL

FTP_I_END_DECLS

#define FTP_CNL "\n"
#define FTP_CCR "\r"
#define FTP_CENDL FTP_CCR FTP_CNL


#define FTP_CAUTHTLS "AUTH TLS"
#define FTP_CPBSZ "PBSZ"
#define FTP_CPBSZ_NULL "0"
#define FTP_CPROT "PROT"
#define FTP_CPROT_PRIVATE "P"

#define FTP_CUSER "USER"
#define FTP_CPASS "PASS"

#define FTP_CPASV "PASV"
#define FTP_CEPSV "EPSV"

#define FTP_CTYPE "TYPE"
#define FTP_CTYPE_ASCII "A"
#define FTP_CTYPE_BINARY "I"

#define FTP_CPWD "PWD"
#define FTP_CCWD "CWD"
#define FTP_CLIST "LIST"
#define FTP_CMLSD "MLSD"
#define FTP_CSIZE "SIZE"

#define FTP_CSTOR "STOR"
#define FTP_CAPPE "APPE"
#define FTP_CREST "REST"
#define FTP_CRETR "RETR"

#define FTP_CRNFR "RNFR"
#define FTP_CRNTO "RNTO"

#define FTP_CDELE "DELE"
#define FTP_CRMD "RMD"
#define FTP_CMKD "MKD"

#define FTP_CUNIX_CHMOD "SITE CHMOD"

#define FTP_CNOOP "NOOP"
#define FTP_CQUIT "QUIT"

#endif
