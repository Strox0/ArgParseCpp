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

Parser::ArgParser::ArgParser(int argc, char** argv)
{
	for (size_t i = 1; i < argc; i++)
	{
		m_tokens.emplace_back(argv[i]);
	}
}

Parser::Flag& Parser::ArgParser::AddFlag(const std::string& name, const std::vector<std::string>& aliases)
{
	if (m_locked)
		throw std::logic_error("No configuration functions can be called after ParseAndValidate");

	CheckAndRegisterNames(name, aliases);
	m_args.emplace_back(std::make_unique<Flag>());
	m_name_arg_map[name] = std::make_pair(ArgType::Flag, m_args.back());
	m_args.back()->AddName(name);
	m_args.back()->AddAliases(aliases);
	return *(Flag*)m_args.back().get();
}

void Parser::ArgParser::Help(std::string_view v)
{
	if (m_locked)
		throw std::logic_error("No configuration functions can be called after ParseAndValidate");

	if (v.empty())
		throw std::logic_error("Help message cannot be empty");

	m_help = v;
	if (m_help.back() != '\n')
		m_help += '\n';
}

Parser::ArgParseResult Parser::ArgParser::ParseAndValidate()
{
	if (m_locked)
		throw std::logic_error("No configuration functions can be called after ParseAndValidate");

	m_locked = true;

	if (m_error)
	{
		return ArgParseResult::ERROR;
	}

	bool m_pos_req = true;
	for (auto& arg : m_positionals)
	{
		if (m_pos_req && !arg->m_required)
			m_pos_req = false;
		else if (!m_pos_req && arg->m_required)
			throw std::logic_error("A Required positional cannot be after an optional one");
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
				return ArgParseResult::HELP_REQUESTED;
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
						m_error.ec = Error::MISSING_VALUE;
						m_error.arg_name = pair.first;
						FormatError();
						return ArgParseResult::ERROR;
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
							m_error.ec = Error::MISSING_VALUE;
							m_error.arg_name = pair.first;
							FormatError();
							return ArgParseResult::ERROR;
						}
					}
					else
					{
						m_error.ec = Error::MISSING_VALUE;
						m_error.arg_name = pair.first;
						FormatError();
						return ArgParseResult::ERROR;
					}
					break;
				}
				case Parser::ArgParser::ArgType::Aggregate:
				{
					if (has_value && pair.second.empty())
					{
						m_error.ec = Error::MISSING_VALUE;
						m_error.arg_name = pair.first;
						FormatError();
						return ArgParseResult::ERROR;
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
						m_error.ec = Error::MISSING_VALUE;
						m_error.arg_name = pair.first;
						FormatError();
						return ArgParseResult::ERROR;
					}
					break;
				}
				case Parser::ArgParser::ArgType::Flag:
					if (has_value)
					{
						m_error.ec = Error::FLAG_HAS_EQUALS_VALUE;
						m_error.arg_name = pair.first;
						FormatError();
						return ArgParseResult::ERROR;
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
				m_error.ec = Error::UNKNOWN_ARGUMENT;
				m_error.arg_name = pair.first;
				FormatError();
				return ArgParseResult::ERROR;
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
				m_error.ec = Error::UNKNOWN_ARGUMENT;
				m_error.arg_name = token;
				FormatError();
				return ArgParseResult::ERROR;
			}
		}
	}

	for (auto& arg : m_positionals)
	{
		arg->Finalize(m_error);
		if (m_error)
		{
			FormatError();
			return ArgParseResult::ERROR;
		}
	}

	for (auto& arg : m_args)
	{
		arg->Finalize(m_error);
		if (m_error)
		{
			FormatError();
			return ArgParseResult::ERROR;
		}
	}

	return ArgParseResult::SUCCESS;
}

std::string Parser::ArgParser::GetErrorMessage() const
{
	return m_formatted_error;
}

const Parser::Diagnostic& Parser::ArgParser::GetDiagnostics() const
{
	return m_error;
}

