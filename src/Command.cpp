#include "Command.hpp"
#include <cctype>

Command::Command()
    : _prefix(), _name(), _params(), _hasPrefix(false), _hasTrailingParam(false)
{}

Command::ParseStatus Command::parse(const std::string &line, Command &out)
{
    // Based on RFC2812, line limit is 512bytes
    if (line.size() > 510)
        return (PARSE_LINE_TOO_LONG);
    if (line.empty() || line.find_first_of("\r\n") != std::string::npos || line.find('\0') != std::string::npos)
        return (PARSE_MALFORMED);

    Command                 parsed;
    std::string::size_type  i = 0;

// check and copy _prefix
    if (line[0] == ':')
    {
        std::string::size_type prefixEnd = line.find(' ', i);

        if (prefixEnd == std::string::npos || prefixEnd == 1)
            return (PARSE_MALFORMED);
        parsed._prefix = line.substr(1, prefixEnd - 1);
        for (std::string::size_type j = 0; j < parsed._prefix.size(); ++j)
        {
            unsigned char c = static_cast<unsigned char>(parsed._prefix[j]);

            if (c <= 0x20 || c == 0x7f || c == ':')
                return PARSE_MALFORMED;
        }
        parsed._hasPrefix = true;
        i = prefixEnd + 1;
        if (i == line.size() || line[i] == ' ')
            return (PARSE_MALFORMED);
    }

// check and copy _name
    std::string::size_type commandEnd = line.find(' ', i);
    if (commandEnd == std::string::npos)
        commandEnd = line.size();
    if (commandEnd == i)
        return PARSE_MALFORMED;

    parsed._name = line.substr(i, commandEnd - i);

    bool allLetters = true;
    bool threeDigits = (parsed._name.size() == 3);

    for (std::string::size_type j = 0; j < parsed._name.size(); ++j)
    {
        unsigned char c = static_cast<unsigned char>(parsed._name[j]);

        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
            allLetters = false;
        if (c < '0' || c > '9')
            threeDigits = false;

        parsed._name[j] = static_cast<char>(std::toupper(c));
    }

    if (!allLetters && !threeDigits)
        return PARSE_MALFORMED;

    i = commandEnd;

// check and copy _params
    std::string::size_type paramCount = 0;

    while (i < line.size())
    {
        if (line[i] != ' ')
            return PARSE_MALFORMED;

        ++i;
        if (i == line.size() || line[i] == ' ')
            return PARSE_MALFORMED;

        if (line[i] == ':')
        {
            parsed._params.push_back(line.substr(i + 1));
            parsed._hasTrailingParam = true;
            ++paramCount;

            if (paramCount > 15)
                return PARSE_TOO_MANY_PARAMS;

            i = line.size();
            break;
        }

        std::string::size_type paramEnd = line.find(' ', i);

        if (paramEnd == std::string::npos)
            paramEnd = line.size();

        parsed._params.push_back(line.substr(i, paramEnd - i));
        ++paramCount;

        if (paramCount > 15)
            return PARSE_TOO_MANY_PARAMS;

        i = paramEnd;
    }

    out = parsed;
    return PARSE_SUCCESS;
}

const std::string   &Command::getPrefix() const
{
    return (_prefix);
}

bool    Command::hasPrefix() const
{
    return (_hasPrefix);
}

const std::string   &Command::getName() const
{
    return (_name);
}

const std::vector<std::string>  &Command::getParams() const
{
    return (_params);
}

bool    Command::hasTrailingParam() const
{
    return (_hasTrailingParam);
}