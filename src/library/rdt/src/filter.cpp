#include "filter.h"

/*
#ifdef __cplusplus
#include <fstream>
#include <iostream>

bool filter_t::Init(const char *filename)
{
    std::string line;
    std::ifstream file;

    file.open(filename);
    if (file.is_open())
    {
        while (getline(file, line))
        {
            m_filter_set.insert(line);
        }
    }
    else
    {
        return false;
    }
    file.close();
    return true;
}

void filter_t::AddItem(const char *item)
{
    m_filter_set.insert(std::string(item));
}

int filter_t::ContainsItem(const char *item) const
{
    return m_filter_set.find(std::string(item)) != m_filter_set.end();
}

void filter_t::Print() const
{
    for (const auto& elem : m_filter_set)
    {
        std::cout << elem << "\n";
    }
}

extern "C"
{
    void *get_filter(const char *filename) {
        filter_t *filter = new filter_t;

        if (filter->Init(filename)) {
            return reinterpret_cast<void *>(filter);
        } else {
            return nullptr;
        }
    }

    void destroy_filter(void *filter) {
        if (filter) {
            delete reinterpret_cast<filter_t *>(filter);
        }
    }

    void add_item(void *filter, const char *item) {
        if (filter) {
            reinterpret_cast<filter_t *>(filter)->AddItem(item);
        }
    }

    int contains_item(void *filter, const char *item) {
        if (filter && item && strlen(item)) {
            return reinterpret_cast<filter_t *>(filter)->ContainsItem(item);
        }
        else {
            return 0;
        }
    }

    void print_filter(void *filter) {
        reinterpret_cast<filter_t *>(filter)->Print();
    }
}

#else
*/

int Init(void *filter, const char *filename)
{
    FILE *fp;
    size_t len = 0;
    ssize_t read;
    char * line = NULL;

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        return false;
    }

    filter_t *f = (filter_t*)filter;
    while ((read = getline(&line, &len, fp)) != -1)
    {
        hashset_add(f->m_filter_set, line);
    }

    fclose(fp);
}

void AddItem(void *filter, const char *item)
{
    filter_t *f = (filter_t*)filter;
    hashset_add(f->m_filter_set, (void *)item);
}

int ContainsItem(void *filter, const char *item)
{
    filter_t *f = (filter_t*)filter;
    return hashset_is_member(f->m_filter_set, (void*)item);
}

void Print(void *filter)
{

}

void *get_filter(const char *filename) {
    filter_t *filter = (filter_t*)malloc(sizeof(filter_t));
    filter->m_filter_set = hashset_create();

    if (Init(filter, filename)) {
        (void*)(filter);
    } else {
        return NULL;
    }
}

void destroy_filter(void *filter) {
    if (filter) {
        filter_t *f = (filter_t*)filter;
        hashset_destroy(f->m_filter_set);
        free(f);
    }
}

void add_item(void *filter, const char *item) {
    if (filter) {
        AddItem(filter, item);
    }
}

int contains_item(void *filter, const char *item) {
    if (filter && item && strlen(item)) {
        return ContainsItem(filter, item);
    }
    else {
        return 0;
    }
}

void print_filter(void *filter) {
    Print(filter);
}
//#endif
