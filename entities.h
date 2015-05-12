/* entities.h */

#ifndef PHTX_ENTITIES_H
#define PHTX_ENTITIES_H

typedef struct entity {
    int c;
    char *name;
} ENTITY;


extern int
str2ent(const char *str, int len);

extern const char *
ent2str(int c);

extern char *
ent_decode(const char *str,
	   int len);

#endif
