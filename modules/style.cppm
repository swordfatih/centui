module;

#include <meta>

export module centui.style;

export namespace centui
{
class style
{
public:
    template <typename type>
    consteval static style resolve(centui::style output = {})
    {
        constexpr auto        context = std::meta::access_context::current();
        static constexpr auto style_fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^centui::style, context));
        static constexpr auto source_fields = std::define_static_array(std::meta::nonstatic_data_members_of(^^type, context));

        type source{};
        template for(constexpr auto style_field: style_fields)
        {
            template for(constexpr auto source_field: source_fields)
            {
                if(std::meta::identifier_of(style_field) == std::meta::identifier_of(source_field))
                {
                    output.[:style_field:] = source.[:source_field:];
                }
            }
        }

        return output;
    }

    template <typename... types>
        requires(sizeof...(types) != 1)
    static consteval style resolve(centui::style output = {})
    {
        ((output = resolve<types>(output)), ...);
        return output;
    }

    int gap;
    int padding;
};
} // namespace centui