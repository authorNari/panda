/*	PANDA -- a simple transaction monitor

Copyright (C) 2004 Ogochan & JMA (Japan Medical Association).

This module is part of PANDA.

	PANDA is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
to anyone for the consequences of using it or for whether it serves
any particular purpose or works at all, unless he says so in writing.
Refer to the GNU General Public License for full details. 

	Everyone is granted permission to copy, modify and redistribute
PANDA, but only under the conditions described in the GNU General
Public License.  A copy of this license is supposed to have been given
to you along with PANDA so you can know your rights and
responsibilities.  It should be in a file named COPYING.  Among other
things, the copyright notice and this notice must be preserved on all
copies. 
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<unistd.h>
#include	<glib.h>
#include	<errno.h>
#include	<iconv.h>

#include	"const.h"
#include	"types.h"
#include	"libmondai.h"
#include	"multipart.h"

#define STR_LITERAL_LENGTH(x) (sizeof(x) - 1)
#define MAX_BOUNDARY_SIZE 70
#define BOUNDARY_NONE 0
#define BOUNDARY_DELIMITER 1
#define BOUNDARY_CLOSE_DELIMITER 2

char *
GetMultipartBoundary(char *content_type)
{
    char *p;

    if (content_type == NULL ||
        strncasecmp(content_type, "multipart/form-data",
                    STR_LITERAL_LENGTH("multipart/form-data")) != 0)
        return NULL;
    p = content_type + STR_LITERAL_LENGTH("multipart/form-data");
    while (*p != '\0' && !isalpha(*p)) p++;
    if (strncasecmp(p, "boundary=", STR_LITERAL_LENGTH("boundary=")) == 0) {
        p += STR_LITERAL_LENGTH("boundary=");
        if (strlen(p) > MAX_BOUNDARY_SIZE) {
            return NULL;
        }
        return StrDup(p);
    }
    else {
        return NULL;
    }
}

static int
CheckBoundary(char *line, char *delimiter, char *close_delimiter)
{
    if (strcmp(line, delimiter) == 0) {
        return BOUNDARY_DELIMITER;
    }
    if (strcmp(line, close_delimiter) == 0) {
        return BOUNDARY_CLOSE_DELIMITER;
    }
    return BOUNDARY_NONE;
}

static char *
ParseValue(char **s)
{
    char *p = *s, *end_p, *rp;
    char *result;
    int len;

    if (*p == '"') {
        p++;
        if ((end_p = strchr(p, '"')) == NULL) {
            return NULL;
        }
        len = end_p - p;
        result = xmalloc(len + 1);
        strncpy(result, p, len);
        result[len] = '\0';
        *s = end_p + 1;
        return result;
    }
    else {
        result = rp = xmalloc(strlen(p));
        while (1) {
            if (isspace(*p) || iscntrl(*p)) {
                break;
            }
            switch (*p) {
            case '(':
            case ')':
            case '<':
            case '>':
            case '@':
            case ',':
            case ';':
            case ':':
            case '\\':
            case '"':
            case '/':
            case '[':
            case ']':
            case '?':
            case '=':
                goto end;
            }
            *rp++ = *p++;
        }
      end:
        *rp = '\0';
        *s = p;
        return result;
    }
}

static char *
ParseParameter(char **s, char *name)
{
    char *p = *s, *value;
    int len = strlen(name);

    if (strncmp(p, name, len) == 0 &&
        p[len] == '=') {
        p += len + 1;
        *s = p;
        return ParseValue(s);
    }
    else {
        return NULL;
    }
}

static int
ParseHeader(FILE *fp, char **name, char **filename)
{
    char buf[SIZE_BUFF];
    int in_content_disposition = 0;

    *name = NULL;
    *filename = NULL;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *p;
        if (strcmp(buf, "\r\n") == 0) break;
        if (strncasecmp(buf, "Content-Disposition:",
                        STR_LITERAL_LENGTH("Content-Disposition:")) == 0) {
            p = buf + STR_LITERAL_LENGTH("Content-Disposition:");
            in_content_disposition = 1;
        }
        else if (!isspace(*buf)) {
            in_content_disposition = 0;
            continue;
        }
        else {
            p = buf;
        }

        if (in_content_disposition) {
            while (isspace(*p)) p++;
            if (strncmp(p, "form-data",
                        STR_LITERAL_LENGTH("form-data")) != 0)
                break;
            p += STR_LITERAL_LENGTH("form-data");
            while (isspace(*p)) p++;
            if (*p != ';') return -1;
            p++;
            while (1) {
                char *value;
                
                while (isspace(*p)) p++;
                if ((value = ParseParameter(&p, "name")) != NULL) {
                    *name = value;
                }
                else if ((value = ParseParameter(&p, "filename")) != NULL) {
                    *filename = value;
                }
                if ((p = strchr(p, ';')) == NULL) break;
                p++;
            }
        }
    }
    if (*name == NULL)
        return -1;
    return 0;
}

static int
ReadLine(FILE *fp, char *buf, int buf_len)
{
    int c;
    char *p, *pend;

    p = buf;
    pend = buf + buf_len - 1;
    while (p < pend && (c = getc(fp)) != EOF) {
        if (c == '\r') {
            *p++ = c;
            if ((c = getc(fp)) == '\n') {
                *p++ = c;
            }
            else {
                ungetc(c, fp);
            }
            break;
        }
        *p++ = c;
    }
    *p = '\0';
    return p - buf;
}

static void *
xrealloc(void *ptr, size_t size)
{
    void *p;

    p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "no memory space!!\n");
        exit(1);
    }
    return p;
}

#define MEMORY_EXPANSION_STRATEGY_THRESHOLD (1024 * 1024)
#define MEMORY_EXPANSION_UNIT (512 * 1024)

static int
ParseBody(FILE *fp, char *delimiter, char *close_delimiter,
          char **value, int *value_len)
{
    int boundary_type = BOUNDARY_NONE;
    char buf[SIZE_BUFF];
    int read_len;
    char *val;
    int val_capa = SIZE_BUFF, val_len = 0;

    val = (char *) xmalloc(val_capa);
    while ((read_len = ReadLine(fp, buf, sizeof(buf))) > 0) {
        boundary_type = CheckBoundary(buf, delimiter, close_delimiter);
        if (boundary_type != BOUNDARY_NONE)
            break;
        if (val_len + read_len + 1 > val_capa) {
            if (val_capa < MEMORY_EXPANSION_STRATEGY_THRESHOLD) {
                val_capa = val_capa * 2;
            }
            else {
                val_capa += MEMORY_EXPANSION_UNIT;
            }
            val = (char *) xrealloc(val, val_capa);
        }
        memcpy(val + val_len, buf, read_len);
        val_len += read_len;
    }
    if (val_len >= 2 &&
        val[val_len - 2] == '\r' && val[val_len - 1] == '\n')
        val_len -= 2;
    val[val_len] = '\0';
    *value = val;
    *value_len = val_len;
    return boundary_type;
}

static int
ParsePart(FILE *fp, char *delimiter, char *close_delimiter,
          GHashTable *values, GHashTable *files)
{
    char *name, *filename, *value;
    int value_len;
    char buf[SIZE_BUFF];
    int boundary_type;

    if (ParseHeader(fp, &name, &filename) < 0)
        return -1;
    boundary_type = ParseBody(fp, delimiter, close_delimiter,
                              &value, &value_len);
    if (boundary_type == BOUNDARY_NONE)
        return -1;
    if (filename == NULL) {
        g_hash_table_insert(values, name, value);
    }
    else {
        MultipartFile *file = (MultipartFile *) xmalloc(sizeof(MultipartFile));
        file->filename = filename;
        file->value = value;
        file->length = value_len;
        g_hash_table_insert(files, name, file);
    }
    return boundary_type;
}

int
ParseMultipart(FILE *fp, char *boundary,
               GHashTable *values, GHashTable *files)
{
    char buf[SIZE_BUFF];
    char *p;
    char delimiter[STR_LITERAL_LENGTH("--") +
                   MAX_BOUNDARY_SIZE +
                   STR_LITERAL_LENGTH("\r\n") +
                   1];
    char close_delimiter[STR_LITERAL_LENGTH("--") +
                         MAX_BOUNDARY_SIZE +
                         STR_LITERAL_LENGTH("--\r\n") +
                         1];
    int boundary_type = BOUNDARY_NONE;

    if (strlen(boundary) > MAX_BOUNDARY_SIZE) {
        return -1;
    }

    sprintf(delimiter, "--%s\r\n", boundary);
    sprintf(close_delimiter, "--%s--\r\n", boundary);

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        boundary_type = CheckBoundary(buf, delimiter, close_delimiter);
        if (boundary_type != BOUNDARY_NONE) {
            break;
        }
    }
    while (boundary_type == BOUNDARY_DELIMITER) {
        boundary_type = ParsePart(fp, delimiter, close_delimiter,
                                  values, files);
    }
    if (boundary_type != BOUNDARY_CLOSE_DELIMITER)
        return -1;
    return 0;
}

#if 0
#include <libgen.h>

void
DumpValue(char *name, char *value, gpointer data)
{
    printf("name='%s'\n", name);
    printf("value='%s'\n", value);
}

void
DumpFile(char *name, MultipartFile *file, gpointer data)
{
    FILE *fp;

    printf("name='%s'\n", name);
    printf("filename='%s'\n", file->filename);
    printf("length=%d\n", file->length);
    fp = fopen(basename(StrDup(file->filename)), "w");
    fwrite(file->value, 1, file->length, fp);
    fclose(fp);
}

int
main(int argc, char **argv)
{
    char *content_type, *filename, *boundary;
    GHashTable *values, *files;

    if (argc < 3) {
        fprintf(stderr, "usage: multipart <content_type> <filename>\n");
        exit(1);
    }
    content_type = argv[1];
    filename = argv[2];
    boundary = GetMultipartBoundary(content_type);
    if (boundary == NULL) {
        fprintf(stderr, "invalid content-type: %s\n", content_type);
        exit(1);
    }
    values = NewNameHash();
    files = NewNameHash();
    if (ParseMultipart(fopen(filename, "r"), boundary, values, files) < 0) {
        fprintf(stderr, "parse error\n");
        exit(1);
    }
    g_hash_table_foreach(values, (GHFunc) DumpValue, NULL);
    g_hash_table_foreach(files, (GHFunc) DumpFile, NULL);
    return 0;
}
#endif