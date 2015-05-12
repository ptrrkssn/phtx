/*
** phtx.c - Peter's HTML Table Data Extractor
**
** This tool can be used to extract data from HTML tables into CSV format.
**
** Copyright (c) 2013 Peter Eriksson <pen@lysator.liu.se>
**
** This program is free software; you can redistribute it and/or modify
** it as you wish - as long as you don't claim that you wrote it.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
**
*/

/* TODO: Handle removal of tags better (delete, not replace with whitespace) */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "entities.h"

#define DEF_CELLS 32
#define DEF_ROWS  64

#define DEF_BUFSIZE 32768

extern char version[];

/* TODO: Make dynamic */
#define MAXTABLES 256

#define NBSP (char) 160
#define is_space(c) (isspace(c) || (c == NBSP))


char *img_magic = NULL;

int verbose = 0;
int debug = 0;
int fill_out = 0;
int span_repeat = 0;

int p_caption = 0;
int p_rowno = 0;
int p_strip = 0;

char *delim = ";";
char *match = NULL;
char *empty = NULL;
int m_no = 0;


typedef struct tablerow {
    int cc; /* Current cell */
    int cm; /* Last cell */
    int cs; /* Cell vector size */
    char **cv;
} TABLEROW;


typedef struct table {
    int id;
    char *caption; /* Table caption */
    
    char *ta_s;
    char *td_s;    /* Start of TD tag */
    
    int cm;        /* Max cm in any row */

    TABLEROW *rp;  /* Currently open row */
    int rc;        /* Current row */
    int rs;        /* Row vector size */
    TABLEROW **rv; /* Row vector */
} TABLE;


/* Tables */
int tc;
TABLE *tv[MAXTABLES];

/* Tablestack */
int tsc = 0;
TABLE *tsv[MAXTABLES];


int
table_row_close(TABLE *tp)
{
    TABLEROW *rp;


    if (debug)
	fprintf(stderr, "table_row_close(tp->id=%d, tp->rc=%d)\n", tp->id, tp->rc);

    if (tp == NULL || tp->rp == NULL)
	return -1;
    
    rp = tp->rp;

    /* Update table max cell idx */
    if (rp->cm > tp->cm)
	tp->cm = rp->cm;

    tp->rp = NULL;
    
    return tp->rc++;
}



TABLEROW *
table_row_create(TABLE *tp,
		 int row)
{
    int i, j;
    TABLEROW *rp = NULL;
    

    if (debug)
	fprintf(stderr, "table_row_create(tp->id=%d, row=%d) : tp->rc=%d\n", tp->id, row, tp->rc);
    
    /* Allocate all rows up to and including the target row */
    for (i = tp->rc; i <= row; i++)
    {
	/* Need more row space? */
        if (i >= tp->rs)
	{
	    TABLEROW **nrv;

	    if (debug)
		fprintf(stderr, "  -> resizing row vector, new size=%d\n", tp->rs+DEF_ROWS);
	    
	    nrv = realloc(tp->rv, sizeof(tp->rv[0])*(tp->rs+DEF_ROWS));
	    if (nrv == NULL)
	    {
		if (debug)
		    fprintf(stderr, "   -> realloc failed\n");
		return NULL;
	    }

	    tp->rv = nrv;
	    tp->rs += DEF_ROWS;
	    
	    for (j = tp->rc; j < tp->rs; j++)
		tp->rv[j] = NULL;
	}
	
	if (tp->rv[i] == NULL)
	{
	    if (debug)
		fprintf(stderr, "   -> allocating new row\n");
	    
	    rp = malloc(sizeof(TABLEROW));
	    if (!rp)
		return NULL;
	    
	    rp->cc = 0;
	    rp->cm = 0;
	    rp->cs = DEF_CELLS;
	    rp->cv = malloc(sizeof(char *) * rp->cs);
	    if (rp->cv == NULL)
	    {
		if (debug)
		    fprintf(stderr, "  -> allocation of row cells failed\n");
		
		free(rp);
		return NULL;
	    }
	    
	    for (j = 0; j < rp->cs; j++)
		rp->cv[j] = NULL;

	    tp->rv[i] = rp;
	}
    }

    if (debug)
	fprintf(stderr, "   -> returning row=%p\n", tp->rv[row]);
    
    return tp->rv[row];
}

    
int
table_row_open(TABLE *tp)
{
    TABLEROW *rp;

    
    if (tp->rp != NULL)
	return -1;
    
    if (debug)
	fprintf(stderr, "table_row_open(id=%d): tp->rc=%d\n", tp->id, tp->rc);

    rp = table_row_create(tp, tp->rc);
    if (!rp)
    {
	if (debug)
	    fprintf(stderr, "  -> table_row_create failed\n");
	return -1;
    }

    tp->rp = tp->rv[tp->rc];

    if (debug)
	fprintf(stderr, "  -> row %d opened\n", tp->rc);
		
    return tp->rc;
}



