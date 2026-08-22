# ArgParser : A C++20 command-line parsing library

ArgParser is a modern command-line parsing library with typed values, positional arguments, flags, repeated-value aggregates, validation, transformation, structured diagnostics, and generated help.

The library is configured through a fluent API and parses directly from `argc` and `argv`. It has no external runtime dependencies.

## Features

- Typed named and positional arguments
- Boolean flags
- Named and positional aggregates (`std::vector<T>`)
- Canonical names and aliases
- Required arguments and typed defaults
- `--name value` and `--name=value` syntax
- The `--` end-of-options marker
- Built-in `--help` and `-h` handling
- Delegated subcommand parsing through stop tokens
- Text transformations
- Text, typed-value, and collection validation
- Aggregate cardinality constraints
- Custom types through a `Parse(std::string_view, T&)` function
- Structured diagnostics and formatted error messages
- Wrapped, configurable help output

## Requirements and integration

ArgParser requires C++20. Add `ArgParser.h` and `ArgParser.cpp` to your project, compile `ArgParser.cpp`, and include the header where the parser is used. A header only option is in the works.

## Quick start

```cpp
#include "ArgParser.h"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    Parser::ArgParser parser(argc, argv);
    parser.Help("Convert an input file and write the result.");

    auto& input = parser
        .AddPositional<std::string>("input")
        .Required()
        .Meta("FILE")
        .Help("File to process.");

    auto& output = parser
        .Add<std::string>("--output", { "-o" })
        .Default("output.txt")
        .Meta("FILE")
        .Help("Destination file.");

    auto& jobs = parser
        .Add<int>("--jobs", { "-j" })
        .Default(1)
        .Meta("COUNT")
        .ValidateValue([](const int& value)
        {
            if (value < 1)
                return Parser::Result::Failure("must be at least 1");

            return Parser::Result::Success();
        })
        .Help("Number of parallel jobs.");

    auto& verbose = parser
        .AddFlag("--verbose", { "-v" })
        .Help("Enable verbose output.");

    switch (parser.ParseAndValidate())
    {
    case Parser::ArgParseResult::HELP_REQUESTED:
        std::cout << parser.GetHelpMessage();
        return 0;

    case Parser::ArgParseResult::ERROR:
        std::cerr << parser.GetErrorMessage() << '\n';
        return 2;

    case Parser::ArgParseResult::SUCCESS:
        break;
    }

    std::cout
        << "input: " << input.Value() << '\n'
        << "output: " << output.Value() << '\n'
        << "jobs: " << jobs.Value() << '\n'
        << "verbose: " << std::boolalpha << verbose.Value() << '\n';
}
```

Example invocations:

```text
app input.txt
app input.txt --output=result.txt --jobs 4 --verbose
app input.txt -o result.txt -j 4 -v
app --help
```

## Argument types

### Named scalar arguments

Use `Add<T>()` for an option that accepts one value:

```cpp
auto& count = parser.Add<int>("--count", { "-c" });
```

Both separated and equals syntax are supported:

```text
--count 5
--count=5
```

An equals sign explicitly supplies everything after it as the value. An empty equals value is therefore valid syntax:

```text
--name=
```

Whether an empty value is accepted depends on `T`, transformations, and validators. `std::string` accepts it; the built-in numeric parsers reject it.

If a scalar appears more than once, the last occurrence wins. Only the retained value is transformed, parsed, and validated:

```text
--count 2 --count 5
```

In this example, the resulting value is `5`.

### Flags

Use `AddFlag()` for a Boolean switch:

```cpp
auto& verbose = parser.AddFlag("--verbose", { "-v" });
```

The value is `false` when omitted and `true` when present. Flags are idempotent: repeating a flag still produces `true` rather than a count.

```text
--verbose --verbose
```

Flags do not accept equals values, including an empty one. `--verbose=true` and `--verbose=` are errors.

### Named aggregates

Use `AddAggregate<T>()` to collect one or more values into a `std::vector<T>`:

```cpp
auto& includes = parser
    .AddAggregate<std::string>("--include", { "-I" })
    .Help("Directories to search.");
```

```text
--include src include generated
```

Named aggregates are greedy. They consume values until one of the following is encountered:

- another registered argument name, alias, or a stop token;
- the `--` end-of-options marker;
- the end of the command line.

Aggregates may be repeated, and later occurrences append values:

```text
--include src include --include generated
```

Cardinality is validation, not token-consumption control. For example, `Exactly(2)` verifies the final collection size but does not make the parser stop consuming after two values. 

### Positional arguments

Use `AddPositional<T>()` for a single positional value:

```cpp
auto& input = parser
    .AddPositional<std::string>("input")
    .Required();
```

Positional arguments are filled in declaration order. Required positionals must be declared before optional positionals.

