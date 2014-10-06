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
#include <ctype.h>
#define __USE_XOPEN
#include <time.h>
#include "ftpinternal.h"

#define TM_YEAR_OFFSET 1900


inline long ftp_i_seconds_between(struct timeval t1, struct timeval t2)
{
	if (t1.tv_sec > t2.tv_sec)
		return t1.tv_sec - t2.tv_sec;
	else
		return t2.tv_sec - t1.tv_sec;
}

void ftp_i_memcpy(void *dest, const void *src, size_t offset, size_t len)
{
	//memcpy with offset
	const unsigned char *in = src;
	unsigned char *out = dest;
	for (int i = 0; i < len; i++)
		*(out+i) = *(in+offset+i);
}

inline void ftp_i_strtolower(char *str)
{
	for(char *c = str; *c; c++)
		*c = tolower(*c);
}

/**
 * Moves an input pointer after the next occurrence of the delimiter string
 * @param input     Input string. Will be altered and can be NULL after this function.
 * @param current   The input string until the delimiter will be copied into here.
 * @param delimiter The delimiter string.
 */
void ftp_i_strsep(char **input, char **current, const char *delimiter)
{
	unsigned long next = 0, delimiter_ptr = 0, current_len;
	char *curstr = *input;
	ftp_bool found_delimiter = ftp_bfalse;

	ftp_i_free(*current);
	*current = NULL;

	if (!curstr)
		return;

	while (curstr[next] != '\0') {
		if (curstr[next] != delimiter[delimiter_ptr])
			delimiter_ptr = 0;
		if (curstr[next] == delimiter[delimiter_ptr])
			delimiter_ptr++;
		if (delimiter[delimiter_ptr] == '\0') {
			found_delimiter = ftp_btrue;
			break;
		}
		next++;
	}
	if (found_delimiter) {
		*input += next + 1;
		current_len = next - strlen(delimiter) + 1;
	} else {
		*input = NULL;
		current_len = next;
	}

	ftp_i_memcpy_nulltrm_malloc(*current, curstr, 0, current_len);
}

unsigned int ftp_i_values_from_comma_separated_string(char *str, unsigned int list[], unsigned int maxlen)
{
	char cur[10];
	int cp = 0, strp = 0, listp = 0;
	while (1) {
		if (*(str+strp) == ',' || *(str+strp) == '\0') {
			cur[cp] = '\0';
			if (listp >= maxlen)
				return maxlen + 1;
			if (strlen(cur) > 0)
				list[listp++] = (unsigned int)strtol(cur, (char **)NULL, 10);
			cp = 0;
			if (*(str+strp) == '\0')
				return listp;
		} else {
			cur[cp++] = *(str+strp);
			if (cp > 9)
				return 0;
		}
		strp++;
	}
	return 0;
}

int ftp_i_textfrombrackets(char *server_answer, char *dest, int maxlen)
{
	int i = 0, inbracket = -1, outbracket = -1;
	//find brackets
	while (*(server_answer+i) != '\0') {
		if (*(server_answer+i) == '(')
			inbracket = i;
		else if (*(server_answer+i) == ')')
			outbracket = i;
		i++;
	}
	if (inbracket == -1 || outbracket == -1)
		return FTP_EUNEXPECTED;
	if (inbracket >= outbracket)
		return FTP_EUNEXPECTED;
	if (outbracket - inbracket > maxlen)
		return FTP_ETOOLONG;
	ftp_i_memcpy_nulltrm(dest, server_answer, inbracket + 1, outbracket - inbracket - 1);
	return 0;
}

ftp_i_ex_answer ftp_i_interpret_ex_answer(char *answer, int *error)
{
	ftp_i_ex_answer f = {0};
	char delimiter[2];
	char *current = NULL;
	delimiter[0] = *answer;
	delimiter[1] = '\0';
	answer++;
	for (int i = 0; i < 3; i++) {
		/*
		 * First two fields are ignored.
		 */
		ftp_i_strsep(&answer, &current, delimiter);
		if (!current) {
			*error = FTP_EUNEXPECTED;
			return f;
		}
	}
	f.tcp_port = (unsigned int)strtol(current, NULL, 10);
	return f;
}

int ftp_i_unix_mode_from_string(char *string, ftp_bool *is_dir)
{
	if (strlen(string) < 10)
		return -1;
	if (is_dir)
		*is_dir = (string[0] == 'd');
	//owner -rwx------
	int modeowner = 0;
	if (string[1] == 'r') modeowner += 4;
	if (string[2] == 'w') modeowner += 2;
	if (string[3] == 'x') modeowner += 1;
	//group ----rwx---
	int modegroup = 0;
	if (string[4] == 'r') modegroup += 4;
	if (string[5] == 'w') modegroup += 2;
	if (string[6] == 'x') modegroup += 1;
	//other -------rwx
	int modeother = 0;
	if (string[7] == 'r') modeother += 4;
	if (string[8] == 'w') modeother += 2;
	if (string[9] == 'x') modeother += 1;

	return (modeowner * 100) + (modegroup * 10) + modeother;
}

/**
 * Parses a MLSD date response in the format YYYYMMDDHHMMSS(.sss)
 * @param  str 14 characters of MLSD date
 * @return     The date.
 */
