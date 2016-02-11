#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#ifndef AHO_CORASICK_H
#define AHO_CORASICK_H

#define MAX_STRING_LEN 8192
#define MAX_FIRST_CHARS 256


#ifdef __cplusplus
extern "C"
{
#endif

#define AC_FLAG_CI          (1<<0) // Set for case insensitive
#define AC_FLAG_NO_PREFIX   (1<<1) // Set for add all patterns, even if prefix exists



struct aho_corasick
{
    struct ac_state *zero_state;
    unsigned int next_state;
    int flag;
};
typedef struct aho_corasick aho_corasick_t;
struct ac_state
{
    unsigned int id;
    void *data;
    size_t output;
    struct ac_state *fail;
    struct ac_state *next;
    struct ac_goto_node *first;
    unsigned int goto_size;
    unsigned char *childrenLabels;
    struct ac_state **childrenStates;
};
typedef struct ac_state ac_state_t;
struct ac_goto_node
{
    unsigned char label;
    struct ac_state *state;
    struct ac_goto_node *next;
};
typedef struct ac_goto_node ac_goto_node_t;

//void * my_alloc(size_t size);
//void my_free(void * ptr);
//#ifdef AC_DEBUG
//size_t my_malloced();
//#endif

int ac_init(aho_corasick_t *ac, int flags);
int ac_add_non_unique_pattern(aho_corasick_t *in, const char *pattern,
                              size_t n, void *pUData = NULL);
int ac_add_patterns_from_file(aho_corasick_t *in, const char *filename);
int ac_maketree(aho_corasick_t *in);

/* search for matches in a aho corasick tree. */
unsigned int ac_search(aho_corasick_t *in, const char *string, size_t n,
                       size_t startpos, size_t *out_start, size_t *out_end,
                       ac_state_t **out_last_state, int case_insensitive,
                       void **ppUData = NULL);

void ac_release(aho_corasick_t *ac);

int ac_is_case_insensitive(aho_corasick_t *ac);

#ifdef __cplusplus
}
#endif


#endif
