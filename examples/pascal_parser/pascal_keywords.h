#ifndef PASCAL_KEYWORDS_H
#define PASCAL_KEYWORDS_H

#include <stdbool.h>

extern const char* pascal_reserved_keywords[];
bool is_pascal_keyword(const char* str);

#endif // PASCAL_KEYWORDS_H
