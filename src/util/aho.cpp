#include "aho.h"

#define FAIL NULL

static int goto_list_insert(ac_state_t *state, ac_goto_node_t *node);
static int ac_goto_set(ac_state_t *from_state, unsigned char symbol,
                       ac_state_t *to_state);
static ac_state_t *ac_goto_get_before_treemake(ac_state_t *state,
        unsigned char symbol);
static ac_state_t *ac_goto_get(ac_state_t *state, unsigned char symbol);
static void ac_free_goto_node(ac_goto_node_t *go);
static void ac_free_nodes(ac_state_t *state, int case_insensitive);

#ifdef AC_DEBUG
static size_t malloced = 0;
void *my_alloc(size_t size)
{
    void *p;

    p = malloc(size);
    if (p == NULL)
    {
        printf("no enough memory, exit!\n");
        printf("memory allocated so far: %ld\n", (long)malloced);
        return (NULL);
    }
    malloced += size;
    return (p);
}

void my_free(void *ptr)
{
    free(ptr);
}

size_t my_malloced()
{
    return malloced;
}
#else

#define my_alloc(x) malloc(x)
#define my_free(x) free(x)

#endif

static int
goto_list_insert(ac_state_t *state, ac_goto_node_t *node)
{
    ac_goto_node_t *p, * p2;
    unsigned char sym = node->label;

    p = state->first;
    if (p == NULL)
    {
        state->first = node;
        state->goto_size = 1;
        node->next = NULL;
        return (1);
    }
    if (p->label > sym)
    {
        state->first = node;
        state->goto_size++;
        node->next = p;
        return (1);
    }
    for (p2 = p; p != NULL && p->label < sym; p = p->next)
        p2 = p;
#ifdef AC_DEBUG
    if (p != NULL && p->label == sym)
    {
        printf("impossible! fatal error\n");
        return (0);
    }
#endif
    //insert between p2 and p
    p2->next = node;
    node->next = p;
    state->goto_size ++;
    return (1);
}

int
ac_init(aho_corasick_t *g, int flags)
{
    if (!g)
        return (0);
    g->next_state = 1;
    g->zero_state = (ac_state *)my_alloc(sizeof(ac_state_t));
    if (g->zero_state == NULL)
        return (0);
    g->flag = flags;
    g->zero_state->id = 0;
    g->zero_state->output = 0;
    g->zero_state->fail = NULL; /* maybe not be referenced ? */
    g->zero_state->first = NULL;
    g->zero_state->goto_size = 0;
    g->zero_state->next = NULL;
    g->zero_state->childrenLabels = NULL;
    g->zero_state->childrenStates = NULL;
    return (1);
}

static int
ac_goto_set(ac_state_t *from_state, unsigned char symbol,
            ac_state_t *to_state)
{
    ac_goto_node_t *edge;
    edge = (ac_goto_node_t *)my_alloc(sizeof(ac_goto_node_t));
    if (edge == NULL)
        return (0);
    edge->label = symbol;
    edge->state = to_state;
    edge->next = NULL;

    goto_list_insert(from_state, edge);
    return (1);
}

static ac_state_t *
ac_goto_get_before_treemake(ac_state_t *state, unsigned char symbol)
{

    ac_goto_node_t *node;
    unsigned char uc;

    node = state->first;
    while (node != NULL)
    {
        uc = node->label;
        if (uc == symbol)
            return node->state;
        else if (uc > symbol)
            break;
        node = node->next;
    }
    return FAIL;
}

static ac_state_t *
ac_goto_get(ac_state_t *state, unsigned char symbol)
{
    ac_goto_node_t *node;
    unsigned char uc;

    node = state->first;
    while (node != NULL)
    {
        uc = node->label;
        if (uc == symbol)
            return node->state;
        else if (uc > symbol)
            break;
        node = node->next;
    }
    if (state->id == 0)
        return state;
    else
        return FAIL;
}

