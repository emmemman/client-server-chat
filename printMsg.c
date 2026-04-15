#include <stdio.h>

void printMsg(FILE *stream, const char *msg) {
	fprintf(stream, "\r%s", msg);
	fflush(stream);
}