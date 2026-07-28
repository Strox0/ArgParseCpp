#include "ArgParser.h"
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

Parser::ArgParser::ArgParser(int argc, char** argv) : m_error(Parser::Error::SUCCESS)
{
	for (size_t i = 1; i < argc; i++)
	{
		m_tokens.emplace_back(argv[i]);
	}
}

Parser::Flag& Parser::ArgParser::AddFlag(const std::string& name, const std::vector<std::string>& aliases)
{
	CheckAndRegisterNames(name, aliases);
	m_args.emplace_back(std::make_unique<Flag>());
	if (m_error == Error::SUCCESS)
	{
		m_name_arg_map[name] = std::make_pair(ArgType::Flag, m_args.back());
		m_args.back()->AddName(name);
		m_args.back()->AddAliases(aliases);
	}
	return *(Flag*)m_args.back().get();
}

void Parser::ArgParser::Help(std::string_view v)
{
	if (v.empty())
		return;

	m_help = v;
	if (m_help.back() != '\n')
		m_help += '\n';
}

Parser::Error Parser::ArgParser::ParseAndValidate()
{
	if (m_error != Parser::Error::SUCCESS)
	{
		return HandleError();
	}

	size_t positionals_index = 0;
	bool options_end = false;
	for (auto curr = m_tokens.begin(); curr < m_tokens.end(); curr++)
	{
		const std::string& token = *curr;
		if (!options_end)
		{
			if (token == "--")
			{
				options_end = true;
				continue;
			}
			else if (token == "--help" || token == "-h")
			{
				m_error = Error::HELP_QUERY;
				return HandleError();
			}

			bool has_value = false;
			std::pair<std::string, std::string> pair = ParseTokenWithEquals(token, has_value);

			if (ResolveName(pair.first))
			{
				ArgumentBase& arg = *m_name_arg_map.at(pair.first).second;
				switch (m_name_arg_map.at(pair.first).first)
				{
				case Parser::ArgParser::ArgType::Scalar:
				{
					if (has_value && pair.second.empty())
					{
						m_error = Error::MISSING_VALUE;
						return HandleError();
					}

					if (has_value)
					{
						arg.AddValue(pair.second);
					}
					else if (curr + 1 != m_tokens.end())
					{
						curr++;
						if (*curr != "--" && !IsKnownName(ParseTokenWithEquals(*curr, has_value).first))
						{
							arg.AddValue(*curr);
						}
						else
						{
							curr--;
							m_error = Error::MISSING_VALUE;
							return HandleError();
						}
					}
					else
					{
						m_error = Error::MISSING_VALUE;
						return HandleError();
					}
					break;
				}
				case Parser::ArgParser::ArgType::Aggregate:
				{
					if (has_value && pair.second.empty())
					{
						m_error = Error::MISSING_VALUE;
						return HandleError();
					}

					bool found_value = has_value;
					if (has_value)
					{
						arg.AppendValue(pair.second);
					}

					while (true)
					{
						curr++;
						if (curr == m_tokens.end() || *curr == "--" || IsKnownName(ParseTokenWithEquals(*curr, has_value).first))
						{
							curr--;
							break;
						}
						found_value = true;

						arg.AppendValue(*curr);
					}

					if (!found_value)
					{
						m_error = Error::MISSING_VALUE;
						return HandleError();
					}
					break;
				}
				case Parser::ArgParser::ArgType::Flag:
					if (has_value)
					{
						m_error = Error::FLAG_HAS_EQUALS_VALUE;
						return HandleError();
					}
					((Flag&)arg).SetTrue();
					break;
				}
			}
			else if (positionals_index < m_positionals.size())
			{
				m_positionals[positionals_index]->AddValue(token);
				positionals_index++;
			}
			else
			{
				m_error = Parser::Error::UNKNOWN_VALUE;
				return HandleError();
			}
		}
		else
		{
			if (positionals_index < m_positionals.size())
			{
				m_positionals[positionals_index]->AddValue(token);
				positionals_index++;
			}
			else
			{
				m_error = Parser::Error::UNKNOWN_VALUE;
				return HandleError();
			}
		}
	}

	bool m_pos_req = true;
	for (auto& arg : m_positionals)
	{
		if (m_pos_req && !arg->m_required)
			m_pos_req = false;
		else if (!m_pos_req && arg->m_required)
		{
			throw std::logic_error("A Required positional cannot be after an optional one");
			return HandleError();
		}

		arg->Finalize();
		m_error = arg->GetError();
		if (m_error != Parser::Error::SUCCESS)
		{
			return HandleError();
		}
	}

	for (auto& arg : m_args)
	{
		arg->Finalize();
		m_error = arg->GetError();
		if (m_error != Parser::Error::SUCCESS)
		{
			return HandleError();
		}
	}

	return Parser::Error::SUCCESS;
}

Parser::Error Parser::ArgParser::HandleError()
{
	std::cerr << "Arg Error: " << (int)m_error << std::endl;
	return m_error;
}

