// AI Generated Test cases


#include "ArgParser.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <chrono>

namespace parser_tests
{

	class TestFailure final : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	class TestSkipped final : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	[[noreturn]] void Fail(
		const char* expression,
		const char* file,
		int line,
		std::string_view detail = {})
	{
		std::ostringstream out;
		out << file << ':' << line << ": check failed: " << expression;
		if (!detail.empty())
		{
			out << " (" << detail << ')';
		}
		throw TestFailure(out.str());
	}

#define CHECK(expression)                                                        \
	do                                                                           \
	{                                                                            \
		if (!(expression))                                                       \
		{                                                                        \
			::parser_tests::Fail(#expression, __FILE__, __LINE__);              \
		}                                                                        \
	} while (false)

#define CHECK_FALSE(expression) CHECK(!(expression))

#define CHECK_EQ(actual_expression, expected_expression)                         \
	do                                                                           \
	{                                                                            \
		const auto& parser_test_actual = (actual_expression);                    \
		const auto& parser_test_expected = (expected_expression);                \
		if (!(parser_test_actual == parser_test_expected))                       \
		{                                                                        \
			::parser_tests::Fail(                                                \
				#actual_expression " == " #expected_expression,                 \
				__FILE__,                                                        \
				__LINE__);                                                       \
		}                                                                        \
	} while (false)

#define CHECK_NEAR(actual_expression, expected_expression, tolerance_expression) \
	do                                                                           \
	{                                                                            \
		const long double parser_test_actual =                                   \
			static_cast<long double>(actual_expression);                         \
		const long double parser_test_expected =                                 \
			static_cast<long double>(expected_expression);                       \
		const long double parser_test_tolerance =                                \
			static_cast<long double>(tolerance_expression);                      \
		if (std::fabs(parser_test_actual - parser_test_expected) >                \
			parser_test_tolerance)                                               \
		{                                                                        \
			::parser_tests::Fail(                                                \
				#actual_expression " ~= " #expected_expression,                 \
				__FILE__,                                                        \
				__LINE__);                                                       \
		}                                                                        \
	} while (false)

#define CHECK_ERROR(parser_expression, expected_error)                           \
	do                                                                           \
	{                                                                            \
		const auto parser_test_expected_error = (expected_error);                 \
		const auto parser_test_parse_result =                                    \
			(parser_expression).ParseAndValidate();                                \
		CHECK_EQ(                                                               \
			parser_test_parse_result,                                              \
			parser_test_expected_error == Parser::Error::SUCCESS                   \
				? Parser::ArgParseResult::SUCCESS                                   \
				: Parser::ArgParseResult::ERROR);                                   \
		CHECK_EQ(                                                               \
			(parser_expression).GetDiagnostics().ec,                              \
			parser_test_expected_error);                                          \
	} while (false)

	template <typename Exception, typename Callable>
	void CheckThrowsAs(Callable&& callable, const char* expression, const char* file, int line)
	{
		try
		{
			std::invoke(std::forward<Callable>(callable));
		}
		catch (const Exception&)
		{
			return;
		}
		catch (...)
		{
			Fail(expression, file, line, "unexpected exception type");
		}

		Fail(expression, file, line, "no exception was thrown");
	}

#define CHECK_THROWS_AS(expression, exception_type)                              \
	::parser_tests::CheckThrowsAs<exception_type>(                               \
		[&]() { static_cast<void>(expression); }, #expression, __FILE__, __LINE__)

	class SimulatedArgv
	{
	public:
		// The supplied strings are command-line fragments, not pre-split argv
		// tokens. They are joined with spaces and tokenized the same way a user
		// writes a command line: unquoted whitespace separates arguments, while
		// single- or double-quoted text remains one argument.
		//
		// Examples:
		//   { "--count 17" }         -> "--count", "17"
		//   { "--count", "17" }     -> "--count", "17"
		//   { R"(--name "two words")" } -> "--name", "two words"
		//   { "--count=17" }         -> "--count=17" (one argv token)
		SimulatedArgv(std::initializer_list<std::string_view> command_line_fragments)
		{
			std::string command_line;
			for (const std::string_view fragment : command_line_fragments)
			{
				if (!command_line.empty())
				{
					command_line.push_back(' ');
				}
				command_line.append(fragment);
			}

			BuildFromCommandLine(command_line);
		}

		explicit SimulatedArgv(std::string_view command_line)
		{
			BuildFromCommandLine(command_line);
		}

		int argc() const
		{
			return static_cast<int>(m_storage.size());
		}

		char** argv()
		{
			return m_pointers.data();
		}

	private:
		void BuildFromCommandLine(std::string_view command_line)
		{
			m_storage.clear();
			m_storage.emplace_back("parser-tests");

			std::string current;
			bool token_started = false;
			char quote = '\0';

			for (std::size_t index = 0; index < command_line.size(); ++index)
			{
				const char character = command_line[index];

				if (quote != '\0')
				{
					if (character == quote)
					{
						quote = '\0';
					}
					else if (character == '\\' && index + 1 < command_line.size() &&
						command_line[index + 1] == quote)
					{
						current.push_back(command_line[++index]);
					}
					else
					{
						current.push_back(character);
					}
					token_started = true;
					continue;
				}

				if (character == '"' || character == '\'')
				{
					quote = character;
					token_started = true;
				}
				else if (std::isspace(static_cast<unsigned char>(character)) != 0)
				{
					if (token_started)
					{
						m_storage.push_back(std::move(current));
						current.clear();
						token_started = false;
					}
				}
				else
				{
					current.push_back(character);
					token_started = true;
				}
			}

			if (quote != '\0')
			{
				throw std::invalid_argument(
					"SimulatedArgv command line contains an unterminated quote");
			}

			if (token_started)
			{
				m_storage.push_back(std::move(current));
			}

			RebuildPointers();
		}

		void RebuildPointers()
		{
			m_pointers.clear();
			m_pointers.reserve(m_storage.size() + 1);
			for (std::string& argument : m_storage)
			{
				m_pointers.push_back(argument.data());
			}
			m_pointers.push_back(nullptr);
		}

		std::vector<std::string> m_storage;
		std::vector<char*> m_pointers;
	};

	class ScopedStreamCapture
	{
	public:
		explicit ScopedStreamCapture(std::ostream& stream)
			: m_stream(stream), m_original_buffer(stream.rdbuf(m_capture.rdbuf()))
		{
		}

		ScopedStreamCapture(const ScopedStreamCapture&) = delete;
		ScopedStreamCapture& operator=(const ScopedStreamCapture&) = delete;

		~ScopedStreamCapture()
		{
			m_stream.rdbuf(m_original_buffer);
		}

		std::string str() const
		{
			return m_capture.str();
		}

	private:
		std::ostream& m_stream;
		std::streambuf* m_original_buffer;
		std::ostringstream m_capture;
	};

	using TestFunction = void (*)();

	struct TestCase
	{
		std::string_view name;
		TestFunction function;
	};

	std::vector<TestCase>& Registry()
	{
		static std::vector<TestCase> registry;
		return registry;
	}

	struct Registrar
	{
		Registrar(std::string_view name, TestFunction function)
		{
			Registry().push_back({ name, function });
		}
	};

#define TEST(name)                                                               \
	static void name();                                                          \
	static const ::parser_tests::Registrar registrar_##name{ #name, &name };     \
	static void name()

	using Parser::Error;

	void CheckReportedError(
		Parser::ArgParser& parser,
		Error expected_error,
		std::string_view expected_argument,
		std::string_view expected_token,
		std::string_view expected_detail,
		std::string_view expected_message)
	{
		CHECK_EQ(parser.ParseAndValidate(), Parser::ArgParseResult::ERROR);

		const Parser::Diagnostic& diagnostic = parser.GetDiagnostics();
		CHECK_EQ(diagnostic.ec, expected_error);
		CHECK_EQ(diagnostic.arg_name, std::string(expected_argument));
		CHECK_EQ(diagnostic.opt_token, std::string(expected_token));
		CHECK_EQ(diagnostic.opt_error_message, std::string(expected_detail));
		CHECK_EQ(parser.GetErrorMessage(), std::string(expected_message));
	}

	// -----------------------------------------------------------------------------
	// Direct Parse overloads
	// -----------------------------------------------------------------------------

	template <typename T>
	std::string IntegerString(T value)
	{
		if constexpr (std::is_signed_v<T>)
		{
			return std::to_string(static_cast<long long>(value));
		}
		else
		{
			return std::to_string(static_cast<unsigned long long>(value));
		}
	}

	template <typename T>
	void CheckSignedIntegerParse()
	{
		T value{};

		CHECK(Parser::Parse("0", value));
		CHECK_EQ(value, static_cast<T>(0));

		CHECK(Parser::Parse("42", value));
		CHECK_EQ(value, static_cast<T>(42));

		CHECK(Parser::Parse("-17", value));
		CHECK_EQ(value, static_cast<T>(-17));

		const std::string minimum = IntegerString(std::numeric_limits<T>::min());
		CHECK(Parser::Parse(minimum, value));
		CHECK_EQ(value, std::numeric_limits<T>::min());

		const std::string maximum = IntegerString(std::numeric_limits<T>::max());
		CHECK(Parser::Parse(maximum, value));
		CHECK_EQ(value, std::numeric_limits<T>::max());

		CHECK_FALSE(Parser::Parse("", value));
		CHECK_FALSE(Parser::Parse("abc", value));
		CHECK_FALSE(Parser::Parse("12x", value));

		const std::string above_maximum =
			std::to_string(static_cast<long long>(std::numeric_limits<T>::max())) +
			"0";
		CHECK_FALSE(Parser::Parse(above_maximum, value));
	}

	template <typename T>
	void CheckUnsignedIntegerParse()
	{
		T value{};

		CHECK(Parser::Parse("0", value));
		CHECK_EQ(value, static_cast<T>(0));

		CHECK(Parser::Parse("42", value));
		CHECK_EQ(value, static_cast<T>(42));

		const std::string maximum = IntegerString(std::numeric_limits<T>::max());
		CHECK(Parser::Parse(maximum, value));
		CHECK_EQ(value, std::numeric_limits<T>::max());

		CHECK_FALSE(Parser::Parse("-1", value));
		CHECK_FALSE(Parser::Parse("", value));
		CHECK_FALSE(Parser::Parse("abc", value));
		CHECK_FALSE(Parser::Parse("12x", value));

		const std::string above_maximum = maximum + "0";
		CHECK_FALSE(Parser::Parse(above_maximum, value));
	}

	TEST(ParseInt64)
	{
		CheckSignedIntegerParse<std::int64_t>();
	}

	TEST(ParseUint64)
	{
		CheckUnsignedIntegerParse<std::uint64_t>();
	}

	TEST(ParseInt32)
	{
		CheckSignedIntegerParse<std::int32_t>();
	}

	TEST(ParseUint32)
	{
		CheckUnsignedIntegerParse<std::uint32_t>();
	}

	TEST(ParseInt16)
	{
		CheckSignedIntegerParse<std::int16_t>();
	}

	TEST(ParseUint16)
	{
		CheckUnsignedIntegerParse<std::uint16_t>();
	}

	TEST(ParseInt8)
	{
		CheckSignedIntegerParse<std::int8_t>();
	}

	TEST(ParseUint8)
	{
		CheckUnsignedIntegerParse<std::uint8_t>();
	}

	template <typename T>
	void CheckFloatingParse()
	{
		T value{};

		CHECK(Parser::Parse("0", value));
		CHECK_NEAR(value, static_cast<T>(0), static_cast<T>(0));

		CHECK(Parser::Parse("1.5", value));
		CHECK_NEAR(value, static_cast<T>(1.5), static_cast<T>(0.00001));

		CHECK(Parser::Parse("-2.25", value));
		CHECK_NEAR(value, static_cast<T>(-2.25), static_cast<T>(0.00001));

		CHECK(Parser::Parse("1e3", value));
		CHECK_NEAR(value, static_cast<T>(1000), static_cast<T>(0.001));

		CHECK_FALSE(Parser::Parse("", value));
		CHECK_FALSE(Parser::Parse("abc", value));
		CHECK_FALSE(Parser::Parse("1.2x", value));
	}

	TEST(ParseFloat)
	{
		CheckFloatingParse<float>();
	}

	TEST(ParseDouble)
	{
		CheckFloatingParse<double>();
	}

	TEST(ParseLongDouble)
	{
		CheckFloatingParse<long double>();
	}

	TEST(ParseString)
	{
		std::string value = "old";

		CHECK(Parser::Parse("hello world", value));
		CHECK_EQ(value, std::string("hello world"));

		CHECK(Parser::Parse("", value));
		CHECK_EQ(value, std::string());
	}

	struct VoidParseResult
	{
	};

	void Parse(std::string_view, VoidParseResult&)
	{
	}

	TEST(ParseableRequiresBooleanLikeParseResult)
	{
		CHECK_FALSE(Parser::Parseable<VoidParseResult>);
	}

	// -----------------------------------------------------------------------------
	// Scalar named arguments
	// -----------------------------------------------------------------------------

	TEST(NamedArgumentSeparateTokensSucceed)
	{
		SimulatedArgv args{ "--count", "17" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& count = parser.Add<std::int64_t>("--count");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(count.Provided());
		CHECK_EQ(count.Value(), std::int64_t{ 17 });
	}

	TEST(NamedArgumentEqualsSyntaxSucceeds)
	{
		SimulatedArgv args{ "--count=17" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& count = parser.Add<std::int64_t>("--count");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(count.Provided());
		CHECK_EQ(count.Value(), std::int64_t{ 17 });
	}

	TEST(NamedArgumentCommandLineStringIsSplitOnWhitespace)
	{
		SimulatedArgv args{ "--count 17" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& count = parser.Add<std::int64_t>("--count");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(count.Provided());
		CHECK_EQ(count.Value(), std::int64_t{ 17 });
	}

	TEST(NamedArgumentAliasSeparateTokensSucceed)
	{
		SimulatedArgv args{ "-c", "21" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& count = parser.Add<std::int64_t>("--count", { "-c", "--iterations" });

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(count.Provided());
		CHECK_EQ(count.Value(), std::int64_t{ 21 });
	}

	TEST(NamedArgumentAliasEqualsSyntaxSuccess)
	{
		SimulatedArgv args{ "-c=21" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& count = parser.Add<std::int64_t>("--count", { "-c" });

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(count.Provided());
		CHECK_EQ(count.Value(), std::int64_t{ 21 });
	}

	TEST(NamedStringEmptyEqualsSyntaxIsMissingValue)
	{
		SimulatedArgv args{ "--name=" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& name = parser.Add<std::string>("--name");

		CHECK_ERROR(parser, Error::MISSING_VALUE);
	}

	TEST(OptionalArgumentWithoutDefaultIsNotProvided)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& value = parser.Add<std::int64_t>("--value");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(value.Provided());
	}

	TEST(DefaultIsUsedAndDoesNotCountAsProvided)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& output = parser.Add<std::string>("--output").Default("fallback.txt");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(output.Provided());
		CHECK_EQ(output.Value(), std::string("fallback.txt"));
	}

	TEST(ExplicitValueOverridesDefault)
	{
		SimulatedArgv args{ "--output", "chosen.txt" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& output = parser.Add<std::string>("--output").Default("fallback.txt");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(output.Provided());
		CHECK_EQ(output.Value(), std::string("chosen.txt"));
	}

	TEST(RequiredArgumentPresent)
	{
		SimulatedArgv args{ "--path", "input.txt" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& path = parser.Add<std::string>("--path").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(path.Provided());
		CHECK_EQ(path.Value(), std::string("input.txt"));
	}

	TEST(RequiredArgumentMissing)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--path").Required();

		CHECK_ERROR(parser, Error::MISSING_REQUIRED);
	}

	TEST(MissingNamedArgumentValue)
	{
		SimulatedArgv args{ "--count" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::int64_t>("--count");

		CHECK_ERROR(parser, Error::MISSING_VALUE);
	}

	TEST(NumericParseFailure)
	{
		SimulatedArgv args{ "--count", "not-a-number" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::int64_t>("--count");

		CHECK_ERROR(parser, Error::PARSE_FAIL);
	}

	TEST(EmptyNumericRequiredEqualsSyntaxIsMissingValue)
	{
		SimulatedArgv args{ "--count=" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::int64_t>("--count").Required();

		CHECK_ERROR(parser, Error::MISSING_VALUE);
	}

	TEST(OptionTerminatorIsNotConsumedAsScalarValue)
	{
		SimulatedArgv args{ "--count", "--", "tail" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::int64_t>("--count");

		CHECK_ERROR(parser, Error::MISSING_VALUE);
	}

	TEST(NegativeNamedNumericValueIsNotMistakenForOption)
	{
		SimulatedArgv args{ "--offset", "-42" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& offset = parser.Add<std::int64_t>("--offset");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(offset.Provided());
		CHECK_EQ(offset.Value(), std::int64_t{ -42 });
	}

	TEST(UnknownNamedArgument)
	{
		SimulatedArgv args{ "--does-not-exist" };
		Parser::ArgParser parser(args.argc(), args.argv());

		CHECK_ERROR(parser, Error::UNKNOWN_ARGUMENT);
	}

	TEST(ExtraBareTokenIsUnknown)
	{
		SimulatedArgv args{ "unclaimed" };
		Parser::ArgParser parser(args.argc(), args.argv());

		CHECK_ERROR(parser, Error::UNKNOWN_ARGUMENT);
	}

	TEST(DoubleDashSeparatorIsImplemented)
	{
		SimulatedArgv args{ "--" };
		Parser::ArgParser parser(args.argc(), args.argv());

		CHECK_ERROR(parser, Error::SUCCESS);
	}

	TEST(HelpMetadataDoesNotAffectSuccessfulParsing)
	{
		SimulatedArgv args{ "--path", "input.txt" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Help("Program-level help.");
		auto& path = parser.Add<std::string>("--path")
			.Help("Input path.")
			.Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(path.Value(), std::string("input.txt"));
	}

	// -----------------------------------------------------------------------------
	// Text validation, transformation, and value validation
	// -----------------------------------------------------------------------------

	TEST(TextValidationAcceptsValue)
	{
		SimulatedArgv args{ "--name", "letters-only" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& name = parser.Add<std::string>("--name").ValidateText(
			[](std::string_view text)
			{
				return std::all_of(
					text.begin(), text.end(), [](unsigned char c)
					{
						return c == '-' || std::isalpha(c) != 0;
					});
			});

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(name.Value(), std::string("letters-only"));
	}

	TEST(TextValidationRejectsValue)
	{
		SimulatedArgv args{ "--name", "abc123" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--name").ValidateText(
			[](std::string_view text)
			{
				return std::none_of(
					text.begin(), text.end(), [](unsigned char c)
					{
						return std::isdigit(c) != 0;
					});
			});

		CHECK_ERROR(parser, Error::TEXT_VALIDATION_INVALID);
	}

	TEST(TransformationChangesStoredString)
	{
		SimulatedArgv args{ "--name", "MiXeD" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& name = parser.Add<std::string>("--name").Transform(
			[](std::string& text)
			{
				std::transform(
					text.begin(), text.end(), text.begin(), [](unsigned char c)
					{
						return static_cast<char>(std::tolower(c));
					});
				return true;
			});

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(name.Value(), std::string("mixed"));
	}

	TEST(TransformationRunsBeforeNumericParsing)
	{
		SimulatedArgv args{ "--amount", "1,234" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& amount = parser.Add<std::int64_t>("--amount").Transform(
			[](std::string& text)
			{
				std::erase(text, ',');
				return true;
			});

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(amount.Value(), std::int64_t{ 1234 });
	}

	TEST(TransformationFailure)
	{
		SimulatedArgv args{ "--name", "anything" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--name").Transform(
			[](std::string&)
			{
				return false;
			});

		CHECK_ERROR(parser, Error::TRANSFORMATION_ERROR);
	}

	TEST(ValueValidationAcceptsParsedValue)
	{
		SimulatedArgv args{ "--scale", "2.5" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& scale = parser.Add<double>("--scale").ValidateValue(
			[](const double& value)
			{
				return value > 0.0;
			});

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_NEAR(scale.Value(), 2.5, 0.000001);
	}

	TEST(ValueValidationRejectsParsedValue)
	{
		SimulatedArgv args{ "--scale", "-0.5" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<double>("--scale").ValidateValue(
			[](const double& value)
			{
				return value > 0.0;
			});

		CHECK_ERROR(parser, Error::VAL_VALIDATION_INVALID);
	}

	TEST(MultipleTransformationsRunInRegistrationOrder)
	{
		SimulatedArgv args{ "--name", "AbC" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& name = parser.Add<std::string>("--name")
			.Transform(
				[](std::string& value)
				{
					value += "-SUFFIX";
					return true;
				})
			.Transform(
				[](std::string& value)
				{
					std::transform(
						value.begin(),
						value.end(),
						value.begin(),
						[](unsigned char c)
						{
							return static_cast<char>(std::tolower(c));
						});
					return true;
				});

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(name.Value(), std::string("abc-suffix"));
	}

	// -----------------------------------------------------------------------------
	// Flags
	// -----------------------------------------------------------------------------

	TEST(FlagDefaultsFalse)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& debug = parser.AddFlag("--debug");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(debug.Value());
	}

	TEST(FlagCanonicalNameSetsTrue)
	{
		SimulatedArgv args{ "--debug" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& debug = parser.AddFlag("--debug");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(debug.Value());
	}

	TEST(FlagAliasSetsTrue)
	{
		SimulatedArgv args{ "-d" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& debug = parser.AddFlag("--debug", { "-d" }).Help("Debug output.");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(debug.Value());
	}

	TEST(MultipleFlagsAreIndependent)
	{
		SimulatedArgv args{ "--debug", "-v" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& debug = parser.AddFlag("--debug", { "-d" });
		auto& verbose = parser.AddFlag("--verbose", { "-v" });
		auto& quiet = parser.AddFlag("--quiet", { "-q" });

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(debug.Value());
		CHECK(verbose.Value());
		CHECK_FALSE(quiet.Value());
	}

	TEST(FlagAfterDoubleDashRemainsPositional)
	{
		SimulatedArgv args{ "--", "--debug" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& debug = parser.AddFlag("--debug");
		auto& value = parser.AddPositional<std::string>("value").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(debug.Value());
		CHECK_EQ(value.Value(), std::string("--debug"));
	}

	TEST(AttachedScalarValueMatchingFlagNameIsNotStolen)
	{
		SimulatedArgv args{ "--output=--verbose" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& verbose = parser.AddFlag("--verbose");
		auto& output = parser.Add<std::string>("--output");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(verbose.Value());
		CHECK_EQ(output.Value(), std::string("--verbose"));
	}

	TEST(AbsentFlagDoesNotConsumeEmptyArgument)
	{
		SimulatedArgv args{ R"("" tail)" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& verbose = parser.AddFlag("--verbose");
		auto& first = parser.AddPositional<std::string>("first").Required();
		auto& second = parser.AddPositional<std::string>("second").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(verbose.Value());
		CHECK_EQ(first.Value(), std::string());
		CHECK_EQ(second.Value(), std::string("tail"));
	}

	template <typename P>
	concept SupportsAddBool = requires(P & parser)
	{
		parser.template Add<bool>("--bad-bool");
	};

	TEST(AddBoolIsRejectedAtCompileTime)
	{
		CHECK_FALSE(SupportsAddBool<Parser::ArgParser>);
	}

	// -----------------------------------------------------------------------------
	// Positional arguments
	// -----------------------------------------------------------------------------

	TEST(RequiredPositionalArgument)
	{
		SimulatedArgv args{ "hello" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& text = parser.AddPositional<std::string>("text").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(text.Provided());
		CHECK_EQ(text.Value(), std::string("hello"));
	}

	TEST(MissingRequiredPositionalArgument)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddPositional<std::string>("text").Required();

		CHECK_ERROR(parser, Error::MISSING_REQUIRED);
	}

	TEST(OptionalPositionalUsesDefault)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& number = parser.AddPositional<std::int64_t>("number").Default(66);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(number.Provided());
		CHECK_EQ(number.Value(), std::int64_t{ 66 });
	}

	TEST(MultiplePositionalsAreAssignedInRegistrationOrder)
	{
		SimulatedArgv args{ "first", "42" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& first = parser.AddPositional<std::string>("first").Required();
		auto& second = parser.AddPositional<std::int64_t>("second").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(first.Value(), std::string("first"));
		CHECK_EQ(second.Value(), std::int64_t{ 42 });
	}

	TEST(NamedAndPositionalArgumentsMayBeInterleaved)
	{
		SimulatedArgv args{ "input.txt", "--count", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& input = parser.AddPositional<std::string>("input").Required();
		auto& count = parser.Add<std::int64_t>("--count").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(input.Value(), std::string("input.txt"));
		CHECK_EQ(count.Value(), std::int64_t{ 3 });
	}

	TEST(NamedArgumentMayAppearBeforePositional)
	{
		SimulatedArgv args{ "--count", "3", "input.txt" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& input = parser.AddPositional<std::string>("input").Required();
		auto& count = parser.Add<std::int64_t>("--count").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(input.Value(), std::string("input.txt"));
		CHECK_EQ(count.Value(), std::int64_t{ 3 });
	}

	TEST(NegativePositionalNumericValue)
	{
		SimulatedArgv args{ "-12" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& value = parser.AddPositional<std::int64_t>("value").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(value.Value(), std::int64_t{ -12 });
	}

	TEST(PositionalTransformationAndValidation)
	{
		SimulatedArgv args{ "HELLO" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& text = parser.AddPositional<std::string>("text")
			.Required()
			.ValidateText(
				[](std::string_view value)
				{
					return std::all_of(
						value.begin(), value.end(), [](unsigned char c)
						{
							return std::isalpha(c) != 0;
						});
				})
			.Transform(
				[](std::string& value)
				{
					std::transform(
						value.begin(),
						value.end(),
						value.begin(),
						[](unsigned char c)
						{
							return static_cast<char>(std::tolower(c));
						});
					return true;
				});

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(text.Value(), std::string("hello"));
	}

	TEST(PositionalContainingEqualsRemainsOneToken)
	{
		SimulatedArgv args{ "key=value" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& value = parser.AddPositional<std::string>("value").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(value.Value(), std::string("key=value"));
	}

	TEST(ExtraPositionalTokenIsUnknown)
	{
		SimulatedArgv args{ "first", "extra" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddPositional<std::string>("first").Required();

		CHECK_ERROR(parser, Error::UNKNOWN_ARGUMENT);
	}

	// -----------------------------------------------------------------------------
	// Aggregates
	// -----------------------------------------------------------------------------

	TEST(AggregateExactCount)
	{
		SimulatedArgv args{ "--items", "1", "2", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items").Exactly(3);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(items.Provided());
		CHECK_EQ(items.Value(), std::vector<std::int64_t>({ 1, 2, 3 }));
	}

	TEST(AggregateAlias)
	{
		SimulatedArgv args{ "-i", "4", "5" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items =
			parser.AddAggregate<std::int64_t>("--items", { "-i" }).Exactly(2);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(items.Provided());
		CHECK_EQ(items.Value(), std::vector<std::int64_t>({ 4, 5 }));
	}

	TEST(AggregateEqualsSyntaxSuccess)
	{
		SimulatedArgv args{ "--items=1", "2", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& aggr = parser.AddAggregate<std::int64_t>("--items").Exactly(3);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(aggr.Value(), std::vector<std::int64_t>({ 1, 2, 3 }));
	}

	TEST(AggregateStopsAtNextRecognizedOption)
	{
		SimulatedArgv args{
			"--items", "1", "2", "3", "--scale", "2.5", "--debug"
		};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items").Exactly(3);
		auto& scale = parser.Add<double>("--scale").Required();
		auto& debug = parser.AddFlag("--debug");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(items.Value(), std::vector<std::int64_t>({ 1, 2, 3 }));
		CHECK_NEAR(scale.Value(), 2.5, 0.000001);
		CHECK(debug.Value());
	}

	TEST(RemovingLaterScalarDoesNotExtendAggregate)
	{
		SimulatedArgv args{ "--items", "one", "--mode", "fast", "tail" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::string>("--items").Exactly(1);
		auto& mode = parser.Add<std::string>("--mode").Required();
		auto& tail = parser.AddPositional<std::string>("tail").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(items.Value(), std::vector<std::string>({ "one" }));
		CHECK_EQ(mode.Value(), std::string("fast"));
		CHECK_EQ(tail.Value(), std::string("tail"));
	}

	TEST(AggregateAcceptsNegativeNumbers)
	{
		SimulatedArgv args{ "--items", "-1", "-2", "-3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items").Exactly(3);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(items.Value(), std::vector<std::int64_t>({ -1, -2, -3 }));
	}

	TEST(AggregateMinAndMaxCount)
	{
		SimulatedArgv args{ "--items", "1", "2", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items").Between(2, 4);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(items.Value(), std::vector<std::int64_t>({ 1, 2, 3 }));
	}

	TEST(AggregateMinAndUnlimited)
	{
		SimulatedArgv args{ "--items", "1", "2", "3", "4", "5", "6" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items").AtLeast(2);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(
			items.Value(),
			std::vector<std::int64_t>({ 1, 2, 3, 4, 5, 6 }));
	}

	TEST(OptionalAggregateWithoutValueIsNotProvided)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items").Exactly(2);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(items.Provided());
	}

	TEST(AggregateDefaultIsUsedAndNotProvided)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items")
			.Exactly(2)
			.Default({ 7, 8 });

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_FALSE(items.Provided());
		CHECK_EQ(items.Value(), std::vector<std::int64_t>({ 7, 8 }));
	}

	TEST(ExplicitAggregateOverridesDefault)
	{
		SimulatedArgv args{ "--items", "1", "2" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items")
			.Exactly(2)
			.Default({ 7, 8 });

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(items.Provided());
		CHECK_EQ(items.Value(), std::vector<std::int64_t>({ 1, 2 }));
	}

	TEST(RequiredAggregatePresent)
	{
		SimulatedArgv args{ "--items", "1", "2" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items")
			.Required()
			.Exactly(2);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK(items.Provided());
	}

	TEST(RequiredAggregateMissing)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items").Required().Exactly(2);

		CHECK_ERROR(parser, Error::MISSING_REQUIRED);
	}

	TEST(AggregateTooFewValues)
	{
		SimulatedArgv args{ "--items", "1", "2" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items").Exactly(3);

		CHECK_ERROR(parser, Error::CARDINALITY_VALIDATION_FAIL);
	}

	TEST(AggregateTooManyValues)
	{
		SimulatedArgv args{ "--items", "1", "2", "3", "4" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items").Between(1, 3);

		CHECK_ERROR(parser, Error::CARDINALITY_VALIDATION_FAIL);
	}

	TEST(AggregateBelowMinimum)
	{
		SimulatedArgv args{ "--items", "1" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items").AtLeast(2);

		CHECK_ERROR(parser, Error::CARDINALITY_VALIDATION_FAIL);
	}

	TEST(AggregateElementParseFailure)
	{
		SimulatedArgv args{ "--items", "1", "bad", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items").Exactly(3);

		CHECK_ERROR(parser, Error::PARSE_FAIL);
	}

	TEST(AggregateTextValidationFailure)
	{
		SimulatedArgv args{ "--items", "good", "bad2" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::string>("--items")
			.Exactly(2)
			.ValidateText(
				[](std::string_view value)
				{
					return std::none_of(
						value.begin(), value.end(), [](unsigned char c)
						{
							return std::isdigit(c) != 0;
						});
				});

		CHECK_ERROR(parser, Error::TEXT_VALIDATION_INVALID);
	}

	TEST(AggregateValueValidationFailure)
	{
		SimulatedArgv args{ "--items", "1", "-2", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items")
			.Exactly(3)
			.ValidateValue(
				[](const std::int64_t& value)
				{
					return value > 0;
				});

		CHECK_ERROR(parser, Error::VAL_VALIDATION_INVALID);
	}

	TEST(AggregateTransformationAppliesToEachElement)
	{
		SimulatedArgv args{ "--items", "ONE", "TWO" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::string>("--items")
			.Exactly(2)
			.Transform(
				[](std::string& value)
				{
					std::transform(
						value.begin(),
						value.end(),
						value.begin(),
						[](unsigned char c)
						{
							return static_cast<char>(std::tolower(c));
						});
					return true;
				});

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(items.Value(), std::vector<std::string>({ "one", "two" }));
	}

	TEST(AggregateTransformationFailure)
	{
		SimulatedArgv args{ "--items", "good", "reject" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::string>("--items")
			.Exactly(2)
			.Transform(
				[](std::string& value)
				{
					return value != "reject";
				});

		CHECK_ERROR(parser, Error::TRANSFORMATION_ERROR);
	}

	TEST(AggregateCollectionValidationAcceptsCollection)
	{
		SimulatedArgv args{ "--items", "1", "2", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items")
			.Exactly(3)
			.ValidateCollection(
				[](const std::vector<std::int64_t>& values)
				{
					return values ==
						std::vector<std::int64_t>({ 1, 2, 3 });
				});

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(items.Value().size(), std::size_t{ 3 });
	}

	TEST(AggregateCollectionValidationRejectsCollection)
	{
		SimulatedArgv args{ "--items", "1", "2", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items")
			.Exactly(3)
			.ValidateCollection(
				[](const std::vector<std::int64_t>& values)
				{
					const auto sum = values[0] + values[1] + values[2];
					return sum < 5;
				});

		CHECK_ERROR(parser, Error::COL_VALIDATION_INVALID);
	}

	TEST(AggregateHelpMetadataDoesNotAffectParsing)
	{
		SimulatedArgv args{ "--items", "1", "2" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& items = parser.AddAggregate<std::int64_t>("--items")
			.Exactly(2)
			.Help("Two integers.");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(items.Value(), std::vector<std::int64_t>({ 1, 2 }));
	}

	// -----------------------------------------------------------------------------
	// Help query handling
	// -----------------------------------------------------------------------------

	TEST(LongHelpReturnsHelpQuery)
	{
		SimulatedArgv args{ "--help" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Help("Application help.");

		CHECK_EQ(parser.ParseAndValidate(), Parser::ArgParseResult::HELP_REQUESTED);
	}

	TEST(ShortHelpReturnsHelpQuery)
	{
		SimulatedArgv args{ "-h" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Help("Application help.");

		CHECK_EQ(parser.ParseAndValidate(), Parser::ArgParseResult::HELP_REQUESTED);
	}

	TEST(HelpQueryTakesPriorityOverMissingRequiredArguments)
	{
		SimulatedArgv args{ "--help" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--required").Required();

		CHECK_EQ(parser.ParseAndValidate(), Parser::ArgParseResult::HELP_REQUESTED);
	}

	TEST(HelpListsEveryArgumentKind)
	{
		//SimulatedArgv args{ "--help" };
		//Parser::ArgParser parser(args.argc(), args.argv());
		//parser.Help("Overview.");
		//parser.Add<std::string>("--output", { "-O_ALIAS" })
		//	.Help("Scalar documentation.");
		//parser.AddPositional<std::string>("INPUT").Help("Positional documentation.");
		//parser.AddAggregate<std::string>("--items")
		//	.Unlimited()
		//	.Help("Aggregate documentation.");
		//parser.AddFlag("--verbose", { "-V_ALIAS" }).Help("Flag documentation.");

		//ScopedStreamCapture stdout_capture(std::cout);
		//const Error error =
		//	parser.ParseAndValidate();
		//const std::string output = stdout_capture.str();

		//CHECK_EQ(parser.ParseAndValidate(), Parser::ArgParseResult::HELP_REQUESTED);
		//CHECK(output.find("Overview.") != std::string::npos);
		//CHECK(output.find("--output") != std::string::npos);
		//CHECK(output.find("-O_ALIAS") != std::string::npos);
		//CHECK(output.find("Scalar documentation.") != std::string::npos);
		//CHECK(output.find("INPUT") != std::string::npos);
		//CHECK(output.find("Positional documentation.") != std::string::npos);
		//CHECK(output.find("--items") != std::string::npos);
		//CHECK(output.find("Aggregate documentation.\n") != std::string::npos);
		//CHECK(output.find("--verbose") != std::string::npos);
		//CHECK(output.find("-V_ALIAS") != std::string::npos);
		//CHECK(output.find("Flag documentation.") != std::string::npos);
		throw std::exception("HelpListsEveryArgumentKind not implemented");
	}

	TEST(EmptyProgramHelpIsSafe)
	{
		SimulatedArgv args{ "--help" };
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(parser.Help(""), std::logic_error);
	}

	TEST(EmptyScalarHelpIsSafe)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& value = parser.Add<std::string>("--value");
		CHECK_THROWS_AS(value.Help(""), std::logic_error);
	}

	TEST(EmptyFlagHelpIsSafe)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& verbose = parser.AddFlag("--verbose");
		CHECK_THROWS_AS(verbose.Help(""),std::logic_error);
	}

	TEST(AttachedHelpLikeValueIsNotAHelpQuery)
	{
		SimulatedArgv args{ "--label=--help" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& label = parser.Add<std::string>("--label");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(label.Value(), std::string("--help"));
	}

	TEST(HelpTokenAfterDoubleDashRemainsPositional)
	{
		SimulatedArgv args{ "--", "--help" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& value = parser.AddPositional<std::string>("value").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(value.Value(), std::string("--help"));
	}


	// -----------------------------------------------------------------------------
	// Structured diagnostics and formatted error reporting
	// -----------------------------------------------------------------------------

	TEST(MissingValueDiagnosticUsesSuppliedAlias)
	{
		SimulatedArgv args{ "-c" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::int64_t>("--count", { "-c" });

		CheckReportedError(
			parser,
			Error::MISSING_VALUE,
			"-c",
			"",
			"",
			"Error: argument '-c' requires a value.");
	}

	TEST(MissingRequiredDiagnostic)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--path").Required();

		CheckReportedError(
			parser,
			Error::MISSING_REQUIRED,
			"--path",
			"",
			"",
			"Error: required argument '--path' was not provided.");
	}

	TEST(ParseFailureDiagnosticPreservesSuppliedAliasAndReason)
	{
		SimulatedArgv args{ "-c", "not-a-number" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::int64_t>("--count", { "-c" });

		CheckReportedError(
			parser,
			Error::PARSE_FAIL,
			"-c",
			"not-a-number",
			"expected an integer",
			"Error: failed to parse value 'not-a-number' for argument '-c': "
			"expected an integer.");
	}

	TEST(Int8ParseFailurePreservesInvalidTextReason)
	{
		std::int8_t value{};
		const Parser::Result result = Parser::Parse("not-a-number", value);

		CHECK_FALSE(result);
		CHECK_EQ(result.error_message, std::string("expected an integer"));
	}

	TEST(Uint8ParseFailurePreservesRangeReason)
	{
		std::uint8_t value{};
		const Parser::Result result = Parser::Parse("256", value);

		CHECK_FALSE(result);
		CHECK_EQ(result.error_message, std::string("integer result out of range"));
	}

	TEST(TextValidationDiagnosticIncludesCallbackDetail)
	{
		SimulatedArgv args{ "--name", "abc123" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--name").ValidateText(
			[](std::string_view)
			{
				return Parser::Result::Failure("letters only");
			});

		CheckReportedError(
			parser,
			Error::TEXT_VALIDATION_INVALID,
			"--name",
			"abc123",
			"letters only",
			"Error: invalid text value 'abc123' for argument '--name': letters only.");
	}

	TEST(TransformationDiagnosticIncludesCallbackDetail)
	{
		SimulatedArgv args{ "--name", "anything" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--name").Transform(
			[](std::string&)
			{
				return Parser::Result::Failure("normalization rejected input");
			});

		CheckReportedError(
			parser,
			Error::TRANSFORMATION_ERROR,
			"--name",
			"anything",
			"normalization rejected input",
			"Error: could not transform value 'anything' for argument '--name': "
			"normalization rejected input.");
	}

	TEST(ValueValidationDiagnosticDoesNotDuplicateFinalPeriod)
	{
		SimulatedArgv args{ "--scale", "-0.5" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<double>("--scale").ValidateValue(
			[](const double&)
			{
				return Parser::Result::Failure("scale must be positive.");
			});

		CheckReportedError(
			parser,
			Error::VAL_VALIDATION_INVALID,
			"--scale",
			"-0.5",
			"scale must be positive.",
			"Error: invalid value '-0.5' for argument '--scale': "
			"scale must be positive.");
	}

	TEST(CustomDiagnosticPreservesExclamationMarkWithoutAddingPeriod)
	{
		SimulatedArgv args{ "--name", "bad" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--name").ValidateText(
			[](std::string_view)
			{
				return Parser::Result::Failure("name is invalid!");
			});

		CheckReportedError(
			parser,
			Error::TEXT_VALIDATION_INVALID,
			"--name",
			"bad",
			"name is invalid!",
			"Error: invalid text value 'bad' for argument '--name': name is invalid!");
	}

	TEST(UnknownEqualsArgumentDiagnosticIntentionallyReportsOnlyName)
	{
		SimulatedArgv args{ "--does-not-exist=payload" };
		Parser::ArgParser parser(args.argc(), args.argv());

		CheckReportedError(
			parser,
			Error::UNKNOWN_ARGUMENT,
			"--does-not-exist",
			"",
			"",
			"Error: unknown argument '--does-not-exist'.");
	}

	TEST(FlagValueDiagnosticUsesSuppliedAlias)
	{
		SimulatedArgv args{ "-v=yes" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddFlag("--verbose", { "-v" });

		CheckReportedError(
			parser,
			Error::FLAG_HAS_EQUALS_VALUE,
			"-v",
			"",
			"",
			"Error: flag '-v' does not accept a value.");
	}

	TEST(ExactCardinalityDiagnosticContainsStructuredCounts)
	{
		SimulatedArgv args{ "--items", "1", "2" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items").Exactly(3);

		CheckReportedError(
			parser,
			Error::CARDINALITY_VALIDATION_FAIL,
			"--items",
			"",
			"",
			"Error: argument '--items' expects exactly 3 values, but received 2.");

		const Parser::Diagnostic::Aggregate& aggregate =
			parser.GetDiagnostics().aggregate;
		CHECK_EQ(aggregate.card, Parser::Cardinality::EXACTLY);
		CHECK_EQ(aggregate.count, std::size_t{ 3 });
		CHECK_EQ(aggregate.received_count, std::size_t{ 2 });
	}

	TEST(BetweenCardinalityDiagnosticContainsStructuredBounds)
	{
		SimulatedArgv args{ "--items", "1", "2", "3", "4" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items").Between(1, 3);

		CheckReportedError(
			parser,
			Error::CARDINALITY_VALIDATION_FAIL,
			"--items",
			"",
			"",
			"Error: argument '--items' expects between 1 and 3 values, but "
			"received 4.");

		const Parser::Diagnostic::Aggregate& aggregate =
			parser.GetDiagnostics().aggregate;
		CHECK_EQ(aggregate.card, Parser::Cardinality::BETWEEN);
		CHECK_EQ(aggregate.min, std::size_t{ 1 });
		CHECK_EQ(aggregate.max, std::size_t{ 3 });
		CHECK_EQ(aggregate.received_count, std::size_t{ 4 });
	}

	TEST(AggregateValueValidationDiagnosticIdentifiesElement)
	{
		SimulatedArgv args{ "--items", "1", "-2", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items")
			.Exactly(3)
			.ValidateValue(
				[](const std::int64_t& value)
				{
					return value > 0
						? Parser::Result::Success()
						: Parser::Result::Failure("item must be positive");
				});

		CheckReportedError(
			parser,
			Error::VAL_VALIDATION_INVALID,
			"--items",
			"-2",
			"item must be positive",
			"Error: invalid value '-2' for argument '--items': item must be positive.");
	}

	TEST(AggregateDefaultStillRunsPerValueValidation)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items")
			.Exactly(2)
			.Default({ 1, -2 })
			.ValidateValue(
				[](const std::int64_t& value)
				{
					return value > 0
						? Parser::Result::Success()
						: Parser::Result::Failure(
							"default values must be positive");
				});

		CheckReportedError(
			parser,
			Error::VAL_VALIDATION_INVALID,
			"--items",
			"",
			"default values must be positive",
			"Error: invalid value for argument '--items': "
			"default values must be positive.");
	}

	TEST(CollectionValidationDiagnosticUsesCallbackDetail)
	{
		SimulatedArgv args{ "--items", "1", "2", "3" };
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddAggregate<std::int64_t>("--items")
			.Exactly(3)
			.ValidateCollection(
				[](const std::vector<std::int64_t>& values)
				{
					if (values[0] + values[1] + values[2] > 5)
					{
						return Parser::Result::Failure(
							"value at index 2 makes the total too large");
					}
					return Parser::Result::Success();
				});

		CheckReportedError(
			parser,
			Error::COL_VALIDATION_INVALID,
			"--items",
			"",
			"value at index 2 makes the total too large",
			"Error: invalid collection for argument '--items': "
			"value at index 2 makes the total too large.");
	}

	// -----------------------------------------------------------------------------
	// Invalid parser configurations, reported by exceptions
	// -----------------------------------------------------------------------------

	TEST(RequiredScalarCannotHaveDefault)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(parser.Add<std::int64_t>("--count").Required().Default(5), std::logic_error);
	}

	TEST(RequiredAggregateCannotHaveDefault)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(
			(parser.AddAggregate<std::int64_t>("--items")
				.Exactly(2)
				.Required()
				.Default({ 1, 2 })),
			std::logic_error);
	}

	TEST(ScalarDefaultCannotBeSetTwice)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(parser.Add<std::int64_t>("--count").Default(1).Default(2), std::logic_error);
	}

	TEST(AggregateDefaultCannotBeSetTwice)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(
			(parser.AddAggregate<std::int64_t>("--items")
				.Exactly(2)
				.Default({ 1, 2 })
				.Default({ 3, 4 })),
			std::logic_error);
	}

	TEST(RequiredPositionalCannotFollowOptionalPositional)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddPositional<std::string>("optional").Default("default");
		parser.AddPositional<std::string>("required").Required();

		CHECK_THROWS_AS(parser.ParseAndValidate(), std::logic_error);
	}

	TEST(DuplicateCanonicalName)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--name");
		CHECK_THROWS_AS(parser.Add<std::int64_t>("--name"), std::logic_error);
	}

	TEST(AliasCollidesWithCanonicalName)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--first", { "-f" });
		CHECK_THROWS_AS(parser.Add<std::string>("-f"), std::logic_error);
	}

	TEST(AliasCollidesWithAnotherAlias)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.Add<std::string>("--first", { "-x" });
		CHECK_THROWS_AS((parser.Add<std::string>("--second", { "-x" })), std::logic_error);
	}

	TEST(DuplicateAliasWithinOneRegistrationIsRejected)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS((parser.Add<std::string>("--value", { "-v", "-v" })), std::logic_error);
	}

	TEST(EmptyNamedArgumentIsRejected)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(parser.Add<std::string>(""), std::logic_error);
	}

	TEST(BuiltInHelpNameIsReserved)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(parser.AddFlag("--help"), std::logic_error);
	}

	TEST(FlagNameCollidesWithArgumentName)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddFlag("--debug");
		CHECK_THROWS_AS(parser.Add<std::string>("--debug"), std::logic_error);
	}

	TEST(FlagAliasCollidesWithArgumentName)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		parser.AddFlag("--flag", {"--debug"});
		CHECK_THROWS_AS(parser.Add<std::string>("--debug"), std::logic_error);
	}

	TEST(CardinalityAlreadySetError)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(parser.AddAggregate<std::int64_t>("--items").Exactly(3).Unlimited(), std::logic_error);
	}

	TEST(MinimumGreaterThanMaximumIsCardinalityError)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		CHECK_THROWS_AS(parser.AddAggregate<std::int64_t>("--items").Between(5, 4), std::logic_error);
	}

	TEST(ValueOr)
	{
		SimulatedArgv args{"--float", "9.7"};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& c = parser.Add<int32_t>("--count");
		auto& f = parser.Add<double>("--float");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(c.ValueOr(5), 5);
		CHECK_EQ(f.ValueOr(7.8), 9.7);
	}

	TEST(ScalarValueOrUsesConfiguredDefault)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& value = parser.Add<std::int64_t>("--value").Default(17);

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(value.ValueOr(99), std::int64_t{ 17 });
	}

	TEST(AggregateValueOrUsesConfiguredDefault)
	{
		SimulatedArgv args{};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& values = parser.AddAggregate<std::int64_t>("--values")
			.Exactly(2)
			.Default({ 7, 8 });

		const std::vector<std::int64_t> backup{ 99, 100 };

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(values.ValueOr(backup), std::vector<std::int64_t>({ 7, 8 }));
	}

	TEST(ValueOrUsesBackupAfterParseFailure)
	{
		SimulatedArgv args{ "--value", "invalid" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& value = parser.Add<std::int64_t>("--value");

		CHECK_ERROR(parser, Error::PARSE_FAIL);
		CHECK_EQ(value.ValueOr(42), std::int64_t{ 42 });
	}

	template <typename T>
	concept ConstAggregateValueOr = Parser::Parseable<T> && requires(
		const Parser::Aggregate<T>& values, const std::vector<T>& backup)
	{
		values.ValueOr(backup);
	};

	TEST(AggregateValueOrCanBeCalledOnConstObject)
	{
		CHECK(ConstAggregateValueOr<std::int64_t>);
	}

	TEST(MissingValueForArgument)
	{
		SimulatedArgv args{ "--count", "--float", "9.7" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& c = parser.Add<int32_t>("--count");
		auto& f = parser.Add<double>("--float");

		CHECK_ERROR(parser, Error::MISSING_VALUE);
	}

	TEST(DoubleDashNoOptionsOnlyPositional)
	{
		SimulatedArgv args{ "--","hello", "9" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& p1 = parser.AddPositional<std::string>("txt");
		auto& p2 = parser.AddPositional<int32_t>("num");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(p1.Value(), "hello");
		CHECK_EQ(p2.Value(), int32_t(9));
	}

	TEST(OptionsAfterDoubleDash)
	{
		SimulatedArgv args{ "--", "--count", "9.7" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& c = parser.Add<int32_t>("--count");
		auto& p = parser.AddPositional<std::string>("c");
		auto& p2 = parser.AddPositional<double>("num");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(p.Value(), "--count");
		CHECK_EQ(p2.Value(), 9.7);
	}

	TEST(EqualsAfterDoubleDashRemainsOnePositionalToken)
	{
		SimulatedArgv args{ "--", "key=value" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& value = parser.AddPositional<std::string>("value").Required();

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(value.Value(), std::string("key=value"));
	}

	TEST(NothingAfterDoubleDash)
	{
		SimulatedArgv args{ "--float", "9.7", "--"};
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& f = parser.Add<double>("--float");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(f.Value(), 9.7);
	}


	TEST(UnlimitedAggragateStopAtDoubleDash)
	{
		SimulatedArgv args{ "--float", "9.7", "9.7","9.7","9.7", "--", "hello" };
		Parser::ArgParser parser(args.argc(), args.argv());
		auto& aggr = parser.AddAggregate<double>("--float").Unlimited();
		auto& pos = parser.AddPositional<std::string>("h");

		CHECK_ERROR(parser, Error::SUCCESS);
		CHECK_EQ(pos.Value(), "hello");
	}

	// -----------------------------------------------------------------------------
	// Full example-style integration test
	// -----------------------------------------------------------------------------

	TEST(FullExampleStyleCommandLine)
	{
		SimulatedArgv args{
			"--path", "SomeFile.TXT",
			"--output", "RESULT.TXT",
			"--count", "7",
			"--scale", "2.5",
			"-d",
			"HELLO",
			"-12",
			"--floats", "1.5", "2.0", "3.25", "4.0"
		};

		Parser::ArgParser parser(args.argc(), args.argv());

		auto lowercase = [](std::string& value)
			{
				std::transform(
					value.begin(), value.end(), value.begin(), [](unsigned char c)
					{
						return static_cast<char>(std::tolower(c));
					});
				return true;
			};

		auto no_numbers = [](std::string_view value)
			{
				return std::none_of(
					value.begin(), value.end(), [](unsigned char c)
					{
						return std::isdigit(c) != 0;
					});
			};

		auto non_negative_text = [](std::string_view value)
			{
				return value.empty() || value.front() != '-';
			};

		auto positive_double = [](const double& value)
			{
				return value > 0.0;
			};

		auto& path = parser.Add<std::string>("--path", { "-p", "--filepath" })
			.Required()
			.ValidateText(no_numbers)
			.Transform(lowercase)
			.Help("Path to the input file.");

		auto& output = parser.Add<std::string>("--output", { "-o" })
			.Default("output.txt")
			.Transform(lowercase);

		auto& count = parser.Add<std::int64_t>("--count", { "-c" })
			.Default(5)
			.ValidateText(non_negative_text);

		auto& scale = parser.Add<double>("--scale", { "-s" })
			.Default(1.0)
			.ValidateValue(positive_double);

		auto& debug = parser.AddFlag("--debug", { "-d" });
		auto& verbose = parser.AddFlag("--verbose", { "-v" });

		auto& positional_text = parser.AddPositional<std::string>("text")
			.Required()
			.Transform(lowercase)
			.ValidateText(no_numbers);

		auto& positional_num = parser.AddPositional<std::int64_t>("num")
			.Default(66);

		auto& floats = parser.AddAggregate<double>("--floats")
			.ValidateValue(positive_double)
			.Required()
			.AtLeast(3);

		CHECK_ERROR(parser, Error::SUCCESS);

		CHECK_EQ(path.Value(), std::string("somefile.txt"));
		CHECK(path.Provided());

		CHECK_EQ(output.Value(), std::string("result.txt"));
		CHECK(output.Provided());

		CHECK_EQ(count.Value(), std::int64_t{ 7 });
		CHECK(count.Provided());

		CHECK_NEAR(scale.Value(), 2.5, 0.000001);
		CHECK(scale.Provided());

		CHECK(debug.Value());
		CHECK_FALSE(verbose.Value());

		CHECK_EQ(positional_text.Value(), std::string("hello"));
		CHECK(positional_text.Provided());

		CHECK_EQ(positional_num.Value(), std::int64_t{ -12 });
		CHECK(positional_num.Provided());

		CHECK(floats.Provided());
		CHECK_EQ(floats.Value().size(), std::size_t{ 4 });
		CHECK_NEAR(floats.Value()[0], 1.5, 0.000001);
		CHECK_NEAR(floats.Value()[1], 2.0, 0.000001);
		CHECK_NEAR(floats.Value()[2], 3.25, 0.000001);
		CHECK_NEAR(floats.Value()[3], 4.0, 0.000001);
	}
} // namespace parser_tests

int main()
{
	std::size_t passed = 0;
	std::size_t failed = 0;
	std::size_t skipped = 0;

	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	for (const parser_tests::TestCase& test : parser_tests::Registry())
	{
		try
		{
			test.function();
			++passed;
			//std::cout << "[PASS] " << test.name << '\n';
		}
		catch (const parser_tests::TestSkipped& error)
		{
			++skipped;
			std::cout << "[SKIP] " << test.name << ": " << error.what() << '\n';
		}
		catch (const std::exception& error)
		{
			++failed;
			std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
		}
		catch (...)
		{
			++failed;
			std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
		}
	}

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::chrono::milliseconds dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration(end - start));

	std::cout << "\nSummary: " << passed << " passed, " << failed << " failed, "
		<< skipped << " skipped, "
		<< parser_tests::Registry().size() << " total\n";

	std::cout << "Tests run in " << dur << '\n';

	return failed == 0 ? 0 : 1;
}
