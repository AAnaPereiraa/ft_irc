#include "Client.hpp"

Client::Client()
    : _nickname(), _username(), _realName(), _passwordAccepted(false), _hasUserInfo(false), _registered(false)
{}

bool Client::hasAcceptedPassword() const
{
    return _passwordAccepted;
}

void Client::markPasswordAccepted()
{
    _passwordAccepted = true;
}

bool Client::hasNickname() const
{
    return !_nickname.empty();
}

const std::string &Client::getNickname() const
{
    return _nickname;
}

void Client::setNickname(const std::string &nickname)
{
    _nickname = nickname;
}

bool Client::hasUserInfo() const
{
    return _hasUserInfo;
}

const std::string &Client::getUsername() const
{
    return _username;
}

const std::string &Client::getRealName() const
{
    return _realName;
}

void Client::setUserInfo(const std::string &username,
    const std::string &realName)
{
    _username = username;
    _realName = realName;
    _hasUserInfo = true;
}

bool Client::isRegistered() const
{
    return _registered;
}

bool Client::tryCompleteRegistration()
{
    if (_registered || !_passwordAccepted || _nickname.empty()
        || !_hasUserInfo)
        return false;

    _registered = true;
    return true;
}
