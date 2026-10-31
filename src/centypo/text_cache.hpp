#pragma once

#include <algorithm>
#include <list>
#include <map>
#include <memory>

namespace centypo::details
{

/// Bounded LRU ownership. Eviction never changes resources retained by consumers.
template <class Key, class Value>
class resource_cache
{
public:
    explicit resource_cache(std::size_t budget = 0)
        : m_budget(budget)
    {
    }

    std::shared_ptr<const Value> find(const Key& key)
    {
        const auto found = m_index.find(key);
        if(found == m_index.end())
        {
            return {};
        }

        m_entries.splice(m_entries.begin(), m_entries, found->second);

        return found->second->value;
    }

    void insert(Key key, std::shared_ptr<const Value> value, std::size_t bytes)
    {
        if(bytes > m_budget || m_budget == 0 || m_index.contains(key))
        {
            return;
        }

        while(m_bytes > m_budget - bytes)
        {
            m_bytes -= m_entries.back().bytes;
            m_index.erase(m_entries.back().key);
            m_entries.pop_back();
        }

        m_entries.push_front({std::move(key), std::move(value), bytes});
        try
        {
            m_index.emplace(m_entries.front().key, m_entries.begin());
        }
        catch(...)
        {
            m_entries.pop_front();
            throw;
        }

        m_bytes += bytes;
    }

    void set_budget(std::size_t bytes)
    {
        clear();
        m_budget = bytes;
    }

    void clear()
    {
        m_index.clear();
        m_entries.clear();
        m_bytes = 0;
    }

    std::size_t bytes() const noexcept
    {
        return m_bytes;
    }

    bool owns(const void* value) const
    {
        return std::any_of(
            m_entries.begin(),
            m_entries.end(),
            [&](const auto& entry)
            {
                return entry.value.get() == value;
            }
        );
    }

private:
    struct entry
    {
        Key key;
        std::shared_ptr<const Value> value;
        std::size_t bytes;
    };

    std::list<entry> m_entries;
    std::map<Key, typename std::list<entry>::iterator> m_index;

    std::size_t m_budget = 0;
    std::size_t m_bytes = 0;
};

} // namespace centypo::details