ftp_date ftp_i_date_from_string(char *str)
{
	if (strlen(str) < 14) {
		FTP_WARN("MLSD date fact is not in the appropriate format.\n");
		return ftp_i_date_from_values(0, 0, 0, 0, 0, 0);
	}
	char year[5], month[3], day[3], hour[3], minute[3], second[3];
	ftp_i_memcpy_nulltrm(year, str, 0, 4);
	ftp_i_memcpy_nulltrm(month, str, 4, 2);
	ftp_i_memcpy_nulltrm(day, str, 6, 2);
	ftp_i_memcpy_nulltrm(hour, str, 8, 2);
	ftp_i_memcpy_nulltrm(minute, str, 10, 2);
	ftp_i_memcpy_nulltrm(second, str, 12, 2);
	unsigned int y, m, d, h, min, s;
	y = (unsigned int)strtol(year, NULL, 10);
	m = (unsigned int)strtol(month, NULL, 10);
	d = (unsigned int)strtol(day, NULL, 10);
	h = (unsigned int)strtol(hour, NULL, 10);
	min = (unsigned int)strtol(minute, NULL, 10);
	s = (unsigned int)strtol(second, NULL, 10);
	return ftp_i_date_from_values(y, m, d, h, min, s);
}

static inline time_t convert_timestamp_to_time_t(unsigned long timestamp)
{
	/*
	 * This function may or may not be heavily platform dependent as the
	 * implementation of time_t can vary. If you have a better idea on how to
	 * do this, please let me know.
	 */
	return (time_t)timestamp;
}

ftp_date ftp_i_date_from_unix_timestamp(unsigned long ts)
{
	time_t timestamp = convert_timestamp_to_time_t(ts);
	struct tm *local = localtime(&timestamp);
	if (!local)
		return ftp_i_date_from_values(0,0,0,0,0,0);
	return ftp_i_date_from_values(local->tm_year + TM_YEAR_OFFSET, local->tm_mon, local->tm_mday, local->tm_hour, local->tm_min, local->tm_sec);
}

#pragma mark - Managed Buffer

// FTP_I_MANAGED_BUFFER:

ftp_i_managed_buffer *ftp_i_managed_buffer_new(void)
{
	ftp_i_managed_buffer *buf = (ftp_i_managed_buffer*)malloc(sizeof(ftp_i_managed_buffer));
	if (!buf)
		return NULL;
	buf->offset = 0;
	buf->length = 0;
	buf->size = 1000; //initial buffer size
	buf->buffer = malloc(buf->size);
	if (!buf->buffer) {
		free(buf);
		return NULL;
	}
	*(char*)(buf->buffer) = 0;
	return buf;
}

ftp_status ftp_i_managed_buffer_append(ftp_i_managed_buffer *buf, void *data, unsigned long length)
{
	if (!buf)
		return FTP_ERROR;
	if (buf->length + length + 1 > buf->size) {
		//we create 1000 bytes extra so realloc is used less often
		unsigned long newsiz = buf->length + length + 1000;
		void *newbuf = realloc(buf->buffer, newsiz);
		if (!newbuf)
			return FTP_ERROR;
		buf->buffer = newbuf;
		buf->size = newsiz;
	}
	unsigned long lo = 0;
	unsigned char *in = (unsigned char*)data, *out = (unsigned char*)buf->buffer;
	while (lo < length)
		*(out + (buf->offset++)) = *(in + (lo++));
	*(out + buf->offset) = 0; //buffer is always null-terminated
	buf->length += length;
	return FTP_OK;
}

unsigned long ftp_i_managed_buffer_read(ftp_i_managed_buffer *buf, void *data, unsigned long preferred_length)
{
	unsigned char *out = (unsigned char*)data, *in = (unsigned char*)buf->buffer;
	unsigned long lo = 0;
	while (lo < preferred_length && buf->offset < buf->length)
		*(out + (lo++)) = *(in + (buf->offset++));
	return lo;
}

ftp_bool ftp_i_managed_buffer_contains_str(ftp_i_managed_buffer *buf, char *str, ftp_bool startswith)
{
	char *bufs = ftp_i_managed_buffer_cbuf(buf);
	unsigned long current = 0;

	while (*bufs)
	{
		if (*bufs == str[current]) {
			current++;
		} else {
			if (startswith)
				return ftp_bfalse;
			current = 0;
		}

		if (!str[current])
			return ftp_btrue;

		bufs++;
	}

	return ftp_bfalse;
}

ftp_status ftp_i_managed_buffer_memcpy(ftp_i_managed_buffer *dest, const ftp_i_managed_buffer *src, unsigned long offset, unsigned long length)
{
	if (src->length < offset + length)
		return FTP_ERROR;
	return ftp_i_managed_buffer_append(dest, src->buffer + offset, length);
}

ftp_status ftp_i_managed_buffer_duplicate(ftp_i_managed_buffer *dest, const ftp_i_managed_buffer *src)
{
	if (dest->offset != 0) {
		FTP_ERR("BUG: ftp_i_managed_buffer_duplicate requires an empty buffer as destination.\n");
		return FTP_ERROR;
	}
	return ftp_i_managed_buffer_memcpy(dest, src, 0, src->length);
}

void ftp_i_managed_buffer_print(ftp_i_managed_buffer *buf, ftp_bool ignore_newlines)
{
	unsigned char *b = buf->buffer;
	unsigned long l = 0;
	while (l < buf->length) {
		if (!ignore_newlines ||
			(*(b+l) != CHAR_CR && *(b+l) != CHAR_LF))
			printf("%c",*(b+l));

		l++;
	}
	printf("\n");
}

/**
 * Frees the buffer stucture but not the data store.
 * @param  buf The buffer to disassemble.
 * @return     A pointer to the raw data store. It will be null-terminated and
 *             has to be freed using ftp_i_free.
 */
char *ftp_i_managed_buffer_disassemble(ftp_i_managed_buffer *buf)
{
	char *b = buf->buffer;
	ftp_i_free(buf);
	return b;
}

void ftp_i_managed_buffer_release(ftp_i_managed_buffer *buf)
{
	if (!buf)
		return;
	ftp_i_free(buf->buffer);
	ftp_i_free(buf);
}
