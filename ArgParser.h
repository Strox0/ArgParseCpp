#pragma once
#include <type_traits>
#include <memory>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <functional>
#include <vector>

namespace Parser
{
	enum class Error
	{
		SUCCESS,
		PARSE_FAIL,
		MISSING_REQUIRED,
		REQUIRED_HAS_DEFAULT,
		TEXT_VALIDATION_INVALID,
		VAL_VALIDATION_INVALID,
		COL_VALIDATION_INVALID,
		TRANSFORMATION_ERROR,
		TOO_FEW_ARGS,
		FLAG_AS_ARG,
		MISSING_VALUE,
		UNKNOWN_VALUE,
		DEFAULT_ALREADY_SET,
		REQ_POS_AFTER_OPTIONAL,
		MIN_COUNT_NOT_SET,
		MAX_COUNT_NOT_SET,
		MIN_GREATER_MAX,
		CARDINALITY_INCOMPATIBLE,
		NO_CARDINALITY_SET,
		HELP_QUERY,
		NAME_ALREADY_USED,
		CARDINALITY_ERROR,
	};

	bool Parse(std::string_view v, int64_t& out);
	bool Parse(std::string_view v, uint64_t& out);
	bool Parse(std::string_view v, int32_t& out);
	bool Parse(std::string_view v, uint32_t& out);
	bool Parse(std::string_view v, int16_t& out);
	bool Parse(std::string_view v, uint16_t& out);
	bool Parse(std::string_view v, int8_t& out);
	bool Parse(std::string_view v, uint8_t& out);
	bool Parse(std::string_view v, double& out);
	bool Parse(std::string_view v, long double& out);
	bool Parse(std::string_view v, float& out);
	bool Parse(std::string_view v, std::string& out);
	
	template <typename T>
	concept Parseable = requires(std::string_view v, T& out)
	{
		Parse(v, out);
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
		const std::vector<std::string>& GetAliases() const;

	protected:
		virtual void Finalize() = 0;
		virtual void AppendValue(std::string_view value) {};
		void AddValue(std::string_view value);

		Error m_error = Error::SUCCESS;
		std::string m_help;
		bool m_required = false;
		bool m_set = false;
		bool m_has_default = false;
		bool m_locked = false;
		std::string m_value;
		std::string m_name;
		std::vector<std::string> m_aliases;
	};

	template<Parseable T>
	class Argument : public ArgumentBase
	{
	public:
		Argument(std::string_view parsed_value);
		Argument();
		
		bool Provided() const;
		T Value() const;

		template <class Validation>
		requires std::invocable<Validation&, std::string_view> &&
		std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, bool>
		Argument<T>& ValidateText(Validation&& val);

		template <class Validation>
		requires std::invocable<Validation&, const T&>&&
		std::convertible_to<std::invoke_result_t<Validation&, const T&>, bool>
		Argument<T>& ValidateValue(Validation&& val);

		template<class Transformation>
		requires std::invocable<Transformation&, std::string&>&&
		std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, bool>
		Argument<T>& Transform(Transformation&& trans);

		Argument<T>& Required();
		Argument<T>& Default(const T& default_value);
		Argument<T>& Help(std::string_view help_msg);

	protected:
		void Finalize() override;

	private:
		using TextValidator = std::function<bool(std::string_view)>;
		using ValueValidator = std::function<bool(const T&)>;
		using Transformer = std::function<bool(std::string&)>;

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
		void Finalize() override;

	private:
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

		template <class Validation>
		requires std::invocable<Validation&, std::string_view>&&
		std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, bool>
		Aggregate<T>& ValidateText(Validation&& val);

		template <class Validation>
		requires std::invocable<Validation&, const T&>&&
		std::convertible_to<std::invoke_result_t<Validation&, const T&>, bool>
		Aggregate<T>& ValidateValue(Validation&& val);

		template<class Transformation>
		requires std::invocable<Transformation&, std::string&>&&
		std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, bool>
		Aggregate<T>& Transform(Transformation&& trans);

		template<class Validation>
		requires std::invocable<Validation&, const std::vector<T>&>&&
		std::convertible_to<std::invoke_result_t<Validation&, const std::vector<T>&>, bool>
		Aggregate<T>& ValidateCollection(Validation&& val);

		Aggregate<T>& Required();
		Aggregate<T>& Default(const std::vector<T>& default_value);
		Aggregate<T>& Count(size_t count);
		Aggregate<T>& MinCount(size_t count);
		Aggregate<T>& MaxCount(size_t count);
		Aggregate<T>& Unlimited();
		Aggregate<T>& Help(std::string_view help_msg);

	protected:
		void Finalize() override;
		void AppendValue(std::string_view val) override;

	private:
		Error ApplyCardinality();
		Error ValidateCardinality();

