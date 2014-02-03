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
#include "libmftp.h"
#include <string.h>

void getdata(char *, char *, char *, char *);
void libmftp_main_test(char *host, unsigned int port, char *user, char *pw, char *workingdirectory);
void libmftp_tls_test(char *host, unsigned int port, char *user, char *pw, char *workingdirectory);


int tls = 0;

int main (int argc, const char * argv[])
{
    char user[100], pw[100], workingdir[500], host[500];
    getdata(user, pw, workingdir, host);

    int port = FTP_STANDARD_PORT;

    tls = 0;

    if (argc == 2) {
        if (strcmp(argv[1],"tls") == 0) {
            // SMALL TLS TEST
            libmftp_tls_test(host, port, user, pw, workingdir);
            return 0;
        }
        if (strcmp(argv[1], "fulltls") == 0)
            // FULL TLS TEST
            tls = 1;
    }


    libmftp_main_test(host, port, user, pw, workingdir);

    return 0;
}

void libmftp_main_test(char *host, unsigned int port, char *user, char *pw, char *workingdirectory)
{
    int success = 0;
    ftp_file *f = NULL, *g = NULL;
    ftp_content_listing *cl = NULL, *cl2 = NULL;
    ftp_date d;
    char *buf = NULL;
    ftp_connection *c = ftp_open(host, port, tls == 0 ? ftp_security_none : ftp_security_always);

    /*c->_current_features->use_mlsd = ftp_bfalse;*/

    if (!c) {
        printf("Could not connect. Error: %i\n",ftp_error);
        if (ftp_error == FTP_ESECURITY) {
            printf("This server does not support a TLS connection. FTP_ESECURITY\n");
        }
        goto end;
    }

    if (ftp_auth(c, user, pw, ftp_btrue) != FTP_OK) {
        printf("Could not authenticate. Error: %i\n", c->error);
        goto end;
    }

    if (ftp_change_cur_directory(c, workingdirectory) != FTP_OK) {
        printf("Could not cwd. Error: %i\n", c->error);
        goto end;
    }

    if (ftp_reload_cur_directory(c) != FTP_OK) {
        printf("Could not reload wd. Error: %i\n", c->error);
        goto end;
    }

    if (strcmp(workingdirectory, c->cur_directory) != 0) {
        printf("Warning: Working dir (%s) does not match ftp dir (%s)! Continue? (y/n)\n",workingdirectory,c->cur_directory);
        if (getchar() != 'y') {
            goto end;
        }
    }

    char *test = "This is a test string that will be written to the server.";
    size_t test_len = strlen(test);

    //TEST UPLOAD 1

    f = ftp_fopen(c, "testfile.test", FTP_WRITE, 0);
    if (!f) {
        printf("Could not fopen to write. Error: %i\n", c->error);
        goto end;
    }

    if (ftp_fwrites(test, f) != test_len) {
        printf("Could not write test string. Error: %i\n", *(f->error));
        goto end;
    }

    ftp_fclose(f);
    f = NULL;

    //TEST CONTENT LISTING

    int entry_count;
    cl = ftp_contents_of_directory(c, &entry_count);

    if (!cl) {
        printf("Could not get content listing. Error: %i\n", c->error);
        goto end;
    }

    if (entry_count != 1 || cl->next) {
        printf("More than 1 entry in content listing.\n");
        goto end;
    }

    if (!ftp_item_exists_in_content_listing(cl, "testfile.test", &cl2)) {
        printf("Could not find previously generated file in content listing.\n");
        goto end;
    }

    if (!cl2) {
        printf("Did not get a pointer to the content listing entry.\n");
        goto end;
    }

    if (!cl2->facts.given.modify) {
        printf("Warning: Remote host does not provide modification date information! Continue? (y/n)\n");
        if (getchar() != 'y') {
            goto end;
        }
    } else {
        time_t cur = time(NULL);
        struct tm *curtm = localtime(&cur);
        if (curtm->tm_year + 1900 != cl2->facts.modify.year) {
            printf("Warning: Modification year of remote file (%i) does not equal local year (%i). Continue? (y/n)\n",cl2->facts.modify.year,curtm->tm_year + 1900);
            if (getchar() != 'y') {
                goto end;
            }
        }
    }

    //TEST SIZE

    size_t srv_size;
    if (ftp_size(c, "testfile.test", &srv_size) != FTP_OK) {
        printf("Could not get file size. Error: %i\n", c->error);
        goto end;
    }

    if (srv_size != test_len) {
        printf("Remote file size (%lu) differs from local file size (%lu).\n",srv_size,test_len);
        goto end;
    }

    //TEST READ

    f = ftp_fopen(c, "testfile.test", FTP_READ, 0);
    if (!f) {
        printf("Could not fopen to read. Error: %i\n", c->error);
        goto end;
    }

    buf = malloc(srv_size + 1);

    if (ftp_fread(buf, 1, srv_size, f) != srv_size) {
        printf("Could not read file. Error: %i\n", *(f->error));
        goto end;
    }

    ftp_fclose(f);
    f = NULL;

    *(buf+srv_size) = '\0';
    if (strcmp(test, buf) != 0) {
        printf("Remote file content differs from local file content.\n");
        goto end;
    }

    //TEST UPLOAD 2 (SIMULTANEOUS)

    f = ftp_fopen(c, "testfile1.txt", FTP_WRITE, 0);
    if (!f) {
        printf("Could not fopen to write 2 (1). Error: %i\n", c->error);
        goto end;
    }
    g = ftp_fopen(c, "testfile2.txt", FTP_WRITE, 0);
    if (!g) {
        printf("Could not fopen to write 2 (2). Error: %i\n", c->error);
        goto end;
    }
    if (ftp_fwrites(test, f) != test_len) {
        printf("Could not write test string 2 (1). Error: %i\n", *(f->error));
        goto end;
    }
    if (ftp_fwrites(test, g) != test_len) {
        printf("Could not write test string 2 (2). Error: %i\n", *(g->error));
        goto end;
    }
    ftp_fclose(f);
    ftp_fclose(g);
    f = g = NULL;

    //TEST SIZE 2

    size_t srv_size2;
    if (ftp_size(c, "testfile1.txt", &srv_size) != FTP_OK) {
        printf("Could not get file size 2 (1). Error: %i\n", c->error);
        goto end;
    }
    if (ftp_size(c, "testfile2.txt", &srv_size2) != FTP_OK) {
        printf("Could not get file size 2 (2). Error: %i\n", c->error);
        goto end;
    }
    if (srv_size != test_len || srv_size2 != test_len) {
        printf("Remote file size 2 differs from local file size.\n");
        goto end;
    }

    //TEST FOLDERS

    if (ftp_create_folder(c, "testfolder") != FTP_OK) {
        printf("Could not create folder. Error: %i\n", c->error);
        goto end;
    }

    if (ftp_move(c, "testfile1.txt", "testfolder/testfile1.txt")) {
        printf("Could not move file. Error: %i\n", c->error);
        goto end;
    }

    if (ftp_change_cur_directory(c, "testfolder") != FTP_OK) {
        printf("Could not cwd 2. Error: %i\n", c->error);
        goto end;
    }

    if (!ftp_item_exists(c, "testfile1.txt", NULL)) {
        printf("File does not exist where it was moved to.");
        goto end;
    }

    //TEST DELETE

    if (ftp_delete(c, "testfile1.txt", ftp_bfalse) != FTP_OK) {
        printf("Could not delete file. Error: %i\n", c->error);
        goto end;
    }

    if (ftp_item_exists(c, "testfile1.txt", NULL)) {
        printf("Deleted file still exists.");
        goto end;
    }

    if (ftp_change_cur_directory(c, workingdirectory) != FTP_OK) {
        printf("Could not cwd 3. Error: %i\n", c->error);
        goto end;
    }

    if (ftp_delete(c, "testfolder", ftp_btrue) != FTP_OK) {
        printf("Could not delete folder. Error: %i\n", c->error);
        goto end;
    }

    if (ftp_item_exists(c, "testfolder", NULL)) {
        printf("Deleted folder still exists.");
        goto end;
    }

    if (ftp_delete(c, "testfile.test", ftp_bfalse) != FTP_OK || ftp_delete(c, "testfile2.txt", ftp_bfalse) != FTP_OK) {
        printf("Could not clean up. Error: %i\n", c->error);
        goto end;
    }

    success = 1;
end:
    if (cl) ftp_free(cl);
    if (f) ftp_fclose(f);
    if (g) ftp_fclose(g);
    if (c) ftp_close(c);
    if (buf) free(buf);
    if (!success) {
        printf("Test was NOT successful. :-(\n");
    } else {
        printf("Test was successful! :-)\n");
    }
}