TABLE *
table_open(void)
{
    int i;
    TABLE *tp;


    tp = malloc(sizeof(TABLE));
    if (!tp)
	return NULL;

    tp->id = tc+1;
    tp->caption = NULL;
    
    tp->rc = 0;
    tp->rp = NULL;
    
    tp->cm = 0;
    tp->ta_s = NULL;
    tp->td_s = NULL;

    
    if (debug)
	fprintf(stderr, "table_open(): id=%d, tsc=%d\n", tp->id, tsc);

    tp->rv = malloc(sizeof(tp->rv[0])*DEF_ROWS);
    if (tp->rv == NULL)
	return NULL;
    tp->rs = DEF_ROWS;
    
    for (i = 0; i < tp->rs; i++)
	tp->rv[i] = NULL;
    
    tv[tc++] = tsv[tsc++] = tp;
    
    return tp;
}


TABLE *
table_close(TABLE *tp)
{
    if (debug)
	fprintf(stderr, "table_close(tp->id=%d, tp->rc=%d), tsc=%d\n", tp->id, tp->rc, tsc);

    if (tp->rp)
	return NULL;
    
    --tsc;
    if (tsc == 0)
	return NULL;

    return tsv[tsc-1];
}



int
table_append(TABLE *tp,
	     char *buf,
	     int rowspan,
	     int colspan)
{
    TABLEROW *rp;
    int i, nr, nc;
    int cc;


    if (debug)
	fprintf(stderr, "table_append(tp->id=%d, rowspan=%d, colspan=%d, \"%s\")\n",
		tp ? tp->id : -1, rowspan, colspan, buf);
    
    if (tp == NULL)
	return -1;
    
    if (tp->rp == NULL)
	return -1;
    
    rp = tp->rp;
	
    if (debug)
	fprintf(stderr, "  -> tp->rc=%d, tp->cm=%d, rp->cc=%d, rp->cs=%d\n", 
		tp->rc, tp->cm, rp->cc, rp->cs);
    
    /* Skip pre-filled rowspan:d cells */
    while (rp->cc <= rp->cm && rp->cv[rp->cc] != NULL)
	rp->cc++;

    cc = 0;
    /* Insert cell data */
    for (nc = 0; nc < colspan; nc++)
    {
	cc = rp->cc++;
	nr = 0;
	for (i = tp->rc; nr < rowspan; i++, nr++)
	{
	    if (tp->rv[i] == NULL)
	    {
		rp = table_row_create(tp, i);
		if (!rp)
		    return -1;
	    }
	    else
		rp = tp->rv[i];
	    
	    if (cc >= rp->cs)
	    {
		int j;
		
		rp->cs = cc+DEF_CELLS;
		
		rp->cv = realloc(rp->cv, sizeof(char *) * rp->cs);
		for (j = rp->cc; j < rp->cs; j++)
		    rp->cv[j] = NULL;
		
		if (!rp)
		    return -1;
	    }
	    
	    if (span_repeat || (nr == 0 && nc == 0))
		rp->cv[cc] = strdup(buf);
	    else
		rp->cv[cc] = strdup(""); /* strdup(empty ? empty : ""); */
	    
	    if (cc > rp->cm)
		rp->cm = cc;
	    if (cc > tp->cm)
		tp->cm = cc;
	}
    }
    
    return cc;
}



