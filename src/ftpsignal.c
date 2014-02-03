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
#include "ftpfunctions.h"
#include "ftpsignals.h"

/* SEVERAL PARSERS FOR SERVER ANSWERS */

int ftp_i_char_is_number(char chr)
{
    return chr >= '0' && chr <= '9';
}

int ftp_i_input_sign(char *signal)
{
    //Parse server answer to signal.
    char s[4] = {*signal, *(signal+1), *(signal+2), '\0'};
    for (int i = 0; i < 3; i++) {
        //check if this is a valid signal.
        if (!ftp_i_char_is_number(s[i]))
            return FTP_INTERNAL_SIGNAL_ERROR;
    }
    return atoi(s);
}

int ftp_i_signal_is_error(int signal)
{
    return (signal > 399 && signal < 600) || signal == FTP_INTERNAL_SIGNAL_ERROR;
}

int ftp_i_set_pwd_information(char *server_answer, char **destination_malloc)
{
    //This parses the server answer for PWD:
    // "/" is the current directory
    int startchr = -1, endchr = -1, i = 0, len = 0;
    if (*destination_malloc)
        free(*destination_malloc);
    *destination_malloc = malloc(sizeof(char) * ANSWER_LEN);
    if (!*destination_malloc)
        return FTP_ECOULDNOTALLOCATE;
    while (*(server_answer+i) !='\0') {
        if (*(server_answer+i) == '\"') {
            if (startchr == -1)
                startchr = i;
            else if (endchr == -1)
                endchr = i;
            else
                //strange answer
                break;
        }
        i++;
    }
    startchr++;
    endchr--;
    if (startchr > endchr) {
        FTP_ERR("Invalid input.\n");
        return FTP_EUNEXPECTED;
    }
    len = endchr - startchr + 1;
    if (len > ANSWER_LEN - 1) {
        FTP_ERR("Input too long.\n");
        //can not be longer than server_answer
        return FTP_EUNEXPECTED;
    }
    ftp_i_memcpy_nulltrm(*destination_malloc, server_answer, startchr, len);
    return 0;
}
