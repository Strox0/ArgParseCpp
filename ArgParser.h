#pragma once
#include <type_traits>
#include <memory>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>
#include <vector>
#include <string>
#include <functional>
#include <concepts>
#include <utility>
#include <unordered_map>
#include <map>
#include <stdexcept>
#include <limits>
#include <span>
#include <initializer_list>

namespace Parser
{
	enum class Error
	{
		SUCCESS,
		PARSE_FAIL,
		MISSING_REQUIRED,
		TEXT_VALIDATION_INVALID,
		VAL_VALIDATION_INVALID,
		COL_VALIDATION_INVALID,
		TRANSFORMATION_ERROR,
		MISSING_VALUE,
		UNKNOWN_ARGUMENT,
		CARDINALITY_VALIDATION_FAIL,
		FLAG_HAS_EQUALS_VALUE,
		STOP_TOKEN_HAS_EQUALS_VALUE
	};

	enum class Cardinality
	{
		UNLIMITED,
		BETWEEN,
		EXACTLY,
		ATLEAST,
		ATMOST,
	};

	enum class ArgParseResult
	{
		SUCCESS,
		ERROR,
		HELP_REQUESTED,
		STOP_TOKEN
	};

	struct Diagnostic
	{
		Error ec = Error::SUCCESS;
		std::string arg_name;
		std::string opt_token;
		std::string opt_error_message;
		bool positional = false;
		struct Aggregate
		{
			size_t count = 0;
			size_t min = 0;
			size_t max = 0;
			size_t received_count = 0;
			Cardinality card = Cardinality::UNLIMITED;
		}aggregate;

		explicit operator bool() const noexcept
		{
			return ec != Error::SUCCESS;
		}
	};

	struct Result
	{
		bool success;
		std::string error_message;

		explicit operator bool() const noexcept
		{
			return success;
		}

		static Result Success()
		{
			return { true, {} };
		}

		static Result Failure(std::string message = "")
		{
			return { false, std::move(message) };
		}

		Result(bool b) : success(b) {};
		Result(bool b, std::string s) : success(b),error_message(s) {};
	};

	struct StopResultView
	{
		std::string_view stop_token;
		std::span<const std::string> tokens;
		std::string_view carry_name;
	};

	struct CardQueryRes
	{
		size_t exact_count = 0;
		size_t min_count = 0;
		size_t max_count = 0;
		Cardinality card = Cardinality::UNLIMITED;
		CardQueryRes(size_t exact, size_t min, size_t max, Cardinality c) :
			exact_count(exact), min_count(min), max_count(max), card(c)
		{};
	};

	Result Parse(std::string_view v, int64_t& out);
	Result Parse(std::string_view v, uint64_t& out);
	Result Parse(std::string_view v, int32_t& out);
	Result Parse(std::string_view v, uint32_t& out);
	Result Parse(std::string_view v, int16_t& out);
	Result Parse(std::string_view v, uint16_t& out);
	Result Parse(std::string_view v, int8_t& out);
	Result Parse(std::string_view v, uint8_t& out);
	Result Parse(std::string_view v, double& out);
	Result Parse(std::string_view v, long double& out);
	Result Parse(std::string_view v, float& out);
	Result Parse(std::string_view v, std::string& out);
	
	std::string StringDefault(const int64_t in);
	std::string StringDefault(const uint64_t in);
	std::string StringDefault(const int32_t in);
	std::string StringDefault(const uint32_t in);
	std::string StringDefault(const int16_t in);
	std::string StringDefault(const uint16_t in);
	std::string StringDefault(const int8_t in);
	std::string StringDefault(const uint8_t in);
	std::string StringDefault(const double in);
	std::string StringDefault(const long double in);
	std::string StringDefault(const float in);
	std::string StringDefault(const std::string& in);

