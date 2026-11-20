#ifndef COMMAND_HPP
# define COMMAND_HPP

# include <string>
# include <vector>

class Command
{
    public:
        enum    ParseStatus
        {
            PARSE_SUCCESS,
            PARSE_MALFORMED,
            PARSE_LINE_TOO_LONG,
            PARSE_TOO_MANY_PARAMS
        };

        Command();

        static ParseStatus  parse(const std::string &line, Command &out);

        const std::string               &getPrefix() const;
        bool                            hasPrefix() const;
        const std::string               &getName() const;
        const std::vector<std::string>  &getParams() const;
        bool                            hasTrailingParam() const;

    private:
        std::string                 _prefix;
        std::string                 _name;
        std::vector<std::string>    _params;
        bool                        _hasPrefix;
        bool                        _hasTrailingParam;
};

#endif