int
puts_csv(const char *buf,
	 FILE *fp)
{
    int quote = 0;
    int lastc = 0;
    const char *end;
    

    if (!buf || !*buf)
    {
	if (empty)
	    if (fputs(empty, fp) < 0)
		return -1;
	
	return 0;
    }
    
    end = buf+strlen(buf);
    if (p_strip)
    {
	while (*buf && is_space(*buf))
	    ++buf;
	while (end > buf && is_space(end[-1]))
	    --end;
    }
	
    if (strstr(buf, delim))
	quote = '"';

    if (quote)
	if (putc(quote, fp) < 0)
	    return -1;

    if (buf == end)
    {
	if (empty)
	    if (fputs(empty, fp) < 0)
		return -1;
	
	if (quote)
	    if (putc(quote, fp) < 0)
		return -1;
	return 0;
    }

    for (; *buf && buf < end; ++buf)
    {
	if (p_strip > 1 && is_space(*buf) && is_space(lastc))
	    continue;
	
	if (*buf == quote)
	    if (putc('\\', fp) < 0)
		return -1;

	if (*buf == '\n')
	{
	    if (putc('\\', fp) < 0)
		return -1;
	    
	    if (putc('n', fp) < 0)
		return -1;
	}
	else
	    if (putc(*buf, fp) < 0)
		return -1;
	
	lastc = *buf;
    }
    
    if (quote)
	if (putc(quote, fp) < 0)
	    return -1;

    return 1;
}


int
table_print_csv(TABLE *tp,
		FILE *fp)
{
    int nr, nc;
    TABLEROW *rp;


    if (!tp)
	return 0; /* Nothing to print */

    if (debug)
	fprintf(stderr, "table_print_csv(tp->id=%d, tp->rc=%d, tp->cm=%d)\n",
		tp->id, tp->rc, tp->cm);
    
    if (p_caption && tp->caption)
    {
	if (!match)
	{
	    if (fprintf(fp, "%d%s", tp->id, delim) < 0)
		return -1;
	    
	    if (p_rowno && fprintf(fp, "%d%s", 0, delim) < 0)
		return -1;
	}
	else
	    if (p_rowno && fprintf(fp, "%d%s", 0, delim) < 0)
		return -1;
	
	if (puts_csv(tp->caption, fp) < 0)
	    return -1;
	
	if (putc('\n', fp) < 0)
	    return -1;
    }
    
    for (nr = 0; nr < tp->rc; nr++)
    {
	rp = tp->rv[nr];

	if (!match)
	{
	    if (fprintf(fp, "%d", tp->id) < 0)
		return -1;
	    
	    if (p_rowno && fprintf(fp, "%s%d", delim, nr+1) < 0)
		return -1;
	}
	else
	    if (p_rowno && fprintf(fp, "%d", nr+1) < 0)
		return -1;

	nc = 0;
	if (rp)
	{
	    for (; nc <= rp->cm; nc++)
	    {
		if (!match || nc > 0 || p_rowno)
		    if (fputs(delim, fp) < 0)
			return -1;
		
		if (puts_csv(rp->cv[nc], fp) < 0)
		    return -1;
	    }
	}
	
	if (fill_out)
	    for (; nc <= tp->cm; nc++)
	    {
		if (!match || nc > 0 || p_rowno)
		{
		    if (fputs(delim, fp) < 0)
			return -1;
		}
		if (empty)
		    if (fputs(empty, fp) < 0)
			return -1;
	    }
	
	if (putc('\n', fp) < 0)
	    return -1;
    }

    return 1;
}



void
output(TABLE *tp,
       char *buf,
       int len,
       int rowspan,
       int colspan)
{
    char *cp;
    
    
    if (debug > 1)
	fprintf(stderr, "output(tp->id=%d, tp->rc=%d, rowspan=%d, colspan=%d): '%.*s'\n",
		tp->id, tp->rc, rowspan, colspan, len, buf);

    cp = ent_decode(buf, len);
    if (!cp)
    {
	if (debug > 1)
	    fprintf(stderr, "   -> ent_decode() failed\n");
	return;
    }	
	
    table_append(tp, cp, rowspan, colspan);
}


int
is_tag(char *buf,
       char *tag)
{
    char *cp;

    cp = buf;
    if (*cp++ != '<')
	return 0;

    while (*tag && toupper(*cp) == *tag)
    {
	++tag;
	++cp;
    }
    if (*tag)
	return 0;
    return (*cp == ' ' || *cp == '>');
}


void *memmem(const void *haystack, size_t hlen, const void *needle, size_t nlen)
{
    int needle_first;
    const void *p = haystack;
    size_t plen = hlen;
    
    if (!nlen)
	return NULL;
    
    needle_first = *(unsigned char *)needle;
    
    while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
    {
	if (!memcmp(p, needle, nlen))
	    return (void *)p;
	
	p++;
	plen = hlen - (p - haystack);
    }
    
    return NULL;
}

