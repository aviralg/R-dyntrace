#ifndef R_3_3_1_FILTER_H
#define R_3_3_1_FILTER_H

#ifdef __cplusplus
#include <cstring>
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
#endif

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
