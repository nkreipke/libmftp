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
#include "ftpfunctions.h"
#include "ftpcommands.h"
#include "ftpparse.h"

ftp_content_listing *ftp_i_mkcontentlisting(void)
{
    ftp_content_listing *c = calloc(1,sizeof(ftp_content_listing));
    if (!c)
        return NULL;
    bzero((void*)c, sizeof(ftp_content_listing));
    return c;
}

void ftp_free(ftp_content_listing *cl)
{
    if (cl->next)
        ftp_free(cl->next);
    ftp_i_free(cl->filename);
    ftp_i_free(cl);
}

ftp_content_listing *ftp_i_applyclfilter(ftp_content_listing *c, int *items_count)
{
    int itemscount = *items_count;
    ftp_content_listing *start = c;
    ftp_content_listing *cur = c;
    ftp_content_listing *previous = NULL;
    while (cur) {
        if (cur->facts.given.type &&
            cur->facts.type != ft_file &&
            cur->facts.type != ft_dir) {
            //delete this item
            if (previous) {
                previous->next = cur->next;
                cur->next = NULL;
                ftp_free(cur);
                cur = previous->next;
            } else {
                start = cur->next;
                cur->next = NULL;
                ftp_free(cur);
                cur = start;
            }
            itemscount--;
        } else {
            previous = cur;
            cur = cur->next;
        }
    }
    *items_count = itemscount;
    return start;
}

ftp_file_type ftp_i_strtotype(char *str)
{
    strlower(str);
    if (strcmp(str, "file") == 0)
        return ft_file;
    else if (strcmp(str, "dir") == 0)
        return ft_dir;
    else
        return ft_other;
}

ftp_bool ftp_i_applyfact(char *fact, ftp_file_facts *facts, unsigned long factlen)
{
    char key[100], value[100];
    unsigned long p = 0;
    //FTP_LOG("Got file fact %s\n", fact);
    //search for =
    while (*(fact+p) != '=') {
        if (*(fact+p) == '\0')
            // this is a malformed fact reply, abort parsing.
            return ftp_bfalse;
        p++;
    }
    strcpy(value, fact+p+1);
    *(fact+p) = '\0';
    strcpy(key, fact);
    strlower(key);
    unsigned long vlen = factlen - p - 1;
    //interpret key
    if (strcmp(key, "size") == 0) {
        facts->size = strtoul(value, (char**)NULL, 10);
        facts->given.size = 1;
    } else if (strcmp(key, "modify") == 0) {
        if (vlen < 14) return ftp_bfalse;
        facts->modify = ftp_i_date_from_string(value);
        facts->given.modify = 1;
    } else if (strcmp(key, "create") == 0) {
        if (vlen < 14) return ftp_bfalse;
        facts->create = ftp_i_date_from_string(value);
        facts->given.create = 1;
    } else if (strcmp(key, "type") == 0) {
        facts->type = ftp_i_strtotype(value);
        facts->given.type = 1;
    } else if (strcmp(key, "unix.group") == 0) {
        facts->unixgroup = (unsigned int)strtol(value, (char**)NULL, 10);
        facts->given.unixgroup = 1;
    } else if (strcmp(key, "unix.mode") == 0) {
        facts->unixmode = (unsigned int)strtol(value, (char**)NULL, 10);
        facts->given.unixmode = 1;
    }
    return ftp_btrue;
}

ftp_bool ftp_i_applyfacts(char *factlist, ftp_file_facts *facts)
{
    for_sep(fs, factlist, ";", {
        unsigned long len = strlen(fs);
        if (len == 0)
            continue;
        if (!ftp_i_applyfact(fs, facts, len)) {
            ftp_i_free(fs);
            return ftp_bfalse;
        }
    });

    return ftp_btrue;
}