	template <typename T>
	concept Parseable = requires(std::string_view v, T& out)
	{
		{Parse(v, out)} -> std::same_as<Result>;
	} 
	&& std::is_default_constructible_v<T>
	&& std::is_copy_assignable_v<T>
	&& std::is_copy_constructible_v<T>
	&& !std::is_same_v<T,bool>;

	template <typename T>
	concept HasStringDefaultRef = requires(const T & in)
	{
		{ StringDefault(in) } -> std::convertible_to<std::string>;
	};

	template <typename T>
	concept HasStringDefaultVal = requires (T in)
	{
		{ StringDefault(in) } -> std::convertible_to<std::string>;
	};

	class ArgParser;

	class ArgumentBase
	{
		friend ArgParser;
	public:
		virtual ~ArgumentBase() = default;

	private:
		Error GetError() const;
		std::string_view GetHelp() const;
		void AddName(std::string_view name);
		void AddAliases(const std::vector<std::string>& aliases);
		std::string_view GetName() const;
		std::string_view GetMeta() const;
		std::string_view GetStringDefault() const;
		CardQueryRes GetCardinality() const;
		bool IsRequired() const;
		const std::vector<std::string>& GetAliases() const;
		void Lock();
		bool HasDefaultString() const;

	protected:
		virtual void Finalize(Diagnostic& diagnostic) = 0;
		virtual void AppendValue(std::string_view value) {};
		virtual void AddValue(std::string_view value);

		Error m_error = Error::SUCCESS;
		std::string m_help;
		bool m_required = false;
		bool m_set = false;
		bool m_has_default = false;
		bool m_locked = false;
		bool m_success = false;
		bool m_has_default_str = false;
		std::string m_value;
		std::string m_name;
		std::string m_meta;
		std::string m_default_str;
		std::vector<std::string> m_aliases;

		size_t m_exact_count = 0;
		size_t m_min_count = 0;
		size_t m_max_count = 0;
		Cardinality m_card = Cardinality::UNLIMITED;
	};

	template<Parseable T>
	class Argument : public ArgumentBase
	{
	public:
		Argument(std::string_view parsed_value);
		Argument();
		
		bool Provided() const;
		T Value() const;
		const T& ValueRef() const;
		T ValueOr(const T& backup_val) const;

		template <class Validation>
		requires std::invocable<Validation&, std::string_view> &&
		std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, Result>
		Argument<T>& ValidateText(Validation&& val);

		template <class Validation>
		requires std::invocable<Validation&, const T&>&&
		std::convertible_to<std::invoke_result_t<Validation&, const T&>, Result>
		Argument<T>& ValidateValue(Validation&& val);

		template<class Transformation>
		requires std::invocable<Transformation&, std::string&>&&
		std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, Result>
		Argument<T>& Transform(Transformation&& trans);

		Argument<T>& Required();
		Argument<T>& Default(const T& default_value);
		Argument<T>& Help(std::string_view help_msg);
		Argument<T>& Meta(std::string_view meta_var_name);

	protected:
		void Finalize(Diagnostic& d) override;

	private:
		using TextValidator = std::function<Result(std::string_view)>;
		using ValueValidator = std::function<Result(const T&)>;
		using Transformer = std::function<Result(std::string&)>;

		std::vector<TextValidator> m_text_vals;
		std::vector<ValueValidator> m_val_vals;
		std::vector<Transformer> m_text_trans;

		T m_parsed_val{};
		T m_default_val{};
	};

	class Flag : public ArgumentBase
	{
	public:
		Flag() = default;
		Flag(bool b) : m_state(b) {};
		bool Value() const;
		Flag& Help(std::string_view help_msg);

	protected:
		void Finalize(Diagnostic& d) override;
		void AddValue(std::string_view val) override {};
		
	private:
		friend ArgParser;
		void SetTrue();
		bool m_state = false;
	};

	template <Parseable T>
	class Aggregate : public ArgumentBase
	{
	public:
		friend ArgParser;

		Aggregate();

