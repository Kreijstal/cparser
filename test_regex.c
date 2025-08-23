#include <stdio.h>
#include <string.h>
#include <regex.h>

int main() {
    const char* text = "1.5 + 2.5;";
    const char* pattern = "[0-9]+\\.[0-9]+";
    regex_t regex;
    regmatch_t pmatch[1];
    int reti;

    printf("Testing regex pattern: '%s' on text: '%s'\n", pattern, text);

    // Compile the regex
    reti = regcomp(&regex, pattern, REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        return 1;
    }

    // Execute the regex
    reti = regexec(&regex, text, 1, pmatch, 0);
    if (reti == 0) {
        printf("Match found!\n");
        printf("Match starts at %d and ends at %d\n", (int)pmatch[0].rm_so, (int)pmatch[0].rm_eo);

        int match_len = pmatch[0].rm_eo - pmatch[0].rm_so;
        char matched_str[match_len + 1];
        strncpy(matched_str, text + pmatch[0].rm_so, match_len);
        matched_str[match_len] = '\0';
        printf("Matched string: '%s'\n", matched_str);

    } else if (reti == REG_NOMATCH) {
        printf("No match found.\n");
    } else {
        char errbuf[100];
        regerror(reti, &regex, errbuf, sizeof(errbuf));
        fprintf(stderr, "Regex match failed: %s\n", errbuf);
        return 1;
    }

    regfree(&regex);
    return 0;
}