int
ac_add_non_unique_pattern(aho_corasick_t *in, const char *pattern,
                          size_t n, void *pUData)
{
    aho_corasick_t *g = in;
    ac_state_t *state, *s = NULL;
    size_t j = 0;
    int r;
    unsigned char ch;

    state = g->zero_state;
    while (j != n)
    {
        ch = *(pattern + j);
        if (g->flag & AC_FLAG_CI)
            ch = tolower(ch);
        s = ac_goto_get_before_treemake(state, ch);
        if (s == FAIL)
            break;

        if (((in->flag & AC_FLAG_NO_PREFIX) == 0) && (s->output > 0))
        {
#ifdef AC_DEBUG
            printf("-----pattern=%s, this pattern can be safely ignored - its prefix is a pattern\n",
                   pattern);
#endif
            return (1);
        }
        state = s;
        ++j;
    }

    if (((in->flag & AC_FLAG_NO_PREFIX) == 0) && (j == n))
    {
        if (s->output == 0)
        {
            s->output = j;
            ac_free_goto_node(s->first);
            s->first = NULL;
            s->goto_size = 0;
#ifdef AC_DEBUG
            printf("-----pattern=%s, this pattrn discards previous patterns - it's a prefix of previous ones",
                   pattern);
#endif
            return (1);
        }
        else if (s->output == j)
        {
#ifdef AC_DEBUG
            printf("info:same pattern already exists\n");
#endif
            return (1);
        }
#ifdef AC_DEBUG
        else
        {
            printf("warning: impossible,somewhare logic error\n");
            return (0);
        }
#endif
    }

    while (j != n)
    {
        s = (ac_state_t *)my_alloc(sizeof(ac_state_t));
        if (s == NULL)
            return (0);
        memset(s, 0, sizeof(ac_state_t));
        s->id = g->next_state++;

        s->first = NULL;
        s->goto_size = 0;
        s->childrenLabels = NULL;
        s->childrenStates = NULL;

        ch = *(pattern + j);
        if (g->flag & AC_FLAG_CI)
            ch = tolower(ch);
        r = ac_goto_set(state, ch, s);
        if (r == 0)
            return (0);
        state = s;
        s->output = 0;
        s->fail = NULL;
        ++j;
    }

    s->data = pUData;
    s->output = n;

    return (1);
}

//First call should pass in zero state
static int ac_optimize(ac_state_t *state, int caseInsensitive)
{
    ac_goto_node_t *nextChild, *child = state->first;
    int i = 0;
    if (state->first == NULL)
        return 1;

    if (caseInsensitive == 1)
    {
        state->goto_size <<= 1;
        if ((state->childrenLabels = (unsigned char *)my_alloc(sizeof(
                                         unsigned char) * (state->goto_size))) == NULL)
            return 0;
        if ((state->childrenStates = (ac_state_t **)my_alloc(sizeof(
                                         ac_state_t *) * (state->goto_size))) == NULL)
            return 0;
    }
    else
    {
        if ((state->childrenLabels = (unsigned char *)my_alloc(sizeof(
                                         unsigned char) * (state->goto_size))) == NULL)
            return 0;
        if ((state->childrenStates = (ac_state_t **)my_alloc(sizeof(
                                         ac_state_t *) * (state->goto_size))) == NULL)
            return 0;
    }
    state->first = NULL;
    while (child != NULL)
    {
        if (caseInsensitive == 1)
        {
            state->childrenLabels[i] = toupper(child->label);
            state->childrenStates[i++] = child->state;
            state->childrenLabels[i] = tolower(child->label);
            state->childrenStates[i] = child->state;
        }
        else
        {
            state->childrenLabels[i] = child->label;
            state->childrenStates[i] = child->state;
        }

        if (ac_optimize(child->state, caseInsensitive) == 0)
            return 0;
        i++;
        nextChild = child->next;
        my_free(child);
        child = nextChild;
    }
    return 1;
}

int ac_maketree(aho_corasick_t *in)
{
    ac_state_t *state, *s, *r, *head, *tail;
    aho_corasick_t *g = in;
    ac_goto_node_t *node;
    unsigned char sym;

    head = NULL;
    tail = NULL;

    /* all goto_node under zero_state */
    for (node = g->zero_state->first; node != NULL; node = node->next)
    {
        s = node->state;

        if (head == NULL)
        {
            head = s;
            tail = s;
        }
        else
        {
            tail->next = s;
            tail = s;
        }
        tail->next = NULL;

        s->fail = g->zero_state;
    }

    // following is the most tricky part
    // set fail() for depth > 0
    for (; head != NULL; head = head->next)
    {
        r = head;

        //now r's all goto_node
        for (node = r->first; node != NULL; node = node->next)
        {
            s = node->state;
            sym = node->label;
            /* no need to set leaf's fail function -- only if each pattern is unique
              if non-unique pattern allowed, have to comment off following if statement -- otherwise, may cause segment fault */
            //if ( s->output == 0){    // or s->first == NULL
            tail->next = s;
            tail = s;
            tail->next = NULL;

            state = r->fail;
            while (ac_goto_get(state, sym) == FAIL)
                state = state->fail;
            s->fail = ac_goto_get(state, sym);
            //}
        }

    }

    ac_optimize(g->zero_state, g->flag & AC_FLAG_CI);
    return (1);

}