int
is_match(char *buf, int buflen, char *str)
{
    return memmem(buf, buflen, str, strlen(str)) != NULL;
}


char *
load_file(const char *path,
	  size_t *buflen)
{
    FILE *fp;
    struct stat sb;
    char *buf;
    size_t bufsize, bufpos, to_read, got;


    bufsize = DEF_BUFSIZE;
    
    if (path && strcmp(path, "-") != 0)
    {
	fp = fopen(path, "r");
	if (!fp)
	    return NULL;
	
	if (fstat(fileno(fp), &sb) != 0)
	{
	    fclose(fp);
	    return NULL;
	}
	
	bufsize = sb.st_size + DEF_BUFSIZE;
    }
    else
	fp = stdin;

    buf = malloc(bufsize+1);
    if (!buf)
    {
	if (fp != stdin)
	    fclose(fp);
	
	return NULL;
    }

    *buflen = 0;
    bufpos = 0;
    do
    {
	if (bufpos >= bufsize)
	{
	    if (debug > 1)
		fprintf(stderr, "read_file: realloc(bufsize=%lu+%lu), bufpos=%lu\n",
			(unsigned long) bufsize, (unsigned long) DEF_BUFSIZE, (unsigned long) bufpos);
	    
	    bufsize += DEF_BUFSIZE;
	    buf = realloc(buf, bufsize+1);
	    if (!buf)
	    {
		if (fp != stdin)
		    fclose(fp);
		return NULL;
	    }
	}
	
	to_read = bufsize-bufpos;
	got = fread(buf+bufpos, 1, to_read, fp);
	if (debug > 1)
	    fprintf(stderr, "read_file: fread(bufpos=%lu, to_read=%lu) -> got=%lu\n",
		    (unsigned long) bufpos, (unsigned long) to_read, (unsigned long) got);
	
	if (got > 0)
	{
	    *buflen += got;
	    bufpos += got;
	}
    } while (got == to_read);

    if (fp != stdin)
	fclose(fp);
    
    buf[*buflen] = '\0';
    return buf;
}

void
print_line(char *str,
	   FILE *fp)
{
    char *ep;
    
    ep = strchr(str, '\n');
    if (ep)
	fprintf(fp, "%.*s", (int) (ep-str), str);
    else
	fputs(str, fp);
    putc('\n', fp);
}


void
print_version(FILE *fp)
{
    fprintf(fp, "[PHTX %s - Peter's HTML Table Extractor>]\n", version);
}