void Parser::ArgParser::FormatError()
{
	m_formatted_error.clear();

	const auto append_detail = [this]()
		{
			if (!m_error.opt_error_message.empty())
			{
				m_formatted_error += ": ";
				m_formatted_error += m_error.opt_error_message;
			}
		};

	switch (m_error.ec)
	{
	case Parser::Error::PARSE_FAIL:
	{
		m_formatted_error = "Error: failed to parse value";

		if (!m_error.opt_token.empty())
		{
			m_formatted_error += " '";
			m_formatted_error += m_error.opt_token;
			m_formatted_error += "'";
		}

		m_formatted_error += " for argument '";
		m_formatted_error += m_error.arg_name;
		m_formatted_error += "'";

		append_detail();
		m_formatted_error += '.';
		break;
	}

	case Parser::Error::MISSING_REQUIRED:
	{
		m_formatted_error =
			"Error: required argument '" +
			m_error.arg_name +
			"' was not provided.";
		break;
	}

	case Parser::Error::TEXT_VALIDATION_INVALID:
	{
		m_formatted_error = "Error: invalid text value";

		if (!m_error.opt_token.empty())
		{
			m_formatted_error += " '";
			m_formatted_error += m_error.opt_token;
			m_formatted_error += "'";
		}

		m_formatted_error += " for argument '";
		m_formatted_error += m_error.arg_name;
		m_formatted_error += "'";

		append_detail();
		m_formatted_error += '.';
		break;
	}

	case Parser::Error::VAL_VALIDATION_INVALID:
	{
		m_formatted_error = "Error: invalid value";

		if (!m_error.opt_token.empty())
		{
			m_formatted_error += " '";
			m_formatted_error += m_error.opt_token;
			m_formatted_error += "'";
		}

		m_formatted_error += " for argument '";
		m_formatted_error += m_error.arg_name;
		m_formatted_error += "'";

		append_detail();
		m_formatted_error += '.';
		break;
	}

	case Parser::Error::COL_VALIDATION_INVALID:
	{
		m_formatted_error = "Error: invalid collection";

		if (!m_error.opt_token.empty())
		{
			m_formatted_error += " '";
			m_formatted_error += m_error.opt_token;
			m_formatted_error += "'";
		}

		m_formatted_error += " for argument '";
		m_formatted_error += m_error.arg_name;
		m_formatted_error += "'";

		append_detail();
		m_formatted_error += '.';
		break;
	}

	case Parser::Error::TRANSFORMATION_ERROR:
	{
		m_formatted_error = "Error: could not transform value";

		if (!m_error.opt_token.empty())
		{
			m_formatted_error += " '";
			m_formatted_error += m_error.opt_token;
			m_formatted_error += "'";
		}

		m_formatted_error += " for argument '";
		m_formatted_error += m_error.arg_name;
		m_formatted_error += "'";

		append_detail();
		m_formatted_error += '.';
		break;
	}

	case Parser::Error::MISSING_VALUE:
	{
		m_formatted_error =
			"Error: argument '" +
			m_error.arg_name +
			"' requires a value.";
		break;
	}

	case Parser::Error::UNKNOWN_ARGUMENT:
	{
		m_formatted_error = "Error: unknown argument";
		m_formatted_error += " '";
		m_formatted_error += m_error.arg_name;
		m_formatted_error += "'";
		m_formatted_error += '.';
		break;
	}

	case Parser::Error::CARDINALITY_VALIDATION_FAIL:
	{
		m_formatted_error =
			"Error: argument '" +
			m_error.arg_name +
			"' ";

		switch (m_error.aggregate.card)
		{
		case Cardinality::EXACTLY:
			m_formatted_error +=
				"expects exactly " +
				std::to_string(m_error.aggregate.count) +
				" values";
			break;

		case Cardinality::ATLEAST:
			m_formatted_error +=
				"expects at least " +
				std::to_string(m_error.aggregate.min) +
				" values";
			break;

		case Cardinality::ATMOST:
			m_formatted_error +=
				"accepts at most " +
				std::to_string(m_error.aggregate.max) +
				" values";
			break;

		case Cardinality::BETWEEN:
			m_formatted_error +=
				"expects between " +
				std::to_string(m_error.aggregate.min) +
				" and " +
				std::to_string(m_error.aggregate.max) +
				" values";
			break;

		case Cardinality::UNLIMITED:
			// Reaching this state indicates an internal inconsistency.
			m_formatted_error +=
				"has invalid aggregate cardinality";
			break;
		}

		m_formatted_error +=
			", but received " +
			std::to_string(m_error.aggregate.received_count) +
			'.';

		break;
	}

	case Parser::Error::FLAG_HAS_EQUALS_VALUE:
	{
		m_formatted_error =
			"Error: flag '" +
			m_error.arg_name +
			"' does not accept a value.";

		break;
	}

	default:
	{
		m_formatted_error = "Error: an unknown command-line error occurred.";
		break;
	}
	}

	std::cout << m_formatted_error << std::endl;
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

void Parser::Flag::Finalize(Diagnostic& d)
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

Parser::Result Parser::Parse(std::string_view v, int64_t& out)
{
	return Parser::ParseInteger(v, out);
}

Parser::Result Parser::Parse(std::string_view v, uint64_t& out)
{
	return Parser::ParseInteger(v, out);
}

Parser::Result Parser::Parse(std::string_view v, int32_t& out)
{
	return Parser::ParseInteger(v, out);
}

Parser::Result Parser::Parse(std::string_view v, uint32_t& out)
{
	return Parser::ParseInteger(v, out);
}

Parser::Result Parser::Parse(std::string_view v, int16_t& out)
{
	return Parser::ParseInteger(v, out);
}

Parser::Result Parser::Parse(std::string_view v, uint16_t& out)
{
	return Parser::ParseInteger(v, out);
}

Parser::Result Parser::Parse(std::string_view v, int8_t& out)
{
	int16_t value{};

	if (!Parser::ParseInteger(v, value) ||
		value < INT8_MIN ||
		value > INT8_MAX)
	{
		return Parser::Result::Failure("result out of range");
	}

	out = static_cast<int8_t>(value);
	return Parser::Result::Success();
}

Parser::Result Parser::Parse(std::string_view v, uint8_t& out)
{
	uint16_t value{};

	if (!Parser::ParseInteger(v, value) || value > UINT8_MAX)
		return Parser::Result::Failure("result out of range");

	out = static_cast<uint8_t>(value);
	return Parser::Result::Success();
}

Parser::Result Parser::Parse(std::string_view v, double& out)
{
	return Parser::ParseFloatingPoint(v, out);
}

Parser::Result Parser::Parse(std::string_view v, long double& out)
{
	return Parser::ParseFloatingPoint(v, out);
}

Parser::Result Parser::Parse(std::string_view v, float& out)
{
	return Parser::ParseFloatingPoint(v, out);
}

Parser::Result Parser::Parse(std::string_view v, std::string& out)
{
	out.assign(v.data(), v.size());
	return Parser::Result::Success();
}