The helper name must not be empty. It is used in help and diagnostics and is not matched against command-line tokens.

### Positional aggregates

Use `AddPositionalAggregate<T>()` to collect all remaining positional values:

```cpp
auto& files = parser.AddPositionalAggregate<std::string>("files");
```

Only one positional aggregate may be registered. It logically follows every scalar positional, regardless of when it was declared. If it is required, no optional scalar positional may precede it.

## Subcommands

ArgParser supports delegated subcommand parsing through stop tokens. Register each subcommand name with `StopAt()`:

```cpp
Parser::ArgParser parser(argc, argv);
parser.Help("Manage projects.");
parser.StopAt({ "add", "remove" });
```

When parsing encounters a registered stop token before `--`, it validates the arguments owned by the current parser and returns `ArgParseResult::STOP_TOKEN`. `GetStopResult()` identifies the token and provides the remaining command-line tokens to another parser:

```cpp
switch (parser.ParseAndValidate())
{
case Parser::ArgParseResult::HELP_REQUESTED:
    std::cout << parser.GetHelpMessage();
    return 0;

case Parser::ArgParseResult::ERROR:
    std::cerr << parser.GetErrorMessage() << '\n';
    return 2;

case Parser::ArgParseResult::SUCCESS:
    // No subcommand was supplied.
    return 0;

case Parser::ArgParseResult::STOP_TOKEN:
    break;
}

const Parser::StopResultView& stop = parser.GetStopResult();

if (stop.stop_token == "add")
{
    Parser::ArgParser add_parser(stop);
    add_parser.Help("Add a project.");

    auto& name = add_parser
        .AddPositional<std::string>("name")
        .Required();

    // Parse and handle add_parser here.
}
```

`StopResultView` is non-owning and remains valid only while its source parser exists. The delegated-parser constructor copies the remaining tokens and command name, so the delegated parser is independent once construction finishes.

`STOP_TOKEN` is a successful validation result for the current parser. Its registered argument and flag values follow the normal accessor rules.

The same pattern can be repeated for nested subcommands. Delegated parsers retain the complete invocation name, so help for a nested parser uses names such as `app project add`. Parent help lists registered stop tokens under `Subcommands` and tells users to run `app subcommand --help` for command-specific help.

Stop tokens must be nonempty, unique, free of `=`, and must not conflict with registered names, aliases, `--help`, `-h`, or `--`. Equals syntax is not valid for a stop token. For example, `add=value` produces `Error::STOP_TOKEN_HAS_EQUALS_VALUE` when `add` is registered as one.

## Configuring arguments

Scalar arguments and aggregates support the following fluent configuration methods:

| Method | Meaning |
| --- | --- |
| `Required()` | The argument must be supplied. |
| `Default(value)` | Use a typed default when the argument is omitted. |
| `Help(text)` | Set the argument description shown in help. |
| `Meta(name)` | Set the value placeholder shown in help. |
| `Transform(callback)` | Transform supplied text before parsing. |
| `ValidateText(callback)` | Validate transformed text before parsing. |
| `ValidateValue(callback)` | Validate the parsed value. |

Aggregates additionally support `ValidateCollection()` and cardinality constraints.

`Required()` and `Default()` are mutually exclusive. Configuration methods throw `std::logic_error` when combined incorrectly or called after `ParseAndValidate()`.

Defaults are already typed values. Text transformations, text validators, and `Parse()` are therefore not applied to defaults. Typed validators still run. Aggregate defaults also undergo cardinality and collection validation.

Generated help displays a default when ArgParser can convert it to text with `StringDefault()`. Built-in numeric and string types provide this conversion. An empty result is shown as `<empty>`.

### Aggregate cardinality

```cpp
auto& coordinates = parser
    .AddAggregate<double>("--point")
    .Exactly(2);
```

Available constraints:

| Method | Requirement when the aggregate is present or defaulted |
| --- | --- |
| `Exactly(n)` | Exactly `n` values |
| `Between(min, max)` | Between `min` and `max`, inclusive |
| `AtLeast(n)` | At least `n` values |
| `AtMost(n)` | At most `n` values |
| `Unlimited()` | No cardinality limit |

Only one cardinality method may be selected. Counts must be greater than zero. An optional aggregate that is omitted and has no default skips cardinality validation.

Generated help shows each aggregate's cardinality as `unlimited`, `exactly n`, `between min and max`, `at least n`, or `at most n`.

## Value access and parser lifecycle

`ArgParser` is single-use. Register and configure every argument before calling `ParseAndValidate()`. Calling it more than once, or attempting parser/argument configuration afterward, throws `std::logic_error`.

`Value()`, `ValueRef()`, and `ValueOr()` must not be called before parsing. After `ERROR` or `HELP_REQUESTED`, direct value access throws and `ValueOr()` returns its caller-supplied backup.

### Scalar access