unsigned int ac_search(aho_corasick_t *in, const char *string, size_t n,
                       size_t startpos,
                       size_t *out_start, size_t *out_end, ac_state_t **out_last_state,
                       int case_insensitive, void **ppUData)
{
    unsigned char uc;
    const char *pInputPtr;
    size_t iStringIter;
    unsigned int ret = 0;
    ac_state_t *state = in->zero_state;
    int iChildPtr = in->zero_state->goto_size,
        iNumChildren = in->zero_state->goto_size;
    ac_state_t *aAcceptArray[MAX_FIRST_CHARS];
    *out_start = -1;
    *out_end = -1;
    *out_last_state = NULL;

    memset(aAcceptArray, 0, sizeof(aAcceptArray));
    for (int i = 0; i < iNumChildren; i++)
        aAcceptArray[ in->zero_state->childrenLabels[i] ] =
            in->zero_state->childrenStates[i];

    for (iStringIter = startpos; iStringIter < n ; iStringIter++)
    {
        if ((iChildPtr == iNumChildren) && (state == in->zero_state))
        {
            for (pInputPtr = string + iStringIter; pInputPtr < string + n; pInputPtr++)
                if ((state = aAcceptArray[(unsigned char) * pInputPtr]) != 0)
                    break;
            if (pInputPtr >= string + n)
                break;
            iStringIter = pInputPtr - string + 1;
        }
        uc = *(string + iStringIter);
        do
        {
            iNumChildren = state->goto_size;
            for (iChildPtr = 0; iChildPtr < iNumChildren; iChildPtr++)
                if (uc == state->childrenLabels[iChildPtr])
                    break;
            if (iChildPtr == iNumChildren)
            {
                state = state->fail;
                if (state == in->zero_state)
                    break;
            }
        }
        while (iChildPtr == iNumChildren);
        if ((iChildPtr != iNumChildren) || (state != in->zero_state))
        {
            state = state->childrenStates[iChildPtr];
            if (state->output != 0)
            {
                *out_start = iStringIter - state->output + 1;
                *out_end = iStringIter + 1;
                *out_last_state = state;
                if (ppUData != NULL)
                    *ppUData = state->data;
                if (!(in->flag & AC_FLAG_NO_PREFIX))
                    return state->id;
                ret = state->id;
            }
        }
    }
//     *out_start = -1;
//     *out_end = -1;
//     *out_last_state = state;
//     return (0);
    if (*out_last_state == NULL)
        *out_last_state = state;
    return ret;
}

int ac_add_patterns_from_file(aho_corasick_t *ac, const char *filename)
{
    char buf[MAX_STRING_LEN], *start, *end;
    FILE *fp;
    int r;

    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
#ifdef AC_DEBUG
        printf("failed to open file: %s\n", filename);
#endif
        return (0);
    }

    /* no problem when a line exceed max length -- it'll be split into multiple lines */
    for (; fgets(buf, MAX_STRING_LEN, fp) != NULL;)
    {

        /* trim whitespace */
        start = buf;
        while (isspace(*start) && *start != 0) start++;

        /* ignore empty or comment line */
        if (*start == 0 || *start == '#')
            continue;
        end = buf + strlen(buf) - 1;
        while (isspace(*end)) end--;
        *(++end) = 0;  /* needed later for strstr() */

        r = ac_add_non_unique_pattern(ac, start,
                                      end - start);/* not include the last '\0' */
        if (r == 0)return (0);
    }
    fclose(fp);
    return (1);
}

static void ac_free_goto_node(ac_goto_node_t *go)
{
    ac_state_t *s;
    ac_goto_node_t *go_next;

    go_next = go->next;
    s = go->state;
    if (s->first != NULL)
        ac_free_goto_node(s->first);
    my_free(s->childrenLabels);
    my_free(s->childrenStates);
    my_free(s);
    if (go_next != NULL)
        ac_free_goto_node(go_next);
    my_free(go);
}

static void ac_free_nodes(ac_state_t *state, int case_insensitive)
{
    if (state->childrenStates != NULL)
    {
        for (size_t i = 0; i < state->goto_size; i++)
        {
            ac_free_nodes(state->childrenStates[i], case_insensitive);
            if (case_insensitive ==
                1)   //if case insensitive, skip one to prevent double freeing
                i++;
        }
        my_free(state->childrenLabels);
        state->childrenLabels = NULL;
        my_free(state->childrenStates);
        state->childrenStates = NULL;
    }
    my_free(state);
    state = NULL;
}

void ac_release(aho_corasick_t *ac)
{
    ac_state_t *zero;

    if (ac == NULL)
        return;

    zero = ac->zero_state;
    if (zero == NULL)
        return;
    ac_free_nodes(zero, ac->flag & AC_FLAG_CI);
}

int ac_is_case_insensitive(aho_corasick_t *ac)
{   return ac->flag & AC_FLAG_CI;   }


