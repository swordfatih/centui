#pragma once

#include <map>

#include <centui/render.hpp>

namespace centui::details
{

struct texture_cache_statistics
{
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t uploads = 0;
    std::size_t evictions = 0;
    std::size_t bytes = 0;
    std::size_t peak_bytes = 0;
};

// Eviction runs before submission. The current list's textures remain resident
// even when its working set exceeds the retained budget.
template <typename Texture>
class texture_cache
{
    using key = std::weak_ptr<const bitmap>;

    struct usage
    {
        std::size_t bytes;
        std::size_t tick;
    };

    std::map<key, Texture, std::owner_less<>> m_values;
    std::map<key, usage, std::owner_less<>> m_usage;
    std::size_t m_tick = 0;
    texture_cache_statistics m_stats;

public:
    auto end()
    {
        return m_values.end();
    }

    auto find(const key& value)
    {
        auto found = m_values.find(value);
        if(found != m_values.end())
        {
            ++m_stats.hits;
            m_usage.at(value).tick = ++m_tick;
        }
        else
        {
            ++m_stats.misses;
        }
        return found;
    }

    auto emplace(const key& value, Texture texture)
    {
        const auto image = value.lock();
        const auto bytes = static_cast<std::size_t>(image->width) * image->height * 4;
        m_usage.insert_or_assign(value, usage{bytes, ++m_tick});
        try
        {
            auto result = m_values.emplace(value, std::move(texture));
            m_stats.bytes += bytes;
            ++m_stats.uploads;
            m_stats.peak_bytes = std::max(m_stats.peak_bytes, m_stats.bytes);
            return result;
        }
        catch(...)
        {
            m_usage.erase(value);
            throw;
        }
    }

    void clear()
    {
        m_values.clear();
        m_usage.clear();
        m_stats.bytes = 0;
    }

    auto statistics() const
    {
        return m_stats;
    }

    void begin_frame(const draw_list& list, std::size_t count, std::size_t budget)
    {
        std::map<key, bool, std::owner_less<>> active;
        std::size_t incoming = 0;
        std::size_t incoming_count = 0;
        for(const auto& command: list.commands)
        {
            if(command.image && active.emplace(command.image, true).second && !m_values.contains(command.image))
            {
                validate_bitmap(*command.image);
                incoming += static_cast<std::size_t>(command.image->width) * command.image->height * 4;
                ++incoming_count;
            }
        }
        for(auto it = m_usage.begin(); it != m_usage.end();)
        {
            if(it->first.expired())
            {
                m_stats.bytes -= it->second.bytes;
                m_values.erase(it->first);
                it = m_usage.erase(it);
                ++m_stats.evictions;
            }
            else
            {
                ++it;
            }
        }
        while(m_stats.bytes + incoming > budget || m_values.size() + incoming_count > count)
        {
            auto oldest = m_usage.end();
            for(auto it = m_usage.begin(); it != m_usage.end(); ++it)
            {
                if(!active.contains(it->first) && (oldest == m_usage.end() || it->second.tick < oldest->second.tick))
                {
                    oldest = it;
                }
            }
            if(oldest == m_usage.end())
            {
                break;
            }
            m_stats.bytes -= oldest->second.bytes;
            m_values.erase(oldest->first);
            m_usage.erase(oldest);
            ++m_stats.evictions;
        }
    }
};

} // namespace centui::details