| State after parsing | `Provided()` | `Value()` / `ValueRef()` | `ValueOr(backup)` |
| --- | ---: | --- | --- |
| Supplied and valid | `true` | Parsed value | Parsed value |
| Omitted with a valid default | `false` | Default | Default |
| Omitted without a default | `false` | Throws | `backup` |
| Overall parse error or help request | Whether encountered before parsing stopped | Throws | `backup` |

### Aggregate access

| State after parsing | `Provided()` | `Value()` / `ValueRef()` | `ValueOr(backup)` |
| --- | ---: | --- | --- |
| Supplied and valid | `true` | Parsed collection | Parsed collection |
| Omitted with a valid default | `false` | Default collection | Default collection |
| Omitted without a default | `false` | Empty collection | `backup` |
| Overall parse error or help request | Whether encountered before parsing stopped | Throws | `backup` |

`Provided()` requires the argument to be locked, which occurs when `ParseAndValidate()` returns normally. It does not require successful parsing and does not validate the value. Before parsing it throws `std::logic_error`.

Flags expose `Value()` rather than `Provided()`. `Value()` returns `false` or `true` after successful parsing and throws before parsing, after an error, or after a help request.

## Transformation and validation

Callbacks return `Parser::Result`. A failure may include a message that is incorporated into the formatted diagnostic.

### Text transformations

Transformations receive mutable text and run in registration order:

```cpp
auto& mode = parser
    .Add<std::string>("--mode")
    .Transform([](std::string& text)
    {
        for (char& ch : text)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

        return Parser::Result::Success();
    });
```

The example requires `<cctype>`.

A failed transformation produces `Error::TRANSFORMATION_ERROR`. A transformation's changes are committed only when that transformation succeeds.

### Text validation

Text validators run after all transformations and before typed parsing:

```cpp
auto& code = parser
    .Add<std::string>("--code")
    .ValidateText([](std::string_view text)
    {
        if (text.size() != 3)
            return Parser::Result::Failure("must contain exactly 3 characters");

        return Parser::Result::Success();
    });
```

A failure produces `Error::TEXT_VALIDATION_INVALID`.

### Typed-value validation

Value validators receive the parsed value:

```cpp
auto& percentage = parser
    .Add<int>("--percentage")
    .ValidateValue([](const int& value)
    {
        if (value < 0 || value > 100)
            return Parser::Result::Failure("must be between 0 and 100");

        return Parser::Result::Success();
    });
```

A failure produces `Error::VAL_VALIDATION_INVALID`.

### Collection validation

Aggregate collection validators receive the completed parsed vector:

```cpp
auto& values = parser
    .AddAggregate<int>("--values")
    .ValidateCollection([](const std::vector<int>& collection)
    {
        if (collection.front() >= collection.back())
            return Parser::Result::Failure("values must be in ascending order");

        return Parser::Result::Success();
    });
```

A failure produces `Error::COL_VALIDATION_INVALID`.

For supplied scalar values, processing order is:

1. All transformations, in registration order
2. All text validators, in registration order
3. `Parse()` into `T`
4. All typed-value validators, in registration order

For aggregates, steps 1–3 run for every element. Cardinality is then checked, followed by element value validators and collection validators.

Callbacks and custom parsers are expected to report ordinary input failures with `Parser::Result::Failure()`. Exceptions propagate and represent terminal programming or configuration failures; do not continue using the parser after an exception escapes.

## Supported value types

Built-in `Parse()` overloads are provided for:

- `int8_t`, `int16_t`, `int32_t`, and `int64_t`
- `uint8_t`, `uint16_t`, `uint32_t`, and `uint64_t`
- `float`, `double`, and `long double`
- `std::string`

Integer input is decimal and must consume the complete value. Floating-point input uses general format and must also consume the complete value. Range errors are reported as parse failures.

`bool` is intentionally not `Parseable`; use `AddFlag()` for Boolean switches.

## Custom value types

A custom type must be default-constructible, copy-constructible, and copy-assignable. Define a function with this signature in the same namespace as the custom type:

```cpp
Parser::Result Parse(std::string_view text, MyType& output);
```

Placing `Parse()` beside the type allows argument-dependent lookup (ADL) to find it.

```cpp
#include "ArgParser.h"

#include <cstdint>
#include <string_view>

namespace App
{
    struct Port
    {
        std::uint16_t value = 0;
    };

    Parser::Result Parse(std::string_view text, Port& output)
    {
        std::uint16_t parsed = 0;
        Parser::Result result = Parser::Parse(text, parsed);

        if (!result)
            return result;

        output.value = parsed;
        return Parser::Result::Success();
    }
}
```

The type can then be registered normally:

```cpp
auto& port = parser.Add<App::Port>("--port");
```

To display typed defaults for a custom type in generated help, define `StringDefault()` in the same namespace:

