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


#ifndef libmftp_ftpdefinitions_h
#define libmftp_ftpdefinitions_h

#include "ftperrors.h"

/* ftp_status values for ftp_connection->status: */
#define FTP_DOWN             0
#define FTP_UP               1
#define FTP_CONNECTING       2
#define FTP_WAITING          3

#define FTP_STANDARD_PORT    21

#define CHAR_CR '\r'
#define CHAR_LF '\n'

typedef struct _ftp_connection ftp_connection;

typedef char                 ftp_status;
typedef unsigned char        ftp_activity;

#ifdef __cplusplus
typedef bool                 ftp_bool;
#define ftp_bfalse false
#define ftp_btrue true
#else
typedef unsigned char        ftp_bool;
enum ftp_bools {
	ftp_bfalse = 0,
	ftp_btrue = 1
};
#endif

struct ftp_features {
	ftp_bool use_epsv;
	ftp_bool use_mlsd;
};

#ifdef FTP_PERM_ENABLED
typedef struct {
	ftp_bool a_canappend:1;
	ftp_bool c_cancreatechilditems:1;
	ftp_bool d_canbedeleted:1;
	ftp_bool e_canenter:1;
	ftp_bool f_canrename:1;
	ftp_bool l_canlistcontents:1;
	ftp_bool m_canmkd:1;
	ftp_bool p_candeletecontents:1;
	ftp_bool r_canretrieve:1;
	ftp_bool w_canstore:1;
} ftp_perm;
#endif

typedef enum {
	ftp_tt_undefined,
	ftp_tt_ascii,
	ftp_tt_binary
} ftp_transfer_type;

#ifndef FTP_I_DECLS
#define FTP_I_DECLS
#ifdef __cplusplus
#define FTP_I_BEGIN_DECLS extern "C" {
#define FTP_I_END_DECLS }
#else
#define FTP_I_BEGIN_DECLS
#define FTP_I_END_DECLS
#endif
#endif

#if (__STDC_VERSION__ >= 201112L)
#define SUPPORTS_GENERICS
#endif


#endif /* libmftp_ftpdefinitions_h */