#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uthash.h"
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

typedef struct
{
    char *key;
    int value;
    int spamCount;
    int hamCount;
    UT_hash_handle hh;
} dict_entry;

static dict_entry *dict = NULL;

void dict_set(const char *key, bool isSpam)
{
    dict_entry *e = NULL;
    HASH_FIND_STR(dict, key, e);
    if (!e)
    {
        e = malloc(sizeof(*e));
        e->key = strdup(key);
        e->value = 0;
        e->hamCount = 0;
        e->spamCount = 0;
        HASH_ADD_KEYPTR(hh, dict, e->key, strlen(e->key), e);
    }
    e->value++;
    if (isSpam)
        e->spamCount++;
    else
        e->hamCount++;
}

int dict_get(const char *key, int *out_value)
{
    dict_entry *e = NULL;
    HASH_FIND_STR(dict, key, e);
    if (!e)
        return 0;
    *out_value = e->value;
    return 1;
}

int dict_get_spam_count(const char *key, int *out_value)
{
    dict_entry *e = NULL;
    HASH_FIND_STR(dict, key, e);
    if (!e)
        return 0;
    *out_value = e->spamCount;
    return 1;
}

int dict_get_ham_count(const char *key, int *out_value)
{
    dict_entry *e = NULL;
    HASH_FIND_STR(dict, key, e);
    if (!e)
        return 0;
    *out_value = e->hamCount;
    return 1;
}

void free_dict()
{
    dict_entry *current, *tmp;
    HASH_ITER(hh, dict, current, tmp)
    {
        HASH_DEL(dict, current);
        free(current->key);
        free(current);
    }
}

bool parse_line(const char *line, char *label, char *text, size_t text_sz)
{
    if (!line || !label || !text)
        return false;

    const char *comma = strchr(line, ',');
    if (!comma)
        return false;

    size_t label_len = comma - line;
    strncpy(label, line, label_len);
    label[label_len] = '\0';

    const char *p = comma + 1;
    size_t k = 0;

    if (*p == '"')
    {
        p++;
        while (*p && k + 1 < text_sz)
        {
            if (*p == '"')
            {
                if (p[1] == '"')
                {
                    text[k++] = '"';
                    p += 2;
                }
                else
                    break;
            }
            else
                text[k++] = *p++;
        }
    }
    else
    {
        while (*p && *p != ',' && k + 1 < text_sz)
            text[k++] = *p++;
    }

    text[k] = '\0';
    return true;
}

void clean_word(const char *word, char *cleaned)
{
    int j = 0;
    for (int i = 0; word[i]; i++)
    {
        if (isalpha((unsigned char)word[i]) || isdigit((unsigned char)word[i]))
            cleaned[j++] = tolower((unsigned char)word[i]);
    }
    cleaned[j] = '\0';
}

bool is_separator(char c)
{
    return isspace(c) || strchr(",.!?;:\"'()[]{}--_", c) != NULL;
}

void process_text(const char *text, bool isSpam)
{
    char buffer[10000];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    int start = -1;

    for (int i = 0; buffer[i]; i++)
    {
        if (!is_separator(buffer[i]))
        {
            if (start == -1)
                start = i;
        }
        else if (start != -1)
        {
            buffer[i] = '\0';
            char cleaned[1000];
            clean_word(&buffer[start], cleaned);

            if (cleaned[0])
                dict_set(cleaned, isSpam);

            start = -1;
        }
    }

    if (start != -1)
    {
        char cleaned[1000];
        clean_word(&buffer[start], cleaned);
        if (cleaned[0])
            dict_set(cleaned, isSpam);
    }
}

void computeBagOfWord()
{
    FILE *file = fopen("spam.csv", "r");
    if (!file)
    {
        perror("Error opening file");
        return;
    }

    char line[10000];
    char label[10];
    char text[10000];

    while (fgets(line, sizeof(line), file))
    {
        if (parse_line(line, label, text, sizeof(text)))
        {
            bool isSpam = strcmp(label, "spam") == 0;
            process_text(text, isSpam);
        }
    }

    fclose(file);
}

float computeSpamProb()
{
    int hamCount = 0, spamCount = 0;
    FILE *file = fopen("spam.csv", "r");
    if (!file)
        return 0.0f;

    char line[1000];
    while (fgets(line, sizeof(line), file))
    {
        if (strncmp(line, "ham", 3) == 0)
            hamCount++;
        else if (strncmp(line, "spam", 4) == 0)
            spamCount++;
    }

    fclose(file);

    float spamProb = spamCount / (float)(spamCount + hamCount);
    printf("spam: %d, ham: %d\n", spamCount, hamCount);
    printf("P(spam): %.4f, P(ham): %.4f\n", spamProb, 1 - spamProb);

    return spamProb;
}