```cpp
namespace App
{
    std::string StringDefault(const Port& value)
    {
        return std::to_string(value.value);
    }
}
```

Argument-dependent lookup finds this function in the same way as `Parse()`. If a custom type has no `StringDefault()` function, its default is still applied and validated but omitted from generated help. If the function returns an empty string, help displays `<empty>`.

## Names, aliases, and token boundaries

ArgParser does not require registered names to begin with `-`. These are all structurally valid names:

```text
output
-o
--output
```

Names and aliases must be nonempty, unique, and must not contain `=`. The following tokens are reserved:

- `--help`
- `-h`
- `--`

After `--`, name recognition and built-in help recognition stop. Remaining tokens are assigned only to positional arguments.

Because names are unrestricted, the parser cannot infer that every unfamiliar token beginning with `-` was intended to be an option. An unknown token may be consumed as a scalar value, aggregate value, or positional value when one of those destinations is active. `Error::UNKNOWN_ARGUMENT` means that no registered name or available positional destination accepted the token.

A registered argument name or stop token cannot be supplied as a separate scalar/aggregate value because it acts as a token boundary. Users can use equals syntax to supply such a literal value:

```text
--label=--help
```

## Help

`--help` and `-h` are built in and cause `ParseAndValidate()` to return `ArgParseResult::HELP_REQUESTED` when encountered before `--`.

Set the application description with `ArgParser::Help()` and argument descriptions with each argument's `Help()` method:

```cpp
parser.Help("Archive one or more files.");

parser
    .Add<std::string>("--output", { "-o" })
    .Meta("FILE")
    .Help("Write the archive to FILE.");
```

`GetHelpMessage()` may be called at any time. It describes the arguments registered at the moment of the call:

```cpp
std::cout << parser.GetHelpMessage();
std::cout << parser.GetHelpMessage(100, 36);
```

The first width controls the target line width. The second caps the option-label column width. Long individual words and the executable name may exceed the requested line width.

Current help output includes usage, the application description, positional arguments, subcommands, named arguments, aliases, meta-variable names, required markers, displayable defaults, aggregate cardinality, and built-in help.

## Results and diagnostics

`ParseAndValidate()` returns:

| Result | Meaning |
| --- | --- |
| `ArgParseResult::SUCCESS` | Every supplied/defaulted value parsed and validated. |
| `ArgParseResult::ERROR` | A command-line input error occurred. |
| `ArgParseResult::HELP_REQUESTED` | `--help` or `-h` was encountered before `--`. |
| `ArgParseResult::STOP_TOKEN` | A registered stop token was encountered and the current parser's arguments validated successfully. |

After `ERROR`, use `GetErrorMessage()` for human-readable output or `GetDiagnostics()` for structured information:

```cpp
if (parser.ParseAndValidate() == Parser::ArgParseResult::ERROR)
{
    const Parser::Diagnostic& diagnostic = parser.GetDiagnostics();

    std::cerr << parser.GetErrorMessage() << '\n';
    return diagnostic.ec == Parser::Error::UNKNOWN_ARGUMENT ? 2 : 1;
}
```

`Diagnostic` contains:

| Field | Meaning |
| --- | --- |
| `ec` | Error category |
| `arg_name` | Argument or positional helper name associated with the error |
| `opt_token` | Input token associated with parsing/validation failure, when available |
| `opt_error_message` | Message returned by a parser, transformation, or validator |
| `positional` | Whether finalization failed for a positional argument |
| `aggregate` | Expected and received cardinality details |

Error categories are:

- `SUCCESS`
- `PARSE_FAIL`
- `MISSING_REQUIRED`
- `TEXT_VALIDATION_INVALID`
- `VAL_VALIDATION_INVALID`
- `COL_VALIDATION_INVALID`
- `TRANSFORMATION_ERROR`
- `MISSING_VALUE`
- `UNKNOWN_ARGUMENT`
- `CARDINALITY_VALIDATION_FAIL`
- `FLAG_HAS_EQUALS_VALUE`
- `STOP_TOKEN_HAS_EQUALS_VALUE`

Formatted error wording is intended for people. Prefer `Diagnostic` and `Error` when program logic needs to distinguish failures.

## Configuration errors

Invalid parser configuration throws `std::logic_error`. Examples include:

- duplicate or reserved names/aliases;
- names or aliases containing `=`;
- empty, conflicting, or `=`-containing stop tokens;
- an empty positional helper name;
- combining `Required()` and `Default()`;
- configuring cardinality more than once;
- zero or inverted cardinality bounds;
- registering two positional aggregates;
- placing a required positional after an optional positional;
- configuring or parsing again after `ParseAndValidate()`.

These exceptions indicate programming errors rather than recoverable command-line input errors.

## Roadmap

Currently perceived to be feature complete.