		using TextValidator = std::function<bool(std::string_view)>;
		using ValueValidator = std::function<bool(const T&)>;
		using Transformer = std::function<bool(std::string&)>;
		using CollectionValidator = std::function<bool(const std::vector<T>&)>;

		std::vector<TextValidator> m_text_vals;
		std::vector<ValueValidator> m_val_vals;
		std::vector<Transformer> m_text_trans;

		std::vector<CollectionValidator> m_col_vals;

		std::vector<T> m_parsed_vals;
		std::vector<T> m_default_vals;

		std::vector<std::string> m_values;

		size_t m_exact_count = 0;
		size_t m_min_count = 0;
		size_t m_max_count = 0;
		bool m_unlimited = false;
	};

	class ArgParser
	{
	public:
		ArgParser(int argc, char** argv);
		ArgParser() = delete;

		template<typename T>
		Argument<T>& Add(std::string_view name, const std::vector<std::string>& aliases = {});

		template<typename T>
		Argument<T>& AddPositional(std::string_view helper_name);

		template<typename T>
		Aggregate<T>& AddAggregate(std::string_view name, const std::vector<std::string>& aliases = {});

		Flag& AddFlag(std::string_view name, const std::vector<std::string>& aliases = {});

		void Help(std::string_view help_msg);

		Error ValidateArgs(bool exit = true);

	private:
		Error HandleError(bool exit);

	private:
		std::vector<std::unique_ptr<ArgumentBase>> m_args;
		std::vector<std::unique_ptr<ArgumentBase>> m_positionals;
		std::vector<std::unique_ptr<ArgumentBase>> m_aggregates;
		std::vector<std::string> m_tokens;
		Error m_error;
		std::string m_help;
		std::unordered_set<std::string> m_names;
	};