void Parser::ArgParser::CheckAndRegisterNames(const std::string& name, const std::vector<std::string>& al)
{
	if (name.empty())
		throw std::logic_error("Missing argument name");

	if (m_name_arg_map.contains(name) || name == "--help" || name == "-h" || name == "--" || m_alias_map.contains(name))
		throw std::logic_error("Argument name already used");

	if (name.find('=') != name.npos)
		throw std::logic_error("Name contains '='");

	if (!al.empty())
	{
		std::unordered_set<std::string> tmp;
		tmp.insert("--help");
		tmp.insert("-h");
		tmp.insert("--");
		tmp.insert(name);
		for (const auto& n : al)
		{
			if (tmp.contains(n) || m_name_arg_map.contains(n) || m_alias_map.contains(n))
				throw std::logic_error("Argument alias already used");
			else if (n.empty())
				throw std::logic_error("Alias is an empty string");
			else if (n.find('=') != n.npos)
				throw std::logic_error("Alias contains '='");
			else
				tmp.insert(n);
		}

		for (auto& n : al)
		{
			m_alias_map[n] = name;
		}
	}
}

bool Parser::ArgParser::IsKnownName(const std::string& name)
{
	if (m_alias_map.contains(name))
	{
		std::string s = m_alias_map[name];
		return m_name_arg_map.contains(s);
	}
	else
	{
		return m_name_arg_map.contains(name);
	}
}

bool Parser::ArgParser::ResolveName(std::string& name)
{
	if (m_alias_map.contains(name))
		name = m_alias_map[name];

	return m_name_arg_map.contains(name);
}

std::pair<std::string, std::string> Parser::ArgParser::ParseTokenWithEquals(const std::string& token, bool& has_value)
{
	size_t i = token.find('=');
	if (i != token.npos)
	{
		has_value = true;
		return { token.substr(0, i), token.substr(i+1) };
	}
	else
	{
		has_value = false;
		return { token,std::string() };
	}
}

Parser::Error Parser::ArgumentBase::GetError() const
{
	return m_error;
}

std::string_view Parser::ArgumentBase::GetHelp() const
{
	return m_help;
}

void Parser::ArgumentBase::AddName(std::string_view name)
{
	m_name = name;
}

void Parser::ArgumentBase::AddAliases(const std::vector<std::string>& aliases)
{
	if (!aliases.empty())
		m_aliases = aliases;
}

std::string_view Parser::ArgumentBase::GetName() const
{
	return m_name;
}

const std::vector<std::string>& Parser::ArgumentBase::GetAliases() const
{
	return m_aliases;
}

void Parser::ArgumentBase::AddValue(std::string_view value)
{
	if (m_required && m_error == Error::MISSING_REQUIRED)
		m_error = Error::SUCCESS;

	m_set = true;
	m_value = value;
}

bool Parser::Flag::Value() const
{
	return m_state;
}

Parser::Flag& Parser::Flag::Help(std::string_view help_msg)
{
	if (m_locked)
		throw std::logic_error("Argument cannot be configured after ParseAndValidate");

	if (help_msg.empty())
		throw std::logic_error("Help message cannot be empty");

	m_help = help_msg;
	if (m_help.back() != '\n')
		m_help += '\n';
	return *this;
}

void Parser::Flag::Finalize()
{
	if (m_locked || m_error != Error::SUCCESS)
		return;

	m_locked = true;
}

void Parser::Flag::SetTrue()
{
	if (!m_locked)
		m_state = true;
}

bool Parser::Parse(std::string_view v, int64_t& out)
{
	return Parser::ParseInteger(v, out);
}

bool Parser::Parse(std::string_view v, uint64_t& out)
{
	return Parser::ParseInteger(v, out);
}

bool Parser::Parse(std::string_view v, int32_t& out)
{
	return Parser::ParseInteger(v, out);
}

bool Parser::Parse(std::string_view v, uint32_t& out)
{
	return Parser::ParseInteger(v, out);
}

bool Parser::Parse(std::string_view v, int16_t& out)
{
	return Parser::ParseInteger(v, out);
}

bool Parser::Parse(std::string_view v, uint16_t& out)
{
	return Parser::ParseInteger(v, out);
}

bool Parser::Parse(std::string_view v, int8_t& out)
{
	int16_t value{};

	if (!Parser::ParseInteger(v, value) ||
		value < INT8_MIN ||
		value > INT8_MAX)
	{
		return false;
	}

	out = static_cast<int8_t>(value);
	return true;
}

bool Parser::Parse(std::string_view v, uint8_t& out)
{
	uint16_t value{};

	if (!Parser::ParseInteger(v, value) || value > UINT8_MAX)
		return false;

	out = static_cast<uint8_t>(value);
	return true;
}

bool Parser::Parse(std::string_view v, double& out)
{
	return Parser::ParseFloatingPoint(v, out);
}

bool Parser::Parse(std::string_view v, long double& out)
{
	return Parser::ParseFloatingPoint(v, out);
}

bool Parser::Parse(std::string_view v, float& out)
{
	return Parser::ParseFloatingPoint(v, out);
}

bool Parser::Parse(std::string_view v, std::string& out)
{
	out.assign(v.data(), v.size());
	return true;
}