		bool Provided() const;
		std::vector<T> Value() const;
		const std::vector<T>& ValueRef() const;
		std::vector<T> ValueOr(const std::vector<T>& backup_val) const;

		template <class Validation>
		requires std::invocable<Validation&, std::string_view>&&
		std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, Result>
		Aggregate<T>& ValidateText(Validation&& val);

		template <class Validation>
		requires std::invocable<Validation&, const T&>&&
		std::convertible_to<std::invoke_result_t<Validation&, const T&>, Result>
		Aggregate<T>& ValidateValue(Validation&& val);

		template<class Transformation>
		requires std::invocable<Transformation&, std::string&>&&
		std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, Result>
		Aggregate<T>& Transform(Transformation&& trans);

		template<class Validation>
		requires std::invocable<Validation&, const std::vector<T>&>&&
		std::convertible_to<std::invoke_result_t<Validation&, const std::vector<T>&>, Result>
		Aggregate<T>& ValidateCollection(Validation&& val);

		Aggregate<T>& Required();
		Aggregate<T>& Default(const std::vector<T>& default_value);
		Aggregate<T>& Help(std::string_view help_msg);
		Aggregate<T>& Meta(std::string_view meta_var_name);

		Aggregate<T>& Exactly(size_t count);
		Aggregate<T>& Between(size_t min, size_t max);
		Aggregate<T>& AtLeast(size_t count);
		Aggregate<T>& AtMost(size_t count);
		Aggregate<T>& Unlimited();

	protected:
		void Finalize(Diagnostic& d) override;
		void AppendValue(std::string_view val) override;

	private:
		Error ApplyCardinality(Diagnostic& d);

		using TextValidator = std::function<Result(std::string_view)>;
		using ValueValidator = std::function<Result(const T&)>;
		using Transformer = std::function<Result(std::string&)>;
		using CollectionValidator = std::function<Result(const std::vector<T>&)>;

		std::vector<TextValidator> m_text_vals;
		std::vector<ValueValidator> m_val_vals;
		std::vector<Transformer> m_text_trans;

		std::vector<CollectionValidator> m_col_vals;

		std::vector<T> m_parsed_vals;
		std::vector<T> m_default_vals;

		std::vector<std::string> m_values;

		bool m_card_set = false;
	};

	class ArgParser
	{
	public:
		ArgParser(int argc, char** argv);
		ArgParser(const StopResultView& stop_result);
		ArgParser() = delete;

		template<typename T>
		Argument<T>& Add(const std::string& name, const std::vector<std::string>& aliases = {});

		template<typename T>
		Argument<T>& AddPositional(const std::string& helper_name);

		template<typename T>
		Aggregate<T>& AddAggregate(const std::string& name, const std::vector<std::string>& aliases = {});

		Flag& AddFlag(const std::string& name, const std::vector<std::string>& aliases = {});

		template <typename T>
		Aggregate<T>& AddPositionalAggregate(const std::string& helper_name);

		void Help(std::string_view help_msg);
		void StopAt(const std::string& token);
		void StopAt(std::initializer_list<std::string> tokens);

		ArgParseResult ParseAndValidate();
		const Diagnostic& GetDiagnostics() const;
		std::string GetErrorMessage() const;
		std::string GetHelpMessage(size_t max_line_width = 80, size_t max_label_width = 32) const;
		const StopResultView& GetStopResult() const;

	private:
		void FormatError();
		void LockAndFailArgs();
		void CheckAndRegisterNames(const std::string& name, const std::vector<std::string>& al);
		bool IsKnownName(const std::string& name);
		bool ResolveName(std::string& name);
		std::pair<std::string,std::string> ParseTokenWithEquals(const std::string& token, bool& has_value);

	private:

		enum class ArgType
		{
			Scalar,
			Aggregate,
			Flag,
			Positional,
			AggregatePositional,
			StopToken
		};

