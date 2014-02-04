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


#ifndef libmftp_ftpfunctions_h
#define libmftp_ftpfunctions_h

#include "libmftp.h"

#ifdef FTP_VERBOSE
#define FTP_LOG(...) printf("*** Info: " __VA_ARGS__)
#define FTP_WARN(...) printf("*** WARNING: " __VA_ARGS__)
#define FTP_ERR(...) printf("*** ERROR: " __VA_ARGS__)
#else
#define FTP_LOG(...)
#define FTP_WARN(...)
#define FTP_ERR(...)
#endif

#define FTP_INTERNAL_SIGNAL_ERROR 1000


#define ftp_i_free(ptr) if (ptr != NULL) { \
	free(ptr); \
	ptr=NULL; \
}

/**
 * Copies memory and null-terminates the destination.
 * @param  dest   The allocated destination.
 * @param  src    The source data.
 * @param  offset Offset of the source data.
 * @param  len    Length of data to copy.
 */
#define ftp_i_memcpy_nulltrm(dest,src,offset,len) do { \
	ftp_i_memcpy(dest,src,offset,len); \
	*(dest+len) = '\0'; \
} while(0)

/**
 * Allocates memory of the appropriate length and then performs ftp_i_memcpy_nulltrm.
 * @param  dest   Will be set to the newly allocated memory or NULL if allocation failed.
 * @param  src    The source data.
 * @param  offset Offset of the source data.
 * @param  len    Length of data to copy.
 */
#define ftp_i_memcpy_nulltrm_malloc(dest,src,offset,len) do { \
	dest = malloc(len + 1); \
	if (dest) \
		ftp_i_memcpy_nulltrm(dest,src,offset,len); \
} while(0)

/**
 * Allocates memory of the appropriate length and then performs strcpy.
 * @param  dest Will be set to the newly allocated memory or NULL if allocation failed.
 * @param  src  The source string.
 */
#define ftp_i_strcpy_malloc(dest,src) do { \
	char *s = (src); \
	dest = (char*)malloc(sizeof(char) * (strlen(s) + 1)); \
	if (dest) \
		strcpy(dest,s); \
} while(0)


#ifdef FTP_TLS_ENABLED
#define ftp_i_open_getsecurity(c) (c->_tls_info ? ftp_security_always : ftp_security_none)
#else
#define ftp_i_open_getsecurity(c) ftp_security_none
#endif

#define ftp_i_is_timed_out(errno) (errno == EAGAIN || errno == EWOULDBLOCK)
#define ftp_i_connection_is_waiting(c) (c->status == FTP_WAITING)
#define ftp_i_connection_set_error(c,err) c->error = err
#define ftp_i_connection_is_down(c) (c->status == FTP_DOWN)
#define ftp_i_last_signal_was_error(con) ftp_i_signal_is_error(con->last_signal)

#if 0
/* For Testing */
#define ftp_i_printf_array(arr,len,format) printf("contents of "#arr":\n");for(int arr_i=0;arr_i<len;arr_i++){printf("%5i "format"\n",arr_i,*(arr+arr_i));}
#define ftp_i_printf_char_values(arr,len) ftp_i_printf_array(arr,len,"%i");
#endif

/*
 * Iterating through a separated string.
 */
#define for_sep(ival,str,sep,block) do { \
	char *input = str; \
	char *ival = NULL; \
	ftp_i_strsep(&input, &ival, (sep)); \
	for (; input || ival; ftp_i_strsep(&input, &ival, (sep))) \
		if (strlen(ival) > 0) block \
	ftp_i_free(ival); \
} while (0);


#define ANSWER_LEN 5000

typedef struct {
	void *buffer;
	unsigned long size;
	unsigned long length;
	unsigned long offset;
} ftp_i_managed_buffer;

typedef struct {
	/* Currently not used: */
	/*unsigned int net_port;
	char *net_addr;*/
	unsigned int tcp_port;
} ftp_i_ex_answer;

FTP_I_BEGIN_DECLS

/*                    Read/Write */
ssize_t               ftp_i_write(ftp_connection *, int, const void *, size_t);
ssize_t               ftp_i_read(ftp_connection *, int, void *, size_t);
ftp_status            ftp_i_set_transfer_type(ftp_connection *, ftp_transfer_type);

