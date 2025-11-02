#ifndef uucpfuncs_h
#define uucpfuncs_h

const char *uucpme();

void uucpneighbors_init();
char	*uucpneighbors_fetch(const char *, size_t, size_t *, const char *);
int uucprewriteheaders();

#endif