int
main(int argc,
     char *argv[])
{
    char *buf;
    size_t buflen;
    int state = 0;
    char *sp, *cp;
    int nt, nc, nr, nf;
    int ai, aj, ti;
    TABLE *tp = NULL;
    int rowspan = 1;
    int colspan = 1;
    int skip_cell = 0;
    int lastc = -1;
    int line;
    char *outpath = NULL;
    FILE *outfp = NULL;
    

    for (ai = 1; ai < argc && argv[ai][0] == '-'; ai++)
    {
	for (aj = 1; argv[ai][aj]; aj++)
	{
	    switch (argv[ai][aj])
	    {
	      case 0:
		goto EndArg;
		
	      case 'h':
		printf("Usage: %s [<options>] <html-file>\n", argv[0]);
		puts("Options:");
		puts("   -h           Display this information");
		puts("   -V           Print program version");
		puts("   -r           Output row numbers");
		puts("   -c           Output table caption (if defined)");
		puts("   -f           Fill-out tables to equal length rows");
		puts("   -R           Row/Col-Span repeat mode");
		puts("   -v           Increase verbosity level");
		puts("   -s           Increase whitespace strip level");
		puts("   -d           Increase debug level");
		puts("   -I <mode>    IMG special magic mode");
		puts("   -E <string>  String to print instead of empty cells");
		puts("   -D <delim>   CSV field separator (default ';')");
		puts("   -M <match>   Table selector");
		puts("   -O <path>    Output file");
		exit(0);

	      case '-':
		++ai;
		goto EndArg;
		    
	      case 'V':
		print_version(stdout);
		puts("Author: Peter Eriksson <pen@lysator.liu.se>");
#if defined(USER) && defined(HOST)
		printf("Built:  %s %s by %s on %s\n", __DATE__, __TIME__, USER, HOST);
#else
		printf("Built:  %s %s\n", __DATE__, __TIME__);
#endif
		exit(0);
		break;
		
	      case 'c':
		++p_caption;
		break;
		
	      case 'r':
		++p_rowno;
		break;
		
	      case 'f':
		++fill_out;
		break;
		
	      case 'R':
		++span_repeat;
		break;
		
	      case 's':
		++p_strip;
		break;
		
	      case 'd':
		++debug;
		break;
		
	      case 'v':
		++verbose;
		break;
		
	      case 'I':
		if (argv[ai][aj+1])
		{
		    img_magic = strdup(argv[ai]+aj+1);
		    goto NextArg;
		}
		else if (argv[ai+1])
		{
		    img_magic = strdup(argv[++ai]);
		    goto NextArg;
		}
		else
		{
		    fprintf(stderr, "%s: Missing required argument for -I\n", argv[0]);
		    exit(1);
		}
		break;

	      case 'E':
		if (argv[ai][aj+1])
		{
		    empty = strdup(argv[ai]+aj+1);
		    goto NextArg;
		}
		else if (argv[ai+1])
		{
		    empty = strdup(argv[++ai]);
		    goto NextArg;
		}
		else
		{
		    fprintf(stderr, "%s: Missing required argument for -E\n", argv[0]);
		    exit(1);
		}
		break;

	      case 'D':
		if (argv[ai][aj+1])
		{
		    delim = strdup(argv[ai]+aj+1);
		    goto NextArg;
		}
		else if (argv[ai+1])
		{
		    delim = strdup(argv[++ai]);
		    goto NextArg;
		}
		else
		{
		    fprintf(stderr, "%s: Missing required argument for -D\n", argv[0]);
		    exit(1);
		}
		break;

	      case 'O':
		if (argv[ai][aj+1])
		{
		    outpath = strdup(argv[ai]+aj+1);
		    goto NextArg;
		}
		else if (argv[ai+1])
		{
		    outpath = strdup(argv[++ai]);
		    goto NextArg;
		}
		else
		{
		    fprintf(stderr, "%s: Missing required argument for -O\n", argv[0]);
		    exit(1);
		}
		break;

	      case 'M':
		if (argv[ai][aj+1])
		{
		    match = strdup(argv[ai]+aj+1);
		    goto NextArg;
		}
		else if (argv[ai+1])
		{
		    match = strdup(argv[++ai]);
		    goto NextArg;
		}
		else
		{
		    fprintf(stderr, "%s: Missing required argument for -M\n", argv[0]);
		    exit(1);
		}
		break;
		
	      default:
		fprintf(stderr, "%s: -%c: Invalid switch\n", argv[0], argv[ai][aj]);
		exit(1);
	    }
	}
      NextArg:;
    }

  EndArg:
    if (verbose)
	print_version(stderr);
    
    if (match)
	sscanf(match, "%u", &m_no);
    
    nf = 0;
    for (; ai < argc; ai++)
    {
	if (debug)
	    fprintf(stderr, "Parsing file: %s\n", argv[ai]);

	buf = load_file(argv[ai], &buflen);
	if (!buf)
	{
	    fprintf(stderr, "%s: %s: Error loading file: %s\n", argv[0], argv[ai], strerror(errno));
	    exit(1);
	}

	++nf;
	line = 0;
	state = 0;
	cp = buf;
	sp = NULL;
	nt = nc = nr = 0;

	lastc = -1;
	for (cp = buf; *cp; lastc = *cp, ++cp)
	{
	    if (lastc == -1 || lastc == '\n')
	    {
		++line;
		if (verbose > 1 || debug)
		{
		    fprintf(stderr, "%s#%u: >> ", argv[ai], line);
		    print_line(cp, stderr);
		}
	    }
		
	    switch (state)
	    {
	      case 0:
		if (*cp == '<')
		{
		    if (cp[1] == '<')
		    {
			++cp;
			continue;
		    }
		    
		    if (cp[1] == '!' && cp[2] == '-' && cp[3] == '-')
		    {
			sp = cp;
			cp = cp+4;
			while (!(cp[-2] == '-' && cp[-1] == '-' && cp[0] == '>'))
			{
			    if (*cp == '\n' && 0)
				++line;
			    ++cp;
			}
			if (*cp)
			    ++cp;
			if (debug > 1)
			    fprintf(stderr, "comment: %.*s\n", (int) (cp-sp+1), sp);
			memset(sp, ' ', cp-sp);
			continue;
		    }
		    
		    sp = cp;
		    state = 1;
		}
		break;
		
	      case 1:
		if (*cp == '>')
		{
		    if (cp[1] == '>') /* TODO: Remove this? */
		    {
			++cp;
			continue;
		    }
		    
		    if (debug > 1)
			fprintf(stderr, "tag: %.*s\n", (int) (cp-sp+1), sp);
		    
		    if (!sp)
		    {
			state = 0;
			continue;
		    }
		    
		    if (is_tag(sp, "IMG"))
		    {
			if (img_magic && strcmp(img_magic, "tidbokonline") == 0)
			{
			    /* Special magic for 'tidbokonline' */
			    
			    if (is_match(sp, cp-sp+1, "A.gif"))
			    {
				output(tp, "Upptaget", 9, rowspan, colspan);
				skip_cell = 1;
			    }
			    else if (is_match(sp, cp-sp+1, "D.gif"))
			    {
				output(tp, "Abonnerad", 10, rowspan, colspan);
				skip_cell = 1;
			    }
			    else if (is_match(sp, cp-sp+1, "E.gif"))
			    {
				output(tp, "Boka", 5, rowspan, colspan);
				skip_cell = 1;
			    }
			    else if (is_match(sp, cp-sp+1, "G.gif"))
			    {
				output(tp, "Stängt", 6, rowspan, colspan);
				skip_cell = 1;
			    }
			    else if (is_match(sp, cp-sp+1, "H.gif"))
			    {
				output(tp, "Boka", 5, rowspan, colspan);
				skip_cell = 1;
			    }
			    else if (is_match(sp, cp-sp+1, "L.gif") ||
				     is_match(sp, cp-sp+1, "M.gif"))
			    {
				output(tp, "Arrangemang", 12, rowspan, colspan);
				skip_cell = 1;
			    }
			    else if (is_match(sp, cp-sp+1, "N.gif"))
			    {
				output(tp, "Prolympia/JohnBauer", 21, rowspan, colspan);
				skip_cell = 1;
			    }
			    else if (is_match(sp, cp-sp+1, ".gif"))
			    {
				output(tp, "???", 21, rowspan, colspan);
				skip_cell = 1;
			    }
			}
			else
			    memset(sp, ' ', cp-sp+1);
		    }
		    
		    else if (is_tag(sp, "TABLE"))
		    {
			if (tp)
			    tp->ta_s = sp;
			
			tp = table_open();
			
			if (!m_no && match && is_match(sp, cp-sp+1, match))
			    m_no = tp->id;
		    }
		    
		    else if (is_tag(sp, "/TABLE"))
		    {
			TABLE *ntp;
			
			if (tp->td_s)
			{
			    if (verbose || debug)
				fprintf(stderr, "%s#%u: Missing closing TD tag at /TABLE (auto-closed)\n",
					argv[ai], line);
			    if (!skip_cell)
				output(tp, tp->td_s, sp-tp->td_s, rowspan, colspan);
			    skip_cell = 0;
			    tp->td_s = NULL;
			}

			if (tp->rp)
			{
			    if (verbose || debug)
				fprintf(stderr, "%s#%u: Missing closing TR tag at /TABLE (auto-closed)\n",
					argv[ai], line);
			    table_row_close(tp);
			}
			
			ntp = table_close(tp);
			if (ntp)
			{
			    if (ntp->ta_s)
				memset(ntp->ta_s, ' ', cp - ntp->ta_s+1);
			    tp = ntp;
			}
		    }
		    
		    else if (tp && is_tag(sp, "TR"))
		    {
			if (tp->td_s)
			{
			    if (!skip_cell)
			    {
				output(tp, tp->td_s, sp-tp->td_s, rowspan, colspan);
			    }
			    skip_cell = 0;
			    tp->td_s = NULL;
			}

			if (tp->rp != NULL)
			{
			    if (verbose || debug)
				fprintf(stderr, "%s#%u: Missing closing TR tag at new TR (auto-closed)\n",
					argv[ai], line);
			    table_row_close(tp);
			}

			table_row_open(tp);
			tp->td_s = NULL;
		    }
		    
		    else if (tp && is_tag(sp, "/TR"))
		    {
			if (tp->td_s)
			{
			    if (verbose || debug)
				fprintf(stderr, "%s#%u: Missing closing TD tag at /TR (auto-closed)\n",
					argv[ai], line);
			    if (!skip_cell)
				output(tp, tp->td_s, sp-tp->td_s, rowspan, colspan);
			    skip_cell = 0;
			    tp->td_s = NULL;
			}

			table_row_close(tp);
			tp->td_s = NULL;
		    }
		    
		    else if (tp && is_tag(sp, "CAPTION"))
		    {
			tp->td_s = cp+1;
		    }
		    
		    else if (tp && is_tag(sp, "/CAPTION"))
		    {
			if (tp->td_s)
			{
			    if (!skip_cell)
			    {
				tp->caption = ent_decode(tp->td_s, sp-tp->td_s);
				if (debug)
				    fprintf(stderr, "Got table id=%d caption: %s\n", tp->id, tp->caption);
			    }
			    skip_cell = 0;
			    tp->td_s = NULL;
			}
		    }
			 
		    else if (tp && (is_tag(sp, "TD") || is_tag(sp, "TH")))
		    {
			char *xp, tc;
			

			if (tp->rp == NULL)
			{
			    if (verbose || debug)
				fprintf(stderr, "%s#%u: Missing starting TR tag before TD or TH (auto-opened)\n",
				        argv[ai], line);
			    
			    table_row_open(tp);
			}
			
			if (tp->td_s)
			{
			    if (verbose || debug)
				fprintf(stderr, "%s#%u: Missing closing TD or TH tag (auto-closed)\n",
					argv[ai], line);
			    
			    if (!skip_cell)
				output(tp, tp->td_s, sp-tp->td_s, rowspan, colspan);
			    skip_cell = 0;
			    tp->td_s = NULL;
			}
			
			rowspan = 1;
			tc = *cp;
			*cp = '\0';
			xp = strstr(sp, "rowspan");
			*cp = tc;
			if (xp)
			{
			    if (sscanf(xp,"rowspan=%d", &rowspan) != 1)
				(void) sscanf(xp,"rowspan=\"%d\"", &rowspan);
			}
			
			colspan = 1;
			tc = *cp;
			*cp = '\0';
			xp = strstr(sp, "colspan");
			*cp = tc;
			if (xp)
			{
			    if (sscanf(xp,"colspan=%d", &colspan) != 1)
				(void) sscanf(xp,"colspan=\"%d\"", &colspan);
			}
			
			tp->td_s = cp+1;
		    }
		    
		    else if (tp && (is_tag(sp, "/TD") || is_tag(sp, "/TH")))
		    {
			if (tp->td_s)
			{
			    if (!skip_cell)
				output(tp, tp->td_s, sp-tp->td_s, rowspan, colspan);
			    skip_cell = 0;
			    tp->td_s = NULL;
			}
		    }
		    
		    else
		    {
			memset(sp, ' ', cp-sp+1);
		    }
		    
		    state = 0;
		}
		break;
		
	      default:
		fprintf(stderr, "%s: Internal error: Invalid state: %d\n", argv[0], state);
		exit(1);
	    }
	}

	if (verbose)
	    fprintf(stderr, "%s: %d line%s parsed.\n", argv[ai], line, line == 1 ? "" : "s");
    }
    
    if (verbose)
	fprintf(stderr, "Total: %d file%s parsed, %d table%s found.\n", nf, nf == 1 ? "" : "s", tc, tc == 1 ? "" : "s");

    if (outpath)
    {
	outfp = fopen(outpath, "w");
	if (!outfp)
	{
	    fprintf(stderr, "%s: %s: Error opening output file: %s\n",
		    argv[0], outpath, strerror(errno));
	    exit(1);
	}
    }
    else
	outfp = stdout;
    
    for (ti = 0; ti < tc; ti++)
    {
	if (!m_no || tv[ti]->id == m_no)
	    if (table_print_csv(tv[ti], outfp) < 0)
	    {
		fprintf(stderr, "%s: %s: Error writing to output file: %s\n",
			argv[0], outpath, strerror(errno));
		exit(1);
	    }
    }

    if (outfp != stdout)
	if (fclose(outfp) < 0)
	{
	    fprintf(stderr, "%s: %s: Error closing output file: %s\n",
		    argv[0], outpath, strerror(errno));
	    exit(1);
	}
    
    return 0;
}