		std::vector<std::shared_ptr<ArgumentBase>> m_args;
		std::vector<std::unique_ptr<ArgumentBase>> m_positionals;
		std::unique_ptr<ArgumentBase> m_pos_aggregate;
		std::vector<std::string> m_tokens;
		StopResultView m_stop_result;
		Diagnostic m_error;
		std::string m_formatted_error;
		std::string m_help;
		std::string m_exe_name;
		std::map<std::string, std::pair<ArgType,std::shared_ptr<ArgumentBase>>> m_name_arg_map;
		std::unordered_map<std::string, std::string> m_alias_map;
		bool m_locked = false;
	};

	template<typename T>
	inline Argument<T>& ArgParser::Add(const std::string& name, const std::vector<std::string>& aliases)
	{
		if (m_locked)
			throw std::logic_error("No configuration functions can be called after ParseAndValidate");

		CheckAndRegisterNames(name, aliases);
		m_args.emplace_back(std::make_shared<Argument<T>>());
		m_name_arg_map[name] = std::make_pair(ArgType::Scalar, m_args.back());
		m_args.back()->AddName(name);
		m_args.back()->AddAliases(aliases);
		return *(Argument<T>*)m_args.back().get();
	}

	template<typename T>
	inline Argument<T>& ArgParser::AddPositional(const std::string& helper_name)
	{
		if (m_locked)
			throw std::logic_error("No configuration functions can be called after ParseAndValidate");

		if (helper_name.empty())
			throw std::logic_error("Helper name cannot be empty");

		m_positionals.emplace_back(std::make_unique<Argument<T>>());
		m_positionals.back()->AddName(helper_name);
		return *(Argument<T>*)m_positionals.back().get();
	}

	template<typename T>
	inline Aggregate<T>& ArgParser::AddAggregate(const std::string& name, const std::vector<std::string>& aliases)
	{
		if (m_locked)
			throw std::logic_error("No configuration functions can be called after ParseAndValidate");

		CheckAndRegisterNames(name, aliases);
		m_args.emplace_back(std::make_shared<Aggregate<T>>());
		m_name_arg_map[name] = std::make_pair(ArgType::Aggregate, m_args.back());
		m_args.back()->AddName(name);
		m_args.back()->AddAliases(aliases);
		return *(Aggregate<T>*)m_args.back().get();
	}

	template<typename T>
	inline Aggregate<T>& ArgParser::AddPositionalAggregate(const std::string& helper_name)
	{
		if (m_locked)
			throw std::logic_error("No configuration functions can be called after ParseAndValidate");

		if (helper_name.empty())
			throw std::logic_error("Helper name cannot be empty");

		if (m_pos_aggregate)
			throw std::logic_error("There cannot be two positional aggregates");

		m_pos_aggregate = std::make_unique<Aggregate<T>>();
		m_pos_aggregate->AddName(helper_name);
		return *(Aggregate<T>*)m_pos_aggregate.get();
	}

	template<Parseable T>
	inline Argument<T>::Argument(std::string_view parsed_value)
	{
		if (parsed_value.empty())
			return;

		m_value = parsed_value;
		m_set = true;
	}

	template<Parseable T>
	inline Argument<T>::Argument()
	{
		m_set = false;
	}

	template<Parseable T>
	inline bool Argument<T>::Provided() const
	{
		if (!m_locked)
			throw std::logic_error("Provided cannot be called before ParseAndValidate");

		return m_set;
	}

	template<Parseable T>
	inline T Argument<T>::Value() const
	{
		if (!m_locked || !m_success)
			throw std::logic_error("Value called in an invalid state");

		return m_parsed_val;
	}

	template<Parseable T>
	inline const T& Argument<T>::ValueRef() const
	{
		if (!m_locked || !m_success)
			throw std::logic_error("ValueRef called in an invalid state");

		return m_parsed_val;
	}