void libmftp_tls_test(char *host, unsigned int port, char *user, char *pw, char *workingdirectory)
{
    ftp_connection *c = ftp_open(host, port, ftp_security_always);
    if (!c) {
        printf("No TLS connection!\n");
        return;
    }

    if (ftp_auth(c, user, pw, ftp_btrue) != FTP_OK) {
        printf("Could not authenticate. Error: %i\n", c->error);
    }

    if (ftp_item_exists(c, "httpdocs", NULL)) {
        printf("exists!°!11e3degvrbnm\n");
    }

    ftp_close(c);
}

/*
 * If you test this stuff 100 times a day like me, you may want to create an libmftplogin file
 * at /usr/libmftp/libmftplogin. The test program will then use the credentials in this file.
 *
 * Format:
 * <hostname>
 * <workingdirectory>
 * <username>
 * <password>
 *
 */
void getdata(char *user, char *pw, char *workingdir, char *host)
{
    FILE *f = fopen("/usr/libmftp/libmftplogin", "r");
    if (!f) {
        goto inp;
    }
    if (fgets(host, 500, f) != host) {
        goto inp;
    }
    if (fgets(workingdir, 500, f) != workingdir) {
        goto inp;
    }
    if (fgets(user, 100, f) != user) {
        goto inp;
    }
    if (fgets(pw, 100, f) != pw) {
        goto inp;
    }
    for (char *q = host; *q; q++) if (*q == '\n') *q = '\0';
    for (char *q = workingdir; *q; q++) if (*q == '\n') *q = '\0';
    for (char *q = user; *q; q++) if (*q == '\n') *q = '\0';
    for (char *q = pw; *q; q++) if (*q == '\n') *q = '\0';
    goto closep;
inp:
    printf("Enter host:\n");
    scanf("%s",host);
    printf("Enter working directory (for example /libmftp):\n");
    scanf("%s",workingdir);
    printf("Enter user:\n");
    scanf("%s",user);
    printf("\nEnter pw:\n");
    scanf("%s",pw);
closep:
    if (f) fclose(f);
    return;
}
