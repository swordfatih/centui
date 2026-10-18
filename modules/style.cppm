module;

#include <meta>

export module centui.style;

import centui.unit;

export namespace centui
{

class style
{
public:
    template <typename type>
    static consteval style resolve(style output = {})
    {
        merge(output, type{});
        return output;
    }

    template <typename... types>
        requires(sizeof...(types) != 1)
    static consteval style resolve(style output = {})
    {
        ((output = resolve<types>(output)), ...);
        return output;
    }

    edges         padding = edges::all(px(0));
    edges         margin = edges::all(px(0));
    edges         inset = edges::all(fit());
    length        gap = px(0);
    position      position = position::relative;
    length        width = fit();
    length        height = fit();
    direction     flex_direction = direction::column;
    alignment     align_items = alignment::stretch;
    justification justify_content = justification::start;

private:
    template <typename target_type, typename source_type>
    static consteval void merge(target_type& target, const source_type& source)
    {
        constexpr auto        context = std::meta::access_context::current();
        static constexpr auto target_fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^target_type, context));
        static constexpr auto source_fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^source_type, context));

        template for(constexpr auto target_field: target_fields)
        {
            template for(constexpr auto source_field: source_fields)
            {
                if constexpr(std::meta::identifier_of(target_field) == std::meta::identifier_of(source_field))
                {
                    using target_member = typename[:std::meta::type_of(target_field):];
                    using source_member = typename[:std::meta::type_of(source_field):];

                    if constexpr(std::is_class_v<target_member> && std::is_class_v<source_member>)
                    {
                        merge(target.[:target_field:], source.[:source_field:]);
                    }
                    else
                    {
                        target.[:target_field:] = source.[:source_field:];
                    }
                }
            }
        }
    }
};
} // namespace centui