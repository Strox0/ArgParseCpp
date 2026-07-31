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
#include <sstream>
#include <cstddef>
#include <iomanip>
#include <utility>

Parser::ArgParser::ArgParser(int argc, char** argv)
{
	m_exe_name = argv[0];
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

	m_help = v;
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

	bool disallow_req = false;
	for (auto& arg : m_positionals)
	{
		if (m_pos_aggregate)
		{
			if (m_pos_aggregate->IsRequired() && !arg->IsRequired())
				throw std::logic_error("A Required positional cannot be after an optional one");
		}

		if (disallow_req && arg->IsRequired())
			throw std::logic_error("A Required positional cannot be after an optional one");
		else if (!arg->IsRequired())
			disallow_req = true;
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
				LockAndFailArgs();
				return ArgParseResult::HELP_REQUESTED;
			}

			bool has_value = false;
			std::pair<std::string, std::string> pair = ParseTokenWithEquals(token, has_value);
			std::string original_arg_name = pair.first;
			if (ResolveName(pair.first))
			{
				ArgumentBase& arg = *m_name_arg_map.at(pair.first).second;
				arg.AddName(original_arg_name);
				switch (m_name_arg_map.at(pair.first).first)
				{
				case Parser::ArgParser::ArgType::Scalar:
				{
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
							m_error.arg_name = original_arg_name;
							FormatError();
							return ArgParseResult::ERROR;
						}
					}
					else
					{
						m_error.ec = Error::MISSING_VALUE;
						m_error.arg_name = original_arg_name;
						FormatError();
						return ArgParseResult::ERROR;
					}
					break;
				}
				case Parser::ArgParser::ArgType::Aggregate:
				{
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
						m_error.arg_name = original_arg_name;
						FormatError();
						return ArgParseResult::ERROR;
					}
					break;
				}
				case Parser::ArgParser::ArgType::Flag:
					if (has_value)
					{
						m_error.ec = Error::FLAG_HAS_EQUALS_VALUE;
						m_error.arg_name = original_arg_name;
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
			else if (m_pos_aggregate)
			{
				m_pos_aggregate->AppendValue(token);
			}
			else
			{
				m_error.ec = Error::UNKNOWN_ARGUMENT;
				m_error.arg_name = original_arg_name;
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
			else if (m_pos_aggregate)
			{
				m_pos_aggregate->AppendValue(token);
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
			m_error.positional = true;
			FormatError();
			return ArgParseResult::ERROR;
		}
	}

	if (m_pos_aggregate)
	{
		m_pos_aggregate->Finalize(m_error);
		if (m_error)
		{
			m_error.positional = true;
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

std::string Parser::ArgParser::GetHelpMessage(size_t max_line_width, size_t max_label_width) const
{
	struct HelpRow
	{
		std::string label;
		std::string description;
	};

	struct OptionRow
	{
		std::string canonical_name;
		HelpRow help;
	};

	const auto wrap_text = [](std::string_view text, std::size_t width)
		{
			std::vector<std::string> result;
			std::size_t paragraph_begin = 0;

			while (paragraph_begin <= text.size())
			{
				const std::size_t newline = text.find('\n', paragraph_begin);
				const std::size_t paragraph_end = newline == std::string_view::npos ? text.size() : newline;

				const std::string_view paragraph = text.substr(paragraph_begin, paragraph_end - paragraph_begin);

				std::istringstream words{ std::string(paragraph) };
				std::string word;
				std::string line;

				while (words >> word)
				{
					if (line.empty())
					{
						line = word;
					}
					else if (line.size() + 1 + word.size() <= width)
					{
						line += ' ';
						line += word;
					}
					else
					{
						result.push_back(std::move(line));
						line = word;
					}
				}

				if (!line.empty())
				{
					result.push_back(std::move(line));
				}
				else if (paragraph.empty())
				{
					result.emplace_back();
				}

				if (newline == std::string_view::npos)
				{
					break;
				}

				paragraph_begin = newline + 1;
			}

			return result;
		};

	const auto format_meta = [](const ArgumentBase& argument, std::string_view fallback)
		{
			std::string meta = argument.GetMeta().empty() ? std::string(fallback) : std::string(argument.GetMeta());

			if (meta.size() >= 2 &&
				meta.front() == '<' &&
				meta.back() == '>')
			{
				return meta;
			}
			else
			{
				meta.insert(meta.begin(), '<');
				meta.push_back('>');
			}

			return meta;
		};

	const auto make_description = [](const ArgumentBase& argument)
		{
			std::string description{ argument.GetHelp() };

			if (argument.IsRequired())
			{
				if (!description.empty())
				{
					description += ' ';
				}

				description += "(required)";
			}

			return description;
		};

	const auto make_option_label = [](const std::string& name, const ArgumentBase& argument)
		{
			std::string label = name;

			for (const std::string& alias : argument.GetAliases())
			{
				label += ", ";
				label += alias;
			}

			return label;
		};

	const auto append_rows = [&](std::ostringstream& output, const std::vector<HelpRow>& rows)
		{
			if (rows.empty())
			{
				return;
			}

			constexpr std::size_t indent = 2;
			constexpr std::size_t gap = 2;

			std::size_t label_width = 0;

			for (const HelpRow& row : rows)
			{
				label_width = std::max(label_width, row.label.size());
			}

			label_width = std::min(label_width, max_label_width);

			const std::size_t description_column = indent + label_width + gap;

			const std::size_t description_width = description_column < max_line_width ? max_line_width - description_column	: 1;

			for (const HelpRow& row : rows)
			{
				if (row.description.empty())
				{
					output << std::string(indent, ' ') << row.label << '\n';
					continue;
				}

				const std::vector<std::string> description_lines = wrap_text(row.description, description_width);

				if (row.label.size() <= label_width)
				{
					output
						<< std::string(indent, ' ')
						<< std::left
						<< std::setw(static_cast<int>(label_width))
						<< row.label
						<< std::right
						<< std::string(gap, ' ');

					if (!description_lines.empty())
					{
						output << description_lines.front();
					}

					output << '\n';
				}
				else
				{
					output << std::string(indent, ' ') << row.label << '\n';

					if (!description_lines.empty())
					{
						output << std::string(description_column, ' ') << description_lines.front()	<< '\n';
					}
				}

				for (std::size_t i = 1; i < description_lines.size(); ++i)
				{
					output << std::string(description_column, ' ') << description_lines[i] << '\n';
				}
			}
		};

	std::vector<std::string> required_options;
	std::vector<std::string> optional_options;
	std::vector<OptionRow> option_rows;

	optional_options.emplace_back("[--help]");
	option_rows.push_back({
		"--help",
		{
			"--help, -h",
			"Show this help message."
		}
		});

	for (const auto& [canonical_name, typed_argument] : m_name_arg_map)
	{
		const ArgType type = typed_argument.first;
		const ArgumentBase& argument = *typed_argument.second;

		std::string usage = canonical_name;
		std::string label = make_option_label(canonical_name, argument);

		if (type != ArgType::Flag)
		{
			const std::string meta = format_meta(argument, "value");

			usage += ' ';
			usage += meta;

			label += ' ';
			label += meta;

			if (type == ArgType::Aggregate)
			{
				usage += "...";
				label += "...";
			}
		}

		if (argument.IsRequired())
		{
			required_options.push_back(std::move(usage));
		}
		else
		{
			optional_options.push_back("[" + usage + "]");
		}

		option_rows.push_back({
			canonical_name,
			{
				std::move(label),
				make_description(argument)
			}
			});
	}

	std::vector<std::string> usage_parts;

	usage_parts.reserve(required_options.size() + optional_options.size() + m_positionals.size() + (m_pos_aggregate ? 1 : 0));

	usage_parts.insert(usage_parts.end(), required_options.begin(),	required_options.end());
	usage_parts.insert(usage_parts.end(), optional_options.begin(),	optional_options.end());

	// Preserve positional declaration order.
	for (const std::unique_ptr<ArgumentBase>& positional : m_positionals)
	{
		std::string token =	format_meta(*positional, positional->GetName());

		if (!positional->IsRequired())
		{
			token = "[" + token + "]";
		}

		usage_parts.push_back(std::move(token));
	}

	if (m_pos_aggregate)
	{
		std::string token =	format_meta(*m_pos_aggregate, m_pos_aggregate->GetName());
		token += "...";

		if (!m_pos_aggregate->IsRequired())
		{
			token = "[" + token + "]";
		}

		usage_parts.push_back(std::move(token));
	}

	std::ostringstream output;

	const std::string executable = m_exe_name.empty() ? "<program>"	: m_exe_name;

	const std::string usage_prefix = "Usage: " + executable;

	output << usage_prefix;

	std::size_t current_column = usage_prefix.size();

	// Align continued lines underneath the text after "Usage:".
	constexpr std::size_t continuation_indent = sizeof("Usage: ") - 1;

	for (const std::string& part : usage_parts)
	{
		if (current_column + 1 + part.size() > max_line_width)
		{
			output << '\n' << std::string(continuation_indent, ' ');
			current_column = continuation_indent;
		}

		output << ' ' << part;
		current_column += 1 + part.size();
	}

	output << "\n\n";

	// Application-level description.
	if (!m_help.empty())
	{
		const std::vector<std::string> description_lines = wrap_text(m_help, max_line_width);

		for (const std::string& line : description_lines)
		{
			output << line << '\n';
		}

		output << '\n';
	}

	std::vector<HelpRow> positional_rows;

	positional_rows.reserve(m_positionals.size() + (m_pos_aggregate ? 1 : 0));

	for (const std::unique_ptr<ArgumentBase>& positional : m_positionals)
	{
		positional_rows.push_back({
			std::string(positional->GetName()),
			make_description(*positional)
			});
	}

	if (m_pos_aggregate)
	{
		positional_rows.push_back({
			std::string(m_pos_aggregate->GetName()) + "...",
			make_description(*m_pos_aggregate)
			});
	}

	if (!positional_rows.empty())
	{
		output << "Positional arguments:\n";
		append_rows(output, positional_rows);
		output << '\n';
	}

	if (!option_rows.empty())
	{
		std::vector<HelpRow> rows;
		rows.reserve(option_rows.size());

		for (OptionRow& option : option_rows)
		{
			rows.push_back(std::move(option.help));
		}

		output << "Options:\n";
		append_rows(output, rows);
	}

	return output.str();
}

const Parser::Diagnostic& Parser::ArgParser::GetDiagnostics() const
{
	return m_error;
}

void Parser::ArgParser::FormatError()
{
	LockAndFailArgs();
	m_formatted_error.clear();

	const auto append_detail = [this]()
		{
			if (!m_error.opt_error_message.empty())
			{
				m_formatted_error += ": ";
				m_formatted_error += m_error.opt_error_message;
				if (m_formatted_error.back() != '.' && m_formatted_error.back() != '?' && m_formatted_error.back() != '!')
					m_formatted_error += '.';
			}
			else
				m_formatted_error += '.';
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
}

void Parser::ArgParser::LockAndFailArgs()
{
	for (auto& arg : m_positionals)
	{
		arg->Lock();
		arg->m_success = false;
	}

	for (auto& arg : m_args)
	{
		arg->Lock();
		arg->m_success = false;
	}

	if (m_pos_aggregate)
	{
		m_pos_aggregate->Lock();
		m_pos_aggregate->m_success = false;
	}
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
	if (name == "--help" || name == "-h")
		return true;
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

std::string_view Parser::ArgumentBase::GetMeta() const
{
	return m_meta;
}

bool Parser::ArgumentBase::IsRequired() const
{
	return m_required;
}

const std::vector<std::string>& Parser::ArgumentBase::GetAliases() const
{
	return m_aliases;
}

void Parser::ArgumentBase::Lock()
{
	m_locked = true;
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
	if (!m_locked || !m_success)
		throw std::logic_error("Value called in an invalid state");

	return m_state;
}

Parser::Flag& Parser::Flag::Help(std::string_view help_msg)
{
	if (m_locked)
		throw std::logic_error("Argument cannot be configured after ParseAndValidate");

	m_help = help_msg;
	return *this;
}

void Parser::Flag::Finalize(Diagnostic& d)
{
	if (m_locked)
		return;

	m_locked = true;
	m_success = true;
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

	Result r = Parser::ParseInteger(v, value);
	if (!r)
		return r;
	if (value < INT8_MIN ||
		value > INT8_MAX)
	{
		return Parser::Result::Failure("integer result out of range");
	}

	out = static_cast<int8_t>(value);
	return Parser::Result::Success();
}

Parser::Result Parser::Parse(std::string_view v, uint8_t& out)
{
	uint16_t value{};

	Result r = Parser::ParseInteger(v, value);
	if (!r)
		return r;
	if (value > UINT8_MAX)
		return Parser::Result::Failure("integer result out of range");

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