/*                    Input Thread */
int                   ftp_i_establish_input_thread(ftp_connection *);
int                   ftp_i_release_input_thread(ftp_connection *);
void                  ftp_i_set_input_trigger(ftp_connection *, int);
ftp_status            ftp_i_wait_for_triggers(ftp_connection *);

/*                    Signal Processing */
extern int            ftp_i_signal_is_error(int);
int                   ftp_i_input_sign(char *);

/*                    Data Connection */
int                   ftp_i_establish_data_connection(ftp_connection *);
int                   ftp_i_prepare_data_connection(ftp_connection *);
void                  ftp_i_close_data_connection(ftp_connection *);

/*                    PASV */
int                   ftp_i_enter_pasv_old(ftp_connection *c);
int                   ftp_i_enter_pasv(ftp_connection *);

/*                    General Parsing */
unsigned int          ftp_i_values_from_comma_separated_string(char *, unsigned int[], unsigned int);
int                   ftp_i_textfrombrackets(char *, char *, int);
ftp_i_ex_answer       ftp_i_interpret_ex_answer(char *, int *);
int                   ftp_i_set_pwd_information(char*, char**);
ftp_date              ftp_i_date_from_string(char *);
#define               ftp_i_date_from_values(y,m,d,h,min,s) ((ftp_date){(y),(m),(d),(h),(min),(s)})
ftp_date              ftp_i_date_from_unix_timestamp(unsigned long);

/*                    Content Listing Parsing */
ftp_content_listing  *ftp_i_mkcontentlisting(void);
ftp_content_listing  *ftp_i_applyclfilter(ftp_content_listing *, int *);
ftp_content_listing  *ftp_i_read_mlsd_answer(ftp_i_managed_buffer *, int *, int *);
ftp_content_listing  *ftp_i_read_list_answer(ftp_i_managed_buffer *, int *, int *);
ftp_bool			  ftp_i_clfilter_keepthis(ftp_content_listing *);
ftp_bool              ftp_i_applyfact(char *, ftp_file_facts *, unsigned long);
ftp_bool              ftp_i_applyfacts(char *, ftp_file_facts *);
ftp_file_type         ftp_i_strtotype(char *str);
int                   ftp_i_unix_mode_from_string(char *, ftp_bool *);

/*                    Managed Buffer */
ftp_i_managed_buffer *ftp_i_managed_buffer_new(void);
#define               ftp_i_managed_buffer_length(buf) (buf->length)
#define               ftp_i_managed_buffer_cbuf(buf) ((char*)((ftp_i_managed_buffer*)buf)->buffer)
ftp_status            ftp_i_managed_buffer_append(ftp_i_managed_buffer *, void *, unsigned long);
#define               ftp_i_managed_buffer_append_str(buf,string) do {void *s = (void*)(string); ftp_i_managed_buffer_append(buf, s, strlen(s));} while(0)
unsigned long         ftp_i_managed_buffer_read(ftp_i_managed_buffer *, void *, unsigned long);
ftp_status            ftp_i_managed_buffer_memcpy(ftp_i_managed_buffer *, const ftp_i_managed_buffer *, unsigned long, unsigned long);
ftp_status            ftp_i_managed_buffer_duplicate(ftp_i_managed_buffer *, const ftp_i_managed_buffer *);
void                  ftp_i_managed_buffer_print(ftp_i_managed_buffer *);
char *                ftp_i_managed_buffer_disassemble(ftp_i_managed_buffer *);
void                  ftp_i_managed_buffer_free(ftp_i_managed_buffer *);

/*                    General */
void                  ftp_i_strsep(char **, char **, const char *);
extern long           ftp_i_seconds_between(struct timeval t1, struct timeval t2);
extern int            ftp_i_char_is_number(char);
extern void           ftp_i_strtolower(char *);

/*                    Memory Management */
void                  ftp_i_memcpy(void *, const void *, size_t, size_t);


#ifdef FTP_TLS_ENABLED

/*                    FTP/TLS */
ftp_status            ftp_i_tls_connect(int, void**, void*, int*);
void                  ftp_i_tls_disconnect(void **tls_info_ptr);
ssize_t               ftp_i_tls_write(void *, const void *, size_t);
ssize_t               ftp_i_tls_read(void *, void *, size_t);

#endif

FTP_I_END_DECLS

#endif