	template<Parseable T>
	inline T Argument<T>::ValueOr(const T& backup_val) const
	{
		if (!m_locked)
			throw std::logic_error("ValueOr cannot be called before ParseAndValidate");

		if (m_set && m_success)
			return m_parsed_val;
		else if (m_has_default && m_success)
			return m_default_val;
		else
			return backup_val;
	}

	template<Parseable T>
	inline Argument<T>& Argument<T>::Required()
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		if (m_has_default)
		{
			throw std::logic_error("An argument cannot be set as Required while having a default value");
		}
		else
		{
			m_required = true;
		}

		return *this;
	}

	template<Parseable T>
	inline Argument<T>& Argument<T>::Default(const T& default_value)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		if (m_required)
		{
			throw std::logic_error("An argument cannot have a default value while marked as Required");
		}
		else if (m_has_default)
		{
			throw std::logic_error("Default value already set");
		}
		else
		{
			m_default_val = default_value;
			m_has_default = true;

			if constexpr (HasStringDefaultRef<T> || HasStringDefaultVal<T>)
			{
				m_default_str = StringDefault(default_value);
				m_has_default_str = true;
			}
		}

		return *this;
	}
	
	template<Parseable T>
	template<class Validation>
	requires std::invocable<Validation&, std::string_view>&&
	std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, Result>
	inline Argument<T>& Parser::Argument<T>::ValidateText(Validation&& val)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_text_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

	template<Parseable T>
	template<class Validation>
	requires std::invocable<Validation&, const T&>&&
	std::convertible_to<std::invoke_result_t<Validation&, const T&>, Result>
	inline Argument<T>& Parser::Argument<T>::ValidateValue(Validation&& val)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_val_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

	template<Parseable T>
	template<class Transformation>
	requires std::invocable<Transformation&, std::string&>&&
	std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, Result>
	inline Argument<T>& Parser::Argument<T>::Transform(Transformation&& trans)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_text_trans.emplace_back(std::forward<Transformation>(trans));
		return *this;
	}

	template<Parseable T>
	inline Argument<T>& Argument<T>::Help(std::string_view help_msg)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_help = help_msg;
		return *this;
	}

	template<Parseable T>
	inline Argument<T>& Argument<T>::Meta(std::string_view meta_var_name)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_meta = meta_var_name;
		return *this;
	}

	template<Parseable T>
	inline void Argument<T>::Finalize(Diagnostic& d)
	{
		if (m_locked)
			return;

		m_locked = true;

		if (m_set)
		{
			for (auto& t : m_text_trans)
			{
				std::string tmp = m_value;
				Result r = std::invoke(t, tmp);
				if (!r)
				{
					m_error = Error::TRANSFORMATION_ERROR;
					d.opt_error_message = r.error_message;
					d.opt_token = m_value;
					d.arg_name = m_name;
					d.ec = m_error;
					return;
				}
				m_value = tmp;
			}

			for (auto& v : m_text_vals)
			{
				Result r = std::invoke(v, m_value);
				if (!r)
				{
					m_error = Error::TEXT_VALIDATION_INVALID;
					d.opt_error_message = r.error_message;
					d.opt_token = m_value;
					d.arg_name = m_name;
					d.ec = m_error;
					return;
				}
			}

			Result r = Parse(m_value, m_parsed_val);
			if (!r)
			{
				m_error = Error::PARSE_FAIL;
				d.opt_error_message = r.error_message;
				d.opt_token = m_value;
				d.arg_name = m_name;
				d.ec = m_error;
				return;
			}
		}
		else if (m_has_default)
			m_parsed_val = m_default_val;
		else if (m_required)
		{
			m_error = Error::MISSING_REQUIRED;
			d.arg_name = m_name;
			d.ec = m_error;
			return;
		}
		else
			return;

		for (auto& v : m_val_vals)
		{
			Result r = std::invoke(v, m_parsed_val);
			if (!r)
			{
				m_error = Error::VAL_VALIDATION_INVALID;
				d.opt_error_message = r.error_message;
				d.opt_token = m_value;
				d.arg_name = m_name;
				d.ec = m_error;
				return;
			}
		}
		m_success = true;
	}

	template <typename T>
	Parser::Result ParseInteger(std::string_view v, T& out)
	{
		static_assert(std::is_integral_v<T>);

		if (v.empty())
			return Parser::Result::Failure("value is empty");

		T value{};

		const char* begin = v.data();
		const char* end = begin + v.size();

		const auto [ptr, ec] = std::from_chars(begin, end, value, 10);

		if (ec == std::errc::invalid_argument || ptr != end)
			return Parser::Result::Failure("expected an integer");
		else if (ec == std::errc::result_out_of_range)
			return Parser::Result::Failure("integer result out of range");

		out = value;
		return Parser::Result::Success();
	}

	template <typename T>
	Parser::Result ParseFloatingPoint(std::string_view v, T& out)
	{
		static_assert(std::is_floating_point_v<T>);

		if (v.empty())
			return Parser::Result::Failure("value is empty");

		T value{};

		const char* begin = v.data();
		const char* end = begin + v.size();

		const auto [ptr, ec] = std::from_chars(begin, end, value, std::chars_format::general);

		if (ec == std::errc::invalid_argument || ptr != end)
			return Parser::Result::Failure("expected a floating point value");
		else if (ec == std::errc::result_out_of_range)
			return Parser::Result::Failure("floating point result out of range");

		out = value;
		return Parser::Result::Success();
	}

	template <typename T>
	std::string IntegerToString(T value)
	{
		static_assert(std::is_integral_v<T>);

		std::string buffer(std::numeric_limits<T>::digits10 + 3, 0);

		const auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 10);

		if (ec != std::errc{})
			return std::string();

		buffer.resize(ptr - buffer.data());
		return buffer;
	}

	template <typename T>
	std::string FloatingPointToString(T value)
	{
		static_assert(std::is_floating_point_v<T>);

		std::string buffer(std::numeric_limits<T>::max_digits10 + 16,0);
		
		const auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);

		if (ec != std::errc{})
			return std::string();

		buffer.resize(ptr - buffer.data());
		return buffer;
	}

	template <typename T>
	std::string VectorToString(const std::vector<T>& in)
	{
		std::string output = "[";

		for (size_t i = 0; i < in.size(); ++i)
		{
			output += ' ';
			output += StringDefault(in[i]);

			if (i + 1 < in.size())
				output += ',';
			else
				output += ' ';
		}

		output += ']';

		return output;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Default(const std::vector<T>& default_value)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		if (m_required)
		{
			throw std::logic_error("An argument cannot have a default value while marked as Required");
		}
		else if (m_has_default)
		{
			throw std::logic_error("Default value already set");
		}
		else
		{
			m_default_vals = default_value;
			m_has_default = true;

			if constexpr (HasStringDefaultRef<T> || HasStringDefaultVal<T>)
			{
				m_default_str = VectorToString(default_value);
				m_has_default_str = true;
			}
		}

		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Unlimited()
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		if (m_card_set)
			throw std::logic_error("Cardinality already configured");
		m_card_set = true;
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>::Aggregate()
	{
		m_set = false;
	}

	template<Parseable T>
	inline bool Aggregate<T>::Provided() const
	{
		if (!m_locked)
			throw std::logic_error("Provided cannot be called before ParseAndValidate");

		return m_set;
	}

	template<Parseable T>
	inline std::vector<T> Aggregate<T>::Value() const
	{
		if (!m_locked || !m_success)
			throw std::logic_error("Value called in an invalid state");

		return m_parsed_vals;
	}

	template<Parseable T>
	inline const std::vector<T>& Aggregate<T>::ValueRef() const
	{
		if (!m_locked || !m_success)
			throw std::logic_error("ValueRef called in an invalid state");

		return m_parsed_vals;
	}

	template<Parseable T>
	inline std::vector<T> Aggregate<T>::ValueOr(const std::vector<T>& backup_val) const
	{
		if (!m_locked)
			throw std::logic_error("ValueOr cannot be called before ParseAndValidate");

		if (m_set && m_success)
			return m_parsed_vals;
		else if (m_has_default && m_success)
			return m_default_vals;
		else
			return backup_val;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Required()
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		if (m_has_default)
			throw std::logic_error("An argument cannot be set as Required while having a default value");
		else
		{
			m_required = true;
		}

		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Help(std::string_view help_msg)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_help = help_msg;
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Meta(std::string_view meta_var_name)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_meta = meta_var_name;
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Exactly(size_t count)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");
		if (m_card_set)
			throw std::logic_error("Cardinality already configured");
		else if (count == 0)
			throw std::logic_error("Exact count cannot be 0");
		else
		{
			m_card = Cardinality::EXACTLY;
			m_exact_count = count;
			m_card_set = true;
		}
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Between(size_t min, size_t max)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");
		if (m_card_set)
			throw std::logic_error("Cardinality already configured");
		else if (min == 0 || max == 0)
			throw std::logic_error("Count cannot be 0");
		else if (max < min)
			throw std::logic_error("Minimum count cannot be greater than maximum count");
		else
		{
			m_card = Cardinality::BETWEEN;
			m_min_count = min;
			m_max_count = max;
			m_card_set = true;
		}
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::AtLeast(size_t count)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");
		if (m_card_set)
			throw std::logic_error("Cardinality already configured");
		else if (count == 0)
			throw std::logic_error("Count cannot be 0");
		else
		{
			m_card = Cardinality::ATLEAST;
			m_min_count = count;
			m_card_set = true;
		}
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::AtMost(size_t count)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");
		if (m_card_set)
			throw std::logic_error("Cardinality already configured");
		else if (count == 0)
			throw std::logic_error("Count cannot be 0");
		else
		{
			m_card = Cardinality::ATMOST;
			m_max_count = count;
			m_card_set = true;
		}
		return *this;
	}

	template<Parseable T>
	inline void Aggregate<T>::Finalize(Diagnostic& d)
	{
		if (m_locked)
			return;

		m_locked = true;

		if (m_set)
		{
			for (auto& val : m_values)
			{
				for (auto& t : m_text_trans)
				{
					std::string tmp = val;
					Result r = std::invoke(t, tmp);
					if (!r)
					{
						m_error = Error::TRANSFORMATION_ERROR;
						d.opt_error_message = r.error_message;
						d.opt_token = val;
						d.arg_name = m_name;
						d.ec = m_error;
						return;
					}
					val = tmp;
				}

				for (auto& v : m_text_vals)
				{
					Result r = std::invoke(v, val);
					if (!r)
					{
						m_error = Error::TEXT_VALIDATION_INVALID;
						d.opt_error_message = r.error_message;
						d.opt_token = val;
						d.arg_name = m_name;
						d.ec = m_error;
						return;
					}
				}

				T tmp{};
				Result r = Parse(val, tmp);
				if (!r)
				{
					m_error = Error::PARSE_FAIL;
					d.opt_error_message = r.error_message;
					d.opt_token = val;
					d.arg_name = m_name;
					d.ec = m_error;
					return;
				}

				m_parsed_vals.emplace_back(tmp);
			}
		}
		else if (m_has_default)
			m_parsed_vals = m_default_vals;
		else if (m_required)
		{
			m_error = Error::MISSING_REQUIRED;
			d.arg_name = m_name;
			d.ec = m_error;
			return;
		}
		else
		{
			m_success = true;
			return;
		}

		m_error = ApplyCardinality(d);
		if (m_error != Error::SUCCESS)
		{
			d.arg_name = m_name;
			d.ec = m_error;
			return;
		}

		for (auto& v : m_val_vals)
		{
			for (size_t index = 0; index < m_parsed_vals.size(); index++)
			{
				Result r = std::invoke(v, m_parsed_vals[index]);
				if (!r)
				{
					m_error = Error::VAL_VALIDATION_INVALID;
					d.opt_error_message = r.error_message;
					if (index < m_values.size())
						d.opt_token = m_values[index];
					d.arg_name = m_name;
					d.ec = m_error;
					return;
				}
			}
		}

		for (auto& v : m_col_vals)
		{
			Result r = std::invoke(v, m_parsed_vals);
			if (!r)
			{
				m_error = Error::COL_VALIDATION_INVALID;
				d.opt_error_message = r.error_message;
				d.arg_name = m_name;
				d.ec = m_error;
				return;
			}
		}
		m_success = true;
	}

	template<Parseable T>
	inline void Aggregate<T>::AppendValue(std::string_view val)
	{
		if (m_locked)
			return;

		m_values.emplace_back(val);
		if (!m_set)
		{
			m_set = true;
			if (m_error == Error::MISSING_REQUIRED && m_required)
				m_error = Error::SUCCESS;
		}
	}

	template<Parseable T>
	inline Error Aggregate<T>::ApplyCardinality(Diagnostic& d)
	{
		switch (m_card)
		{
		case Parser::Cardinality::BETWEEN:
			if (m_parsed_vals.size() < m_min_count || m_parsed_vals.size() > m_max_count)
			{
				d.aggregate.card = m_card;
				d.aggregate.min = m_min_count;
				d.aggregate.max = m_max_count;
				d.aggregate.received_count = m_parsed_vals.size();
				return Error::CARDINALITY_VALIDATION_FAIL;
			}
			break;
		case Parser::Cardinality::EXACTLY:
			if (m_parsed_vals.size() != m_exact_count)
			{
				d.aggregate.card = m_card;
				d.aggregate.count = m_exact_count;
				d.aggregate.received_count = m_parsed_vals.size();
				return Error::CARDINALITY_VALIDATION_FAIL;
			}
			break;
		case Parser::Cardinality::ATLEAST:
			if (m_parsed_vals.size() < m_min_count)
			{
				d.aggregate.card = m_card;
				d.aggregate.min = m_min_count;
				d.aggregate.received_count = m_parsed_vals.size();
				return Error::CARDINALITY_VALIDATION_FAIL;
			}
			break;
		case Parser::Cardinality::ATMOST:
			if (m_parsed_vals.size() > m_max_count)
			{
				d.aggregate.card = m_card;
				d.aggregate.max = m_max_count;
				d.aggregate.received_count = m_parsed_vals.size();
				return Error::CARDINALITY_VALIDATION_FAIL;
			}
			break;
		case Parser::Cardinality::UNLIMITED:
			break;
		}

		return Error::SUCCESS;
	}

	template<Parseable T>
	template <class Validation>
	requires std::invocable<Validation&, std::string_view>&&
	std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, Result>
	inline Aggregate<T>& Aggregate<T>::ValidateText(Validation&& val)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_text_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

	template<Parseable T>
	template <class Validation>
	requires std::invocable<Validation&, const T&>&&
	std::convertible_to<std::invoke_result_t<Validation&, const T&>, Result>
	inline Aggregate<T>& Aggregate<T>::ValidateValue(Validation&& val)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_val_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

	template<Parseable T>
	template<class Transformation>
	requires std::invocable<Transformation&, std::string&>&&
	std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, Result>
	inline Aggregate<T>& Aggregate<T>::Transform(Transformation&& trans)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_text_trans.emplace_back(std::forward<Transformation>(trans));
		return *this;
	}

	template<Parseable T>		
	template<class Validation>
	requires std::invocable<Validation&, const std::vector<T>&>&&
	std::convertible_to<std::invoke_result_t<Validation&, const std::vector<T>&>, Result>
	inline Aggregate<T>& Aggregate<T>::ValidateCollection(Validation&& val)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_col_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

}