ftp_content_listing *ftp_i_read_mlsd_answer(ftp_i_managed_buffer *buffer, int *items_count)
{
    int itemscount = 0;
    ftp_content_listing *start = NULL, *current = NULL;
/*#ifdef FTP_SERVER_VERBOSE
#ifdef FTP_DEBUG
    printf("*** DEBUG: searching for \\n \\r and \\r\\n\n");
    char *tmp = (char*)buffer->buffer;
    for (int i = 0; *tmp; tmp++, i++)
    {
        if (*tmp == '\r')
            printf("*** DEBUG: found \\r at %i\n",i);
        if (*tmp == '\n')
            printf("*** DEBUG: found \\n at %i\n",i);
        if (*tmp == '\r' && *(tmp+1) == '\n')
            printf("*** DEBUG: found CRLF at %i\n",i);
    }
#endif
#endif*/
#ifdef FTP_CONTENTLISTING_VERBOSE
    printf("Content Listing Raw data following. -------------\n");
    ftp_i_managed_buffer_print(buffer);
#endif

    for_sep(fs, (char*)buffer->buffer, FTP_CNL, {
        if (strlen(fs) == 0)
            continue;
        int i = 0;
        while (*(fs+i) != ' ') {
            if (*(fs+i) == '\0')
                continue;
            i++;
        }
        if (!current) {
            current = ftp_i_mkcontentlisting();
            start = current;
        } else {
            current->next = ftp_i_mkcontentlisting();
            current = current->next;
        }
        if (!current) {
            ftp_i_free(fs);
            return NULL;
        }
        char *filename = fs + i + 1;
        char server_used_wrong_separator = 1;
        for (char *ptr = filename; *ptr; ptr++) {
            /* instead of using \r\n to separate lines, some servers use \n (which is against specification
             * but whatever). this is a fix for this. */
            if (*ptr == '\r' && *(ptr+1) == '\0') {
                server_used_wrong_separator = 0;
                *ptr = '\0';
            }
        }
        if (server_used_wrong_separator)
            FTP_WARN("[MLSD] feature does not comply with specification: uses \\n instead of \\r\\n\n");
        ftp_i_strcpy_malloc(current->filename, fs + i + 1);
        *(fs+i-1) = '\0';
        if (!ftp_i_applyfacts(fs,&(current->facts))) {
            FTP_ERR("[MLSD] Invalid answer.\n");
            ftp_free(start);
            ftp_i_free(fs);
            return NULL;
        }
        itemscount++;
    });

    *items_count = itemscount;
    FTP_LOG("parsed %i mlsd entries\n",itemscount);
    return start;
}

ftp_content_listing *ftp_i_read_list_answer(ftp_i_managed_buffer *buffer, int *items_count)
{
#ifndef FTPPARSE_H
    *items_count = 1;
    return NULL;
#endif
    int itemscount = 0;
    ftp_content_listing *start = NULL, *current = NULL;
    ftp_i_managed_buffer *filename_buffer;
    //LIST is supported for compatibility reasons.
    //this uses ftpparse (http://cr.yp.to/ftpparse.html) by D. J. Bernstein.
    char *buf = buffer->buffer;
    //i don't know whether there are servers that terminate lines with LF only,
    //so this will check it.
    int p = 0;
#ifdef FTP_CONTENTLISTING_VERBOSE
    printf("Content Listing Raw data following. -------------\n");
    ftp_i_managed_buffer_print(buffer);
#endif
    while (*(buf+p) != '\n') {
        if (*(buf+p) == '\0')
            //this is only one line, so it does not matter.
            break;
        p++;
    }
    char *sep = (*(buf+(p-1)) == '\r') ? "\r\n" : "\n";

    for_sep(fs, buf, sep, {
        ssize_t len = strlen(fs);
        if (len == 0)
            continue;
        struct ftpparse fp;
        if (ftpparse(&fp, fs, (int)len)) {
            if (!current) {
                current = ftp_i_mkcontentlisting();
                start = current;
            } else {
                current->next = ftp_i_mkcontentlisting();
                current = current->next;
            }
            if (!current) {
                ftp_i_free(fs);
                return NULL;
            }
            filename_buffer = ftp_i_managed_buffer_new();
            ftp_i_managed_buffer_append(filename_buffer, (void*)fp.name, fp.namelen);
            current->filename = ftp_i_managed_buffer_disassemble(filename_buffer);
            if (fp.unix_permissions[0] != 0) {
                ftp_bool dir;
                current->facts.unixgroup = ftp_i_unix_mode_from_string(fp.unix_permissions, &dir);
                current->facts.type = dir ? ft_dir : ft_file;
            } else {
                //can only guess type
                current->facts.type = fp.flagtrycwd ? ft_dir : ft_file;
            }
            /*current->facts.modify.type = fp.mtimetype;
            if (current->facts.modify.type != tt_notgiven) {
                current->facts.modify.time.timedate = fp.mtime;
            }*/
            current->facts.size = (unsigned long)fp.size;
            if (fp.mtime_given)
            {
                current->facts.modify = fp.mtime;
                current->facts.given.modify = ftp_btrue;
            }
            itemscount++;
        }
    });

    *items_count = itemscount;
    FTP_LOG("parsed %i list entries\n",itemscount);
    return start;
}
