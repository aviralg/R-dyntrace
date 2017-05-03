#ifndef R_3_3_1_FILTER_H
#define R_3_3_1_FILTER_H

#include <string.h>

/*
#ifdef __cplusplus
#include <unordered_set>

struct filter_t
{
public:
    bool Init(const char *filename);
    void AddItem(const char *item);
    int ContainsItem(const char *item) const;
    void Print() const;
private:
    std::unordered_set<std::string> m_filter_set;
};
#else
 */
#include "../../hashset/hashset.h"
#include <stdio.h>

struct filter_t
{
    hashset_t m_filter_set;
};

int Init(void *filter, const char *filename);
void AddItem(void *filter, const char *item);
int ContainsItem(void *filter, const char *item);
void Print(void *filter);
//#endif

#ifdef __cplusplus
extern "C" {
#endif

void *get_filter(const char *filename);
void destroy_filter(void *filter);
void add_item(void *filter, const char *item);
int contains_item(void *filter, const char *item);
void print_filter(void *filter);

#ifdef __cplusplus
}
#endif

#endif //R_3_3_1_FILTER_H