// P(wi|c) = (n_wi,c + 1) / (sum(n_w,c) + |V|)
float prob_word_given_class(const char *word, bool isSpam)
{
    int v = HASH_COUNT(dict);

    dict_entry *e = NULL;
    HASH_FIND_STR(dict, word, e);
    // n_wi,c : nb times word w appeard in class c
    int n_wi_c = 0;
    if (e)
        n_wi_c = isSpam ? e->spamCount : e->hamCount;

    // sum(n_w,c) : nb word labelised as c
    int total_words_in_class = 0;
    dict_entry *current, *tmp;
    HASH_ITER(hh, dict, current, tmp)
    {
        total_words_in_class += isSpam ? current->spamCount : current->hamCount;
    }

    return (n_wi_c + 1.0f) / (total_words_in_class + v);
}

// Compute log P(d|c) = sum(log P(wi|c)) for each class
void compute_log_likelihood(const char *doc, float *log_spam, float *log_ham)
{
    char buffer[10000];
    strncpy(buffer, doc, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    char *token = strtok(buffer, " ,.!?;:\"'()[]{}--_\t\n");
    
    while (token != NULL)
    {
        char cleaned[1000];
        clean_word(token, cleaned);
        
        if (cleaned[0])
        {
            *log_spam += log(prob_word_given_class(cleaned, true));
            *log_ham += log(prob_word_given_class(cleaned, false));
        }
        
        token = strtok(NULL, " ,.!?;:\"'()[]{}--_\t\n");
    }
}


/*
    bayes classifier:
    d: document
    c: class

    we want to find c = argmax p(c|d)
                => wich class has the higher prob considering d happened
    bayes allow us to write that:
    p(c|d) = p(d|c) * p(c) / p(d)
    but since p(d) is idependent we can simply compute
    p(c|d) = p(d|c) * p(c)

    p(d|c) is called likelihood
    p(c) is called prior

    p(c) is simply Nc / Nd
        Nc number of documents labeled as class c in the training set
        Nd total number of documents in the training set

    Naive bayes assumption:
    p(d|c) is just the product of each word of the document given c
    => p(d|c) = p(w1..wn | c) = prod(p(wi | c))

    now our formula is:

    c = argmax p(c) * prod(p(wi | c))

    we can now log everything so that we dont have underflow
    c = argmax [log p(c) + sum(log(p(wi | c))) ]

    how to compute p(wi | c):

    p(wi | c) = (Nwi,c + 1) / (sum(Nw,c) + |V|)

    Nwi,c = count of word wi in all documents labeled c
    |V| = nb  words in voc
    sum(Nw,c) = nb word labelised as c
*/

int main(int argc, char *argv[])
{
    // Build vocabulary |V| from training set
    computeBagOfWord();
    
    char doc[10000];
    if (argc > 1)
    {
        size_t total_len = 0;
        for (int i = 1; i < argc; i++)
            total_len += strlen(argv[i]) + 1;
        
        if (total_len >= sizeof(doc))
        {
            fprintf(stderr, "Error: input too long\n");
            return 1;
        }
        
        doc[0] = '\0';
        for (int i = 1; i < argc; i++)
        {
            strcat(doc, argv[i]);
            if (i < argc - 1)
                strcat(doc, " ");
        }
    }
    else
    {
        fprintf(stderr, "usage: %s <message>\n", argv[0]);
        return 1;
    }

    // Compute prior: P(c) = Nc / Nd
    float p_spam = computeSpamProb();
    float p_ham = 1 - p_spam;

    // Start with log prior: log P(c)
    float log_spam = log(p_spam);
    float log_ham = log(p_ham);
    
    // Add likelihood: log P(d|c) = sum(log P(wi|c))
    // P(wi|c) = (Nwi,c + 1) / (sum(Nw,c) + |V|)
    compute_log_likelihood(doc, &log_spam, &log_ham);

    // Classify: argmax [log P(c) + sum(log P(wi|c))]
    printf("log P(spam|d): %.4f\n", log_spam);
    printf("log P(ham|d): %.4f\n", log_ham);
    printf("=> %s\n", log_spam > log_ham ? "SPAM" : "HAM");

    free_dict();
    return 0;
}