	template<typename T>
	inline Argument<T>& ArgParser::Add(std::string_view name, const std::vector<std::string>& aliases)
	{
		std::vector<std::string> names;
		names.push_back(std::string(name));
		names.insert(names.end(), aliases.begin(), aliases.end());

		for (const auto& n : names)
		{
			if (m_names.contains(std::string(n)))
			{
				m_error = Error::NAME_ALREADY_USED;
				m_args.emplace_back(std::make_unique<Argument<T>>());
				return *(Argument<T>*)m_args.back().get();
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
			m_args.emplace_back(std::make_unique<Argument<T>>());
		}
		else
		{
			if (typeid(bool) == typeid(T))
			{
				m_error = Error::FLAG_AS_ARG;
				m_args.emplace_back(std::make_unique<Argument<T>>());
			}
			else
			{
				if (it + 1 != m_tokens.end())
				{
					m_args.emplace_back(std::make_unique<Argument<T>>(*(it + 1)));
					m_tokens.erase(it, it + 2);
				}
				else
				{
					m_args.emplace_back(std::make_unique<Argument<T>>());
					m_error = Error::MISSING_VALUE;
					m_tokens.erase(it);
				}
			}
		}

		m_args.back()->AddName(name);
		m_args.back()->AddAliases(aliases);
		return *(Argument<T>*)m_args.back().get();
	}

	template<typename T>
	inline Argument<T>& ArgParser::AddPositional(std::string_view helper_name)
	{
		m_positionals.emplace_back(std::make_unique<Argument<T>>());
		m_positionals.back()->AddName(helper_name);
		return *(Argument<T>*)m_positionals.back().get();
	}

	template<typename T>
	inline Aggregate<T>& ArgParser::AddAggregate(std::string_view name, const std::vector<std::string>& aliases)
	{
		std::vector<std::string> names;
		names.push_back(std::string(name));
		names.insert(names.end(), aliases.begin(), aliases.end());

		for (const auto& n : names)
		{
			if (m_names.contains(std::string(n)))
			{
				m_error = Error::NAME_ALREADY_USED;
				m_aggregates.emplace_back(std::make_unique<Aggregate<T>>());
				return *(Aggregate<T>*)m_aggregates.back().get();
			}
			else
				m_names.insert(std::string(n));
		}

		m_aggregates.emplace_back(std::make_unique<Aggregate<T>>());
		m_aggregates.back()->AddName(name);
		m_aggregates.back()->AddAliases(aliases);
		return *(Aggregate<T>*)m_aggregates.back().get();
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
		return m_set;
	}

	template<Parseable T>
	inline T Argument<T>::Value() const
	{
		if (!m_locked)
			return T{};

		return m_parsed_val;
	}

	template<Parseable T>
	inline Argument<T>& Argument<T>::Required()
	{
		if (m_locked)
			return *this;

		if (m_has_default)
		{
			m_error = Error::REQUIRED_HAS_DEFAULT;
		}
		else
		{
			m_required = true;
			if (!m_set)
				m_error = Error::MISSING_REQUIRED;
		}

		return *this;
	}

	template<Parseable T>
	inline Argument<T>& Argument<T>::Default(const T& default_value)
	{
		if (m_locked)
			return *this;

		if (m_required)
		{
			m_error = Error::REQUIRED_HAS_DEFAULT;
		}
		else if (m_has_default)
		{
			m_error = Error::DEFAULT_ALREADY_SET;
		}
		else
		{
			m_default_val = default_value;
			m_has_default = true;
		}

		return *this;
	}
	
	template<Parseable T>
	template<class Validation>
	requires std::invocable<Validation&, std::string_view>&&
	std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, bool>
	inline Argument<T>& Parser::Argument<T>::ValidateText(Validation&& val)
	{
		if (m_locked)
			return *this;

		m_text_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

	template<Parseable T>
	template<class Validation>
	requires std::invocable<Validation&, const T&>&&
	std::convertible_to<std::invoke_result_t<Validation&, const T&>, bool>
		inline Argument<T>& Parser::Argument<T>::ValidateValue(Validation&& val)
	{
		if (m_locked)
			return *this;

		m_val_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

	template<Parseable T>
	template<class Transformation>
	requires std::invocable<Transformation&, std::string&>&&
	std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, bool>
	inline Argument<T>& Parser::Argument<T>::Transform(Transformation&& trans)
	{
		if (m_locked)
			return *this;

		m_text_trans.emplace_back(std::forward<Transformation>(trans));
		return *this;
	}

	template<Parseable T>
	inline Argument<T>& Argument<T>::Help(std::string_view help_msg)
	{
		if (m_locked)
			return *this;

		m_help = help_msg;
		if (m_help.back() != '\n')
			m_help += '\n';
		return *this;
	}

	template<Parseable T>
	inline void Argument<T>::Finalize()
	{
		if (m_locked || m_error != Error::SUCCESS)
			return;

		m_locked = true;

		if (m_set)
		{
			for (auto& t : m_text_trans)
			{
				std::string tmp = m_value;
				if (!std::invoke(t, tmp))
				{
					m_error = Error::TRANSFORMATION_ERROR;
					return;
				}
				m_value = tmp;
			}

			for (auto& v : m_text_vals)
			{
				if (!std::invoke(v, m_value))
				{
					m_error = Error::TEXT_VALIDATION_INVALID;
					return;
				}
			}

			if (!Parse(m_value, m_parsed_val))
			{
				m_error = Error::PARSE_FAIL;
				return;
			}
		}
		else if (m_has_default)
			m_parsed_val = m_default_val;
		else
			return;

		for (auto& v : m_val_vals)
		{
			if (!std::invoke(v,m_parsed_val))
			{
				m_error = Error::VAL_VALIDATION_INVALID;
				return;
			}
		}
	}

	template <typename T>
	bool ParseInteger(std::string_view v, T& out)
	{
		static_assert(std::is_integral_v<T>);

		if (v.empty())
			return false;

		T value{};

		const char* begin = v.data();
		const char* end = begin + v.size();

		const auto [ptr, ec] = std::from_chars(begin, end, value, 10);

		if (ec != std::errc{} || ptr != end)
			return false;

		out = value;
		return true;
	}

	template <typename T>
	bool ParseFloatingPoint(std::string_view v, T& out)
	{
		static_assert(std::is_floating_point_v<T>);

		if (v.empty())
			return false;

		T value{};

		const char* begin = v.data();
		const char* end = begin + v.size();

		const auto [ptr, ec] = std::from_chars(begin, end, value, std::chars_format::general);

		if (ec != std::errc{} || ptr != end)
			return false;

		out = value;
		return true;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Default(const std::vector<T>& default_value)
	{
		if (m_locked)
			return *this;

		if (m_required)
		{
			m_error = Error::REQUIRED_HAS_DEFAULT;
		}
		else if (m_has_default)
		{
			m_error = Error::DEFAULT_ALREADY_SET;
		}
		else
		{
			m_default_vals = default_value;
			m_has_default = true;
		}

		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Count(size_t count)
	{
		if (m_locked)
			return *this;
		m_exact_count = count;
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::MinCount(size_t count)
	{
		if (m_locked)
			return *this;
		m_min_count = count;
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::MaxCount(size_t count)
	{
		if (m_locked)
			return *this;
		m_max_count = count;
		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Unlimited()
	{
		if (m_locked)
			return *this;
		m_unlimited = true;
		if (m_min_count == 0)
			m_min_count = 1;
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
		return m_set;
	}

	template<Parseable T>
	inline std::vector<T> Aggregate<T>::Value() const
	{
		return m_parsed_vals;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Required()
	{
		if (m_locked)
			return *this;

		if (m_has_default)
			m_error = Error::REQUIRED_HAS_DEFAULT;
		else
		{
			m_required = true;
			if (!m_set)
				m_error = Error::MISSING_REQUIRED;
		}

		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Help(std::string_view help_msg)
	{
		m_help = help_msg;
		return *this;
	}

	template<Parseable T>
	inline void Aggregate<T>::Finalize()
	{
		if (m_locked || m_error != Error::SUCCESS)
			return;

		m_locked = true;

		m_error = ValidateCardinality();
		if (m_error != Error::SUCCESS)
			return;

		if (m_set)
		{
			for (auto& val : m_values)
			{
				for (auto& t : m_text_trans)
				{
					std::string tmp = val;
					if (!std::invoke(t, tmp))
					{
						m_error = Error::TRANSFORMATION_ERROR;
						return;
					}
					val = tmp;
				}

				for (auto& v : m_text_vals)
				{
					if (!std::invoke(v, val))
					{
						m_error = Error::TEXT_VALIDATION_INVALID;
						return;
					}
				}

				T tmp{};
				if (!Parse(val, tmp))
				{
					m_error = Error::PARSE_FAIL;
					return;
				}

				m_parsed_vals.emplace_back(tmp);
			}
		}
		else if (m_has_default)
			m_parsed_vals = m_default_vals;
		else
			return;

		m_error = ApplyCardinality();
		if (m_error != Error::SUCCESS)
			return;

		for (auto& v : m_val_vals)
		{
			for (auto& val : m_parsed_vals)
			{
				if (!std::invoke(v, val))
				{
					m_error = Error::VAL_VALIDATION_INVALID;
					return;
				}
			}
		}

		for (auto& v : m_col_vals)
		{
			if (!std::invoke(v, m_parsed_vals))
			{
				m_error = Error::COL_VALIDATION_INVALID;
				return;
			}
		}
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
	inline Error Aggregate<T>::ApplyCardinality()
	{
		if (m_exact_count != 0 && m_parsed_vals.size() != m_exact_count)
			return Error::CARDINALITY_ERROR;
		else if (m_min_count != 0)
		{
			if (m_parsed_vals.size() < m_min_count)
				return Error::CARDINALITY_ERROR;

			if (!m_unlimited && m_max_count < m_parsed_vals.size())
			{
				return Error::CARDINALITY_ERROR;
			}
		}

		return Error::SUCCESS;
	}

	template<Parseable T>
	inline Error Aggregate<T>::ValidateCardinality()
	{
		if (m_exact_count == 0 && !m_unlimited && m_max_count == 0 && m_min_count == 0)
			return Error::NO_CARDINALITY_SET;
		else if (m_exact_count != 0 &&
			(m_unlimited || m_max_count != 0 || m_min_count != 0))
			return Error::CARDINALITY_INCOMPATIBLE;
		else if (m_unlimited && m_max_count != 0)
			return Error::CARDINALITY_INCOMPATIBLE;
		else if (m_min_count != 0 && m_max_count == 0 && !m_unlimited)
			return Error::MAX_COUNT_NOT_SET;
		else if (m_max_count != 0 && m_min_count == 0)
			return Error::MIN_COUNT_NOT_SET;
		else if (m_max_count < m_min_count && !m_unlimited)
			return Error::MIN_GREATER_MAX;

		return Error::SUCCESS;
	}

	template<Parseable T>
	template <class Validation>
	requires std::invocable<Validation&, std::string_view>&&
	std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, bool>
	inline Aggregate<T>& Aggregate<T>::ValidateText(Validation&& val)
	{
		if (m_locked)
			return *this;

		m_text_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

	template<Parseable T>
	template <class Validation>
	requires std::invocable<Validation&, const T&>&&
	std::convertible_to<std::invoke_result_t<Validation&, const T&>, bool>
	inline Aggregate<T>& Aggregate<T>::ValidateValue(Validation&& val)
	{
		if (m_locked)
			return *this;

		m_val_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

	template<Parseable T>
	template<class Transformation>
	requires std::invocable<Transformation&, std::string&>&&
	std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, bool>
	inline Aggregate<T>& Aggregate<T>::Transform(Transformation&& trans)
	{
		if (m_locked)
			return *this;

		m_text_trans.emplace_back(std::forward<Transformation>(trans));
		return *this;
	}

	template<Parseable T>		
	template<class Validation>
	requires std::invocable<Validation&, const std::vector<T>&>&&
	std::convertible_to<std::invoke_result_t<Validation&, const std::vector<T>&>, bool>
	inline Aggregate<T>& Aggregate<T>::ValidateCollection(Validation&& val)
	{
		if (m_locked)
			return *this;

		m_col_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

}