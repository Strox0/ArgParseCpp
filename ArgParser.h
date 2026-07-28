#pragma once
#include <type_traits>
#include <memory>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>
#include <vector>
#include <string>
#include <unordered_set>
#include <functional>
#include <concepts>
#include <utility>
#include <unordered_map>
#include <stdexcept>

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
		UNKNOWN_VALUE,
		HELP_QUERY,
		CARDINALITY_VALIDATION_FAIL,
		FLAG_HAS_EQUALS_VALUE,
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
		{Parse(v, out)} -> std::convertible_to<bool>;
	} 
	&& std::is_default_constructible_v<T>
	&& std::is_copy_assignable_v<T>
	&& std::is_copy_constructible_v<T>
	&& !std::is_same_v<T,bool>;

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
		virtual void AddValue(std::string_view value);

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
		const T& ValueRef() const;
		T ValueOr(const T& backup_val) const;

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
		Aggregate<T>& Help(std::string_view help_msg);

		Aggregate<T>& Exactly(size_t count);
		Aggregate<T>& Between(size_t min, size_t max);
		Aggregate<T>& AtLeast(size_t count);
		Aggregate<T>& AtMost(size_t count);
		Aggregate<T>& Unlimited();

	protected:
		void Finalize() override;
		void AppendValue(std::string_view val) override;

	private:
		enum class Cardinality
		{
			UNLIMITED,
			BETWEEN,
			EXACTLY,
			ATLEAST,
			ATMOST,
		};

		Error ApplyCardinality();

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
		Cardinality m_card = Cardinality::UNLIMITED;
		bool m_card_set = false;
	};

	class ArgParser
	{
	public:
		ArgParser(int argc, char** argv);
		ArgParser() = delete;

		template<typename T>
		Argument<T>& Add(const std::string& name, const std::vector<std::string>& aliases = {});

		template<typename T>
		Argument<T>& AddPositional(const std::string& helper_name);

		template<typename T>
		Aggregate<T>& AddAggregate(const std::string& name, const std::vector<std::string>& aliases = {});

		Flag& AddFlag(const std::string& name, const std::vector<std::string>& aliases = {});

		void Help(std::string_view help_msg);

		Error ParseAndValidate();

	private:
		Error HandleError();
		void CheckAndRegisterNames(const std::string& name, const std::vector<std::string>& al);
		bool IsKnownName(const std::string& name);
		bool ResolveName(std::string& name);
		std::pair<std::string,std::string> ParseTokenWithEquals(const std::string& token, bool& has_value);

	private:

		enum class ArgType
		{
			Scalar,
			Aggregate,
			Flag
		};

		std::vector<std::shared_ptr<ArgumentBase>> m_args;
		std::vector<std::unique_ptr<ArgumentBase>> m_positionals;
		std::vector<std::string> m_tokens;
		Error m_error;
		std::string m_help;
		std::unordered_map<std::string, std::pair<ArgType,std::shared_ptr<ArgumentBase>>> m_name_arg_map;
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
		if (!m_locked || m_error != Error::SUCCESS)
			throw std::logic_error("Value called in an invalid state");

		return m_parsed_val;
	}

	template<Parseable T>
	inline const T& Argument<T>::ValueRef() const
	{
		if (!m_locked || m_error != Error::SUCCESS)
			throw std::logic_error("ValueRef called in an invalid state");

		return m_parsed_val;
	}

	template<Parseable T>
	inline T Argument<T>::ValueOr(const T& backup_val) const
	{
		if (!m_locked)
			throw std::logic_error("ValueOr called before ParseAndValidate");

		if (m_set && m_error == Error::SUCCESS)
			return m_parsed_val;
		else if (m_has_default)
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
			if (!m_set)
				m_error = Error::MISSING_REQUIRED;
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
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

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
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

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
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_text_trans.emplace_back(std::forward<Transformation>(trans));
		return *this;
	}

	template<Parseable T>
	inline Argument<T>& Argument<T>::Help(std::string_view help_msg)
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
		return m_set;
	}

	template<Parseable T>
	inline std::vector<T> Aggregate<T>::Value() const
	{
		if (!m_locked || m_error != Error::SUCCESS)
			throw std::logic_error("Value called in an invalid state");

		return m_parsed_vals;
	}

	template<Parseable T>
	inline const std::vector<T>& Aggregate<T>::ValueRef() const
	{
		if (!m_locked || m_error != Error::SUCCESS)
			throw std::logic_error("ValueRef called in an invalid state");

		return m_parsed_vals;
	}

	template<Parseable T>
	inline std::vector<T> Aggregate<T>::ValueOr(const std::vector<T>& backup_val) const
	{
		if (!m_locked)
			throw std::logic_error("ValueOr cannot be called before ParseAndValidate");

		if (m_set && m_error == Error::SUCCESS)
			return m_parsed_vals;
		else if (m_has_default)
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
			if (!m_set)
				m_error = Error::MISSING_REQUIRED;
		}

		return *this;
	}

	template<Parseable T>
	inline Aggregate<T>& Aggregate<T>::Help(std::string_view help_msg)
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
	inline void Aggregate<T>::Finalize()
	{
		if (m_locked || m_error != Error::SUCCESS)
			return;

		m_locked = true;

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
		switch (m_card)
		{
		case Parser::Aggregate<T>::Cardinality::BETWEEN:
			if (m_parsed_vals.size() < m_min_count || m_parsed_vals.size() > m_max_count)
				return Error::CARDINALITY_VALIDATION_FAIL;
			break;
		case Parser::Aggregate<T>::Cardinality::EXACTLY:
			if (m_parsed_vals.size() != m_exact_count)
				return Error::CARDINALITY_VALIDATION_FAIL;
			break;
		case Parser::Aggregate<T>::Cardinality::ATLEAST:
			if (m_parsed_vals.size() < m_min_count)
				return Error::CARDINALITY_VALIDATION_FAIL;
			break;
		case Parser::Aggregate<T>::Cardinality::ATMOST:
			if (m_parsed_vals.size() > m_max_count)
				return Error::CARDINALITY_VALIDATION_FAIL;
			break;
		case Parser::Aggregate<T>::Cardinality::UNLIMITED:
			break;
		}

		return Error::SUCCESS;
	}

	template<Parseable T>
	template <class Validation>
	requires std::invocable<Validation&, std::string_view>&&
	std::convertible_to<std::invoke_result_t<Validation&, std::string_view>, bool>
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
	std::convertible_to<std::invoke_result_t<Validation&, const T&>, bool>
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
	std::convertible_to<std::invoke_result_t<Transformation&, std::string&>, bool>
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
	std::convertible_to<std::invoke_result_t<Validation&, const std::vector<T>&>, bool>
	inline Aggregate<T>& Aggregate<T>::ValidateCollection(Validation&& val)
	{
		if (m_locked)
			throw std::logic_error("Argument cannot be configured after ParseAndValidate");

		m_col_vals.emplace_back(std::forward<Validation>(val));
		return *this;
	}

}