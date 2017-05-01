#include "filter.h"

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
        delete reinterpret_cast<filter_t *>(filter);
    }

    void add_item(void *filter, const char *item) {
        reinterpret_cast<filter_t *>(filter)->AddItem(item);
    }

    int contains_item(void *filter, const char *item) {
        return reinterpret_cast<filter_t *>(filter)->ContainsItem(item);
    }

    void print_filter(void *filter) {
        return reinterpret_cast<filter_t *>(filter)->Print();
    }

}