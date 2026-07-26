#include "ArgParser.h"
#include <iostream>

Parser::ArgParser::ArgParser(int argc, char** argv) : m_error(Parser::Error::SUCCESS)
{
	for (int i = 1; i < argc; i++)
	{
		std::string token = argv[i];
		size_t index = token.find('=');
		if (index != token.npos)
		{
			std::string t1 = token.substr(0, index);
			std::string t2 = token.substr(index + 1);
			if (!t1.empty())
				m_tokens.emplace_back(t1);
			if (!t2.empty())
				m_tokens.emplace_back(t2);
		}
		else
			m_tokens.emplace_back(argv[i]);
	}
}

Parser::Flag& Parser::ArgParser::AddFlag(std::string_view name, const std::vector<std::string>& aliases)
{
	std::vector<std::string> names;
	names.push_back(std::string(name));
	names.insert(names.end(), aliases.begin(), aliases.end());

	for (const auto& n : names)
	{
		if (m_names.contains(std::string(n)))
		{
			m_error = Error::NAME_ALREADY_USED;
			m_args.emplace_back(std::make_unique<Flag>());
			return *(Flag*)m_args.back().get();
		}
		else
			m_names.insert(std::string(n));
	}

	auto it = std::find(m_tokens.begin(), m_tokens.end(), name);
	if (!aliases.empty() && it == m_tokens.end())
	{
		for (const auto& n : aliases)
		{
			if (it != m_tokens.end())
				break;
			it = std::find(m_tokens.begin(), m_tokens.end(), n);
		}
	}

	if (it == m_tokens.end())
	{
		m_args.emplace_back(std::make_unique<Flag>());
	}
	else
	{
		m_args.emplace_back(std::make_unique<Flag>(true));
		m_tokens.erase(it);
	}

	return *(Flag*)m_args.back().get();
}

void Parser::ArgParser::Help(std::string_view v)
{
	m_help = v;
	if (m_help.back() != '\n')
		m_help += '\n';
}

Parser::Error Parser::ArgParser::ValidateArgs(bool exit)
{
	if (m_error != Parser::Error::SUCCESS)
	{
		return HandleError(exit);
	}

	if (std::find(m_tokens.begin(), m_tokens.end(), "--help") != m_tokens.end() ||
		std::find(m_tokens.begin(), m_tokens.end(), "-h") != m_tokens.end()
		)
	{
		std::cout << m_help;

		for (auto& arg : m_args)
		{
			std::cout << arg->GetHelp();
		}

		if (exit)
			std::exit(0);
		else
			return Error::HELP_QUERY;
	}

	for (auto& arg : m_args)
	{
		auto it = std::find(m_tokens.begin(), m_tokens.end(), arg->GetName());
		if (!arg->GetAliases().empty() && it == m_tokens.end())
		{
			for (const auto& n : arg->GetAliases())
			{
				if (it != m_tokens.end())
					break;
				it = std::find(m_tokens.begin(), m_tokens.end(), n);
			}
		}

		if (it != m_tokens.end())
		{
			if (it + 1 != m_tokens.end())
			{
				if (m_names.contains(*(it + 1)))
				{
					m_error = Error::MISSING_VALUE;
					return HandleError(exit);
				}
				else
				{
					arg->AddValue(*(it + 1));
					m_tokens.erase(it, it + 2);
				}
			}
			else
			{
				m_error = Error::MISSING_VALUE;
				return HandleError(exit);
			}
		}
	}

	for (auto& arg : m_aggregates)
	{
		std::vector<std::string_view> names;
		names.push_back(arg->GetName());
		names.insert(names.end(), arg->GetAliases().begin(), arg->GetAliases().end());
		for (auto& name : names)
		{
			while (true)
			{
				auto it = std::find(m_tokens.begin(), m_tokens.end(), name);
				if (it != m_tokens.end())
				{
					std::vector<std::string>::iterator t = it;
					while (true)
					{
						t++;
						if (t == m_tokens.end() || m_names.contains(*t))
							break;
						else
							arg->AppendValue(*t);
					}
					m_tokens.erase(it, t);
				}
				else
					break;
			}
		}
	}

	for (size_t i = 0; i < m_positionals.size(); i++)
	{
		std::string t;
		if (!m_tokens.empty())
		{
			t = m_tokens.front();
			m_tokens.erase(m_tokens.begin());
		}
		else
			break;

		m_positionals[i]->AddValue(t);
	}
	
	if (!m_tokens.empty())
	{
		m_error = Parser::Error::UNKNOWN_VALUE;
		return HandleError(exit);
	}

	for (auto& arg : m_aggregates)
	{
		arg->Finalize();
		m_error = arg->GetError();
		if (m_error != Parser::Error::SUCCESS)
		{
			return HandleError(exit);
		}
	}

	bool m_pos_req = true;
	for (auto& arg : m_positionals)
	{
		if (m_pos_req && !arg->m_required)
			m_pos_req = false;
		else if (!m_pos_req && arg->m_required)
		{
			m_error = Error::REQ_POS_AFTER_OPTIONAL;
			return HandleError(exit);
		}

		arg->Finalize();
		m_error = arg->GetError();
		if (m_error != Parser::Error::SUCCESS)
		{
			return HandleError(exit);
		}
	}

	for (auto& arg : m_args)
	{
		arg->Finalize();
		m_error = arg->GetError();
		if (m_error != Parser::Error::SUCCESS)
		{
			return HandleError(exit);
		}
	}

	return Parser::Error::SUCCESS;
}

Parser::Error Parser::ArgParser::HandleError(bool exit)
{
	std::cout << "Arg Error: " << (int)m_error << std::endl;
	if (exit)
		std::exit(1);
	return m_error;
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
	m_help = help_msg;
	if (m_help.back() != '\n')
		m_help += '\n';
	return *this;
}

void Parser::Flag::Finalize()
{
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