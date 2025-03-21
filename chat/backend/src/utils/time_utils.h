#ifndef TIME_UTILS_H
#define TIME_UTILS_H

// Retorna una cadena (malloc'd) con el timestamp actual en formato "YYYY-MM-DD HH:MM:SS".
// La cadena debe liberarse con free() cuando ya no se necesite.
char *get_timestamp(void);

#endif
