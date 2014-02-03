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


#include "ftpfunctions.h"

#ifdef FTP_TLS_ENABLED

#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

struct tls_info {
	SSL_CTX *ctx;
	SSL *ssl;
};
#define tls_info_malloc() calloc(1, sizeof(struct tls_info))
int tls_loaded = 0;
void load_tls(void);

#define FTP_LOGSSL(...) FTP_LOG("$$$ " __VA_ARGS__)

#ifdef FTP_VERBOSE
#define FTP_SSL_log_errors() ERR_print_errors_fp(stderr)
#else
#define FTP_SSL_log_errors()
#endif

#define FTP_SSLRETURNERROR(x) *error=x;if(tls->ctx)SSL_CTX_free(tls->ctx);SSL_free(tls->ssl);free(tls);return FTP_ERROR;


ftp_status ftp_i_tls_connect(int sockfd, void **tls_info_ptr, void *tls_reuse_info_ptr, int *error) {
	load_tls();
	if (!tls_loaded) {
		*error = FTP_ETLS_COULDNOTINIT;
		return FTP_ERROR;
	}
	struct tls_info *tls = tls_info_malloc();
	if (!tls) {
		*error = FTP_ECOULDNOTALLOCATE;
		return FTP_ERROR;
	}

	SSL_CTX *ctx = NULL;
	if (tls_reuse_info_ptr) {
		ctx = ((struct tls_info*)tls_reuse_info_ptr)->ctx;
	}
	if (!ctx) {
		FTP_LOGSSL("new context\n");
		tls->ctx = SSL_CTX_new(SSLv23_client_method());
		if (!tls->ctx) {
			FTP_SSL_log_errors();
			*error = FTP_ETLS_COULDNOTINIT;
			free(tls);
			return FTP_ERROR;
		}
		SSL_CTX_set_timeout(tls->ctx, 600);
		long mode = SSL_CTX_get_session_cache_mode(tls->ctx);
		mode |= SSL_SESS_CACHE_CLIENT;
		SSL_CTX_set_session_cache_mode(tls->ctx, mode);
		ctx = tls->ctx;
	}
	FTP_LOGSSL("new ssl\n");
	tls->ssl = SSL_new(ctx);
	if (!tls->ssl) {
		FTP_SSL_log_errors();
		*error = FTP_ETLS_COULDNOTINIT;
		if (tls->ctx) SSL_CTX_free(tls->ctx);
		free(tls);
		return FTP_ERROR;
	}
	if (!SSL_set_fd(tls->ssl, sockfd)) {
		FTP_SSL_log_errors();
		FTP_SSLRETURNERROR(FTP_ETLS_COULDNOTINIT);
	}
	//reuse session (for data connection as some servers require this):
	if (tls_reuse_info_ptr) {
		FTP_LOGSSL("reusing session!\n");
		struct tls_info *reuse = tls_reuse_info_ptr;
		SSL_SESSION *sess = SSL_get_session(reuse->ssl);
		if (!sess || !SSL_set_session(tls->ssl, sess)) {
			FTP_ERR("session reuse failed.\n");
			FTP_SSLRETURNERROR(FTP_ETLS_COULDNOTINIT);
		}
	}
	FTP_LOGSSL("ssl handshake\n");
	if (SSL_connect(tls->ssl) != 1) {
		FTP_ERR("ssl handshake failed.\n");
		FTP_SSL_log_errors();
		FTP_SSLRETURNERROR(FTP_ETLS_COULDNOTINIT);
	}
	FTP_LOGSSL("ssl handshake successful\n");

	*tls_info_ptr = tls;

	return FTP_OK;
}

void ftp_i_tls_disconnect(void **tls_info_ptr) {
	struct tls_info *tls = *tls_info_ptr;
	if (tls) {
		if (tls->ssl) {
			SSL_shutdown(tls->ssl);
			SSL_free(tls->ssl);
		}
		if (tls->ctx) {
			SSL_CTX_free(tls->ctx);
		}
		free(tls);
	}
	*tls_info_ptr = NULL;
}

ssize_t ftp_i_tls_write(void *tls_info_ptr, const void *buf, size_t len) {
	struct tls_info *tls = tls_info_ptr;
	return (ssize_t)SSL_write(tls->ssl, buf, (int)len);
}

ssize_t ftp_i_tls_read(void *tls_info_ptr, void *buf, size_t len) {
	struct tls_info *tls = tls_info_ptr;
	return (ssize_t)SSL_read(tls->ssl, buf, (int)len);
}


void load_tls(void) {
	if (!tls_loaded) {
		OpenSSL_add_all_algorithms();
		SSL_load_error_strings();
		if (SSL_library_init() < 0) {
			return;
		}
		tls_loaded = 1;
	}
}


#endif /*FTP_TLS_ENABLED*/