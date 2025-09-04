#include "pascal_keywords.h"
#include <string.h>
#include <ctype.h>

const char* pascal_reserved_keywords[] = {
    "begin", "end", "if", "then", "else", "while", "do", "for", "to", "downto",
    "repeat", "until", "case", "of", "var", "const", "type",
    "and", "or", "not", "xor", "div", "mod", "in", "nil", "true", "false",
    "array", "record", "set", "packed",
    // Exception handling keywords
    "try", "finally", "except", "raise", "on",
    // Class and object-oriented keywords
    "class", "object", "private", "public", "protected", "published",
    "property", "inherited", "self", "constructor", "destructor",
    // Additional Pascal keywords
    "function", "procedure", "program", "unit", "uses", "interface", "implementation",
    NULL
};

bool is_pascal_keyword(const char* str) {
    for (int i = 0; pascal_reserved_keywords[i] != NULL; i++) {
        if (strcasecmp(str, pascal_reserved_keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}
