#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "entities.h"

extern int debug;

ENTITY iso88591_ev[] = {
    {  34, "&quot;" },
    {  39, "&apos;" },
    {  38, "&amp;" },
    {  60, "&lt;" },
    {  62, "&gt;" },
    { 160, "&nbsp;" },
    { 161, "&iexcl;" },
    { 162, "&cent;" },
    { 163, "&pound;" },
    { 164, "&curren;" },
    { 165, "&yen;" },
    { 166, "&brvbar;" },
    { 167, "&sect;" },
    { 168, "&uml;" },
    { 169, "&copy;" },
    { 170, "&ordf;" },
    { 171, "&laquo;" },
    { 172, "&not;" },
    { 173, "&shy;" },
    { 174, "&reg;" },
    { 175, "&macr;" },
    { 176, "&deg;" },
    { 177, "&plusmn;" },
    { 178, "&sup2;" },
    { 179, "&sup3;" },
    { 180, "&acute;" },
    { 181, "&micro;" },
    { 182, "&para;" },
    { 183, "&middot;" },
    { 184, "&cedil;" },
    { 185, "&sup1;" },
    { 186, "&ordm;" },
    { 187, "&raquo;" },
    { 188, "&frac14;" },
    { 189, "&frac12;" },
    { 190, "&frac34;" },
    { 191, "&iquest;" },
    { 215, "&times;" },
    { 247, "&divide;" },
    { 192, "&Agrave;" },
    { 193, "&Aacute;" },
    { 194, "&Acirc;" },
    { 195, "&Atilde;" },
    { 196, "&Auml;" },
    { 197, "&Aring;" },
    { 198, "&AElig;" },  
    { 199, "&Ccedil;" },
    { 200, "&Egrave;" },
    { 201, "&Eacute;" }, 
    { 202, "&Ecirc;" },
    { 203, "&Euml;" },
    { 204, "&Igrave;" },
    { 205, "&Iacute;" },
    { 206, "&Icirc;" },
    { 207, "&Iuml;" },
    { 208, "&ETH;" },
    { 209, "&Ntilde;" },
    { 210, "&Ograve;" },
    { 211, "&Oacute;" },
    { 212, "&Ocirc;" },
    { 213, "&Otilde;" },
    { 214, "&Ouml;" },
    { 216, "&Oslash;" },
    { 217, "&Ugrave;" },
    { 218, "&Uacute;" },
    { 219, "&Ucirc;" },
    { 220, "&Uuml;" },
    { 221, "&Yacute;" },
    { 222, "&THORN;" },
    { 223, "&szlig;" },
    { 224, "&agrave;" },
    { 225, "&aacute;" },
    { 226, "&acirc;" },
    { 227, "&atilde;" },
    { 228, "&auml;" },
    { 229, "&aring;" },
    { 230, "&aelig;" },
    { 231, "&ccedil;" },
    { 232, "&egrave;" },
    { 233, "&eacute;" },
    { 234, "&ecirc;" },
    { 235, "&euml;" },
    { 236, "&igrave;" },
    { 237, "&iacute;" },
    { 238, "&icirc;" },
    { 239, "&iuml;" },
    { 240, "&eth;" }, 
    { 241 ,"ntilde;" },
    { 242, "&ograve;" },
    { 243, "&oacute;" },
    { 244, "&ocirc;" },
    { 245, "&otilde;" },
    { 246, "&ouml;" },
    { 248, "&oslash;" },
    { 249, "&ugrave;" },
    { 250, "&uacute;" },
    { 251, "&ucirc;" },
    { 252, "&uuml;" },
    { 253, "&yacute;" },
    { 254, "&thorn;" },
    { 255, "&yuml;" },
    { 0, NULL }
};


static int
str_compare(const char *s1,
	    const char *s2,
	    int s2len,
	    int ci)
{
    int d;

#if 0
    fprintf(stderr, "str_compare(\"%s\", \"%.*s\", %d, %d)", s1, s2len, s2, s2len, ci);
#endif
    
    for (d = 0; d == 0 && s2len-- > 0; s1++, s2++)
    {
	if (ci)
	    d = toupper(*s1) - toupper(*s2);
	else
	    d = *s1 - *s2;
    }

    if (!d)
	d = *s1;
#if 0
    fprintf(stderr, " = %d\n", d);
#endif
    
    return d;
}

int
str2ent(const char *str,
	int len)
{
    int i, d = -1;

    
    if (len < 0)
	len = strlen(str);

    if (sscanf(str, "&#x%x;", &d) == 1)
	return d;
    
    if (sscanf(str, "&#%u;", &d) == 1)
	return d;
    
    for (i = 0; d == -1 && iso88591_ev[i].name; ++i)
	if (str_compare(iso88591_ev[i].name, str, len, 0) == 0)
	    return iso88591_ev[i].c;

    for (i = 0; d == -1 && iso88591_ev[i].name; ++i)
	if (str_compare(iso88591_ev[i].name, str, len, 1) == 0)
	    return iso88591_ev[i].c;

    return -1;
}

const char *
ent2str(int c)
{
    int i;

    for (i = 0; iso88591_ev[i].name; ++i)
	if (iso88591_ev[i].c == c)
	    return iso88591_ev[i].name;

    return NULL;
}

char *
ent_decode(const char *str,
	   int len)
{
    char *buf, *bp;
    int i, j, c;
    

    if (!str)
	return NULL;

    if (len < 0)
	len = strlen(str);
    
    buf = malloc(len+1);
    if (!buf)
	return NULL;

    bp = buf;

    for (i = 0; i < len; i++)
    {
	switch (str[i])
	{
	  case '&':
	    for (j = i+1; j < len && str[j] != ';'; ++j)
		;
	    if (j >= len)
		*bp++ = str[i];
	    else
	    {
		c = str2ent(str+i, j-i+1);
		if (debug > 2)
		    printf("str2ent: %.*s -> %c (%d)\n", j-i+1, str+i, c, c);
		
		if (c < 0)
		    c = '?';
		else
		    *bp++ = c;
		i = j;
	    }
	    break;

	  default:
	    *bp++ = str[i];
	}
    }
    
    *bp = '\0';
    return buf;
}
