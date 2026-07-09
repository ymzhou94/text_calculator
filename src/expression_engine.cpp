#include "expression_engine.h"

#include <cmath>
#include <cwchar>
#include <cwctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace textcalc {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kE = 2.718281828459045235360287471352662498;
constexpr double kZeroTolerance = 1e-15;

std::wstring Trim(std::wstring_view text) {
    size_t first = 0;
    while (first < text.size() && std::iswspace(text[first])) {
        ++first;
    }

    size_t last = text.size();
    while (last > first && std::iswspace(text[last - 1])) {
        --last;
    }

    return std::wstring(text.substr(first, last - first));
}

std::wstring NarrowToWide(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

std::wstring FormatNumber(double value) {
    if (std::abs(value) < kZeroTolerance) {
        value = 0.0;
    }

    std::ostringstream stream;
    stream << std::setprecision(12) << value;
    std::string text = stream.str();

    const size_t exponent = text.find_first_of("eE");
    if (exponent == std::string::npos) {
        const size_t decimal = text.find('.');
        if (decimal != std::string::npos) {
            while (!text.empty() && text.back() == '0') {
                text.pop_back();
            }
            if (!text.empty() && text.back() == '.') {
                text.pop_back();
            }
        }
    }

    if (text.empty() || text == "-0") {
        text = "0";
    }

    return NarrowToWide(text);
}

std::wstring FormatScientificNumber(double value) {
    if (std::abs(value) < kZeroTolerance) {
        value = 0.0;
    }

    std::ostringstream stream;
    stream << std::scientific << std::setprecision(12) << value;
    std::string text = stream.str();

    const size_t exponent = text.find_first_of("eE");
    if (exponent == std::string::npos) {
        return NarrowToWide(text);
    }

    std::string mantissa = text.substr(0, exponent);
    while (!mantissa.empty() && mantissa.back() == '0') {
        mantissa.pop_back();
    }
    if (!mantissa.empty() && mantissa.back() == '.') {
        mantissa.pop_back();
    }
    if (mantissa == "-0") {
        mantissa = "0";
    }

    const int exponent_value = std::stoi(text.substr(exponent + 1));
    std::ostringstream normalized;
    normalized << mantissa << 'e' << (exponent_value >= 0 ? '+' : '-')
               << std::setw(2) << std::setfill('0') << std::abs(exponent_value);
    return NarrowToWide(normalized.str());
}

bool IsInteger(double value) {
    return std::isfinite(value) && std::abs(value - std::round(value)) < 1e-12;
}

bool IsLatexUnaryFunction(const std::wstring& command, std::wstring& name) {
    if (command == L"sin" || command == L"cos" || command == L"tan" ||
        command == L"asin" || command == L"acos" || command == L"atan") {
        name = command;
        return true;
    }
    if (command == L"arcsin") {
        name = L"asin";
        return true;
    }
    if (command == L"arccos") {
        name = L"acos";
        return true;
    }
    if (command == L"arctan") {
        name = L"atan";
        return true;
    }
    return false;
}

double PositiveModulo(double left, double right) {
    const double result = std::fmod(left, right);
    if (std::abs(result) < kZeroTolerance) {
        return 0.0;
    }
    if (result < 0.0) {
        return result + std::abs(right);
    }
    return result;
}

class LatexNormalizer {
public:
    explicit LatexNormalizer(std::wstring_view input) : input_(input) {}

    std::wstring Run() {
        const std::wstring normalized = ParseSegment(L'\0');
        if (!error_.empty()) {
            return L"";
        }
        return normalized;
    }

    const std::wstring& error() const {
        return error_;
    }

private:
    std::wstring ParseSegment(wchar_t terminator) {
        std::wstring output;

        while (pos_ < input_.size()) {
            const wchar_t ch = input_[pos_];
            if (terminator != L'\0' && ch == terminator) {
                ++pos_;
                return output;
            }

            if (ch == L'\\') {
                output += ParseCommand();
                if (!error_.empty()) {
                    return L"";
                }
                continue;
            }

            if (ch == L'{') {
                ++pos_;
                const std::wstring inner = ParseSegment(L'}');
                if (!error_.empty()) {
                    return L"";
                }
                output += L"(" + inner + L")";
                continue;
            }

            if (ch == L'(') {
                ++pos_;
                const std::wstring inner = ParseSegment(L')');
                if (!error_.empty()) {
                    return L"";
                }
                output += L"(" + inner + L")";
                continue;
            }

            if (ch == L'^') {
                ++pos_;
                if (pos_ < input_.size() && input_[pos_] == L'{') {
                    ++pos_;
                    const std::wstring exponent = ParseSegment(L'}');
                    if (!error_.empty()) {
                        return L"";
                    }
                    output += L"^(" + exponent + L")";
                } else {
                    output += L"^";
                }
                continue;
            }

            if (ch == L'×') {
                output += L"*";
                ++pos_;
                continue;
            }
            if (ch == L'÷') {
                output += L"/";
                ++pos_;
                continue;
            }
            if (ch == L'−') {
                output += L"-";
                ++pos_;
                continue;
            }
            if (ch == L'π') {
                output += L"pi";
                ++pos_;
                continue;
            }

            output.push_back(ch);
            ++pos_;
        }

        if (terminator != L'\0') {
            Fail(L"missing closing delimiter");
            return L"";
        }

        return output;
    }

    std::wstring ParseCommand() {
        ++pos_;
        const size_t command_start = pos_;
        while (pos_ < input_.size() && std::iswalpha(input_[pos_])) {
            ++pos_;
        }

        if (command_start == pos_) {
            Fail(L"unsupported LaTeX escape");
            return L"";
        }

        const std::wstring command = input_.substr(command_start, pos_ - command_start);

        if (command == L"frac") {
            const std::wstring numerator = ParseRequiredGroup();
            if (!error_.empty()) {
                return L"";
            }
            const std::wstring denominator = ParseRequiredGroup();
            if (!error_.empty()) {
                return L"";
            }
            return L"((" + numerator + L")/(" + denominator + L"))";
        }

        if (command == L"sqrt") {
            SkipSpaces();
            std::wstring degree;
            if (pos_ < input_.size() && input_[pos_] == L'[') {
                ++pos_;
                degree = ParseSegment(L']');
                if (!error_.empty()) {
                    return L"";
                }
                SkipSpaces();
            }

            std::wstring radicand;
            if (pos_ < input_.size() && input_[pos_] == L'{') {
                radicand = ParseRequiredGroup();
            } else if (pos_ < input_.size() && input_[pos_] == L'(') {
                radicand = ParseRequiredParentheses();
            } else {
                Fail(L"\\sqrt requires a grouped value");
                return L"";
            }

            if (!degree.empty()) {
                return L"root(" + degree + L"," + radicand + L")";
            }
            return L"sqrt(" + radicand + L")";
        }

        if (command == L"log") {
            const std::wstring base = ParseOptionalSubscript();
            const std::wstring argument = ParseOptionalArgument();
            if (!error_.empty()) {
                return L"";
            }

            if (!base.empty()) {
                if (argument.empty()) {
                    Fail(L"\\log with a base requires a grouped value");
                    return L"";
                }
                return L"log(" + base + L"," + argument + L")";
            }

            if (!argument.empty()) {
                return L"log(" + argument + L")";
            }
            return L"log";
        }

        if (command == L"ln") {
            const std::wstring argument = ParseOptionalArgument();
            if (!error_.empty()) {
                return L"";
            }
            if (!argument.empty()) {
                return L"ln(" + argument + L")";
            }
            return L"ln";
        }

        std::wstring unary_function;
        if (IsLatexUnaryFunction(command, unary_function)) {
            const std::wstring argument = ParseOptionalArgument();
            if (!error_.empty()) {
                return L"";
            }
            if (!argument.empty()) {
                return unary_function + L"(" + argument + L")";
            }
            return unary_function;
        }

        if (command == L"cdot" || command == L"times" || command == L"ast") {
            return L"*";
        }
        if (command == L"div") {
            return L"/";
        }
        if (command == L"mod" || command == L"bmod") {
            return L"%";
        }
        if (command == L"left" || command == L"right") {
            return L"";
        }
        if (command == L"pi") {
            return L"pi";
        }
        if (command == L"e") {
            return L"e";
        }

        Fail(L"unsupported LaTeX command: \\" + command);
        return L"";
    }

    std::wstring ParseRequiredGroup() {
        SkipSpaces();
        if (pos_ >= input_.size() || input_[pos_] != L'{') {
            Fail(L"expected {...}");
            return L"";
        }
        ++pos_;
        return ParseSegment(L'}');
    }

    std::wstring ParseRequiredParentheses() {
        SkipSpaces();
        if (pos_ >= input_.size() || input_[pos_] != L'(') {
            Fail(L"expected (...)");
            return L"";
        }
        ++pos_;
        return ParseSegment(L')');
    }

    std::wstring ParseOptionalSubscript() {
        SkipSpaces();
        if (pos_ >= input_.size() || input_[pos_] != L'_') {
            return L"";
        }

        ++pos_;
        SkipSpaces();
        if (pos_ < input_.size() && input_[pos_] == L'{') {
            return ParseRequiredGroup();
        }

        const size_t start = pos_;
        while (pos_ < input_.size() &&
               (std::iswalnum(input_[pos_]) || input_[pos_] == L'.')) {
            ++pos_;
        }

        if (start == pos_) {
            Fail(L"expected log base after _");
            return L"";
        }

        return input_.substr(start, pos_ - start);
    }

    std::wstring ParseOptionalArgument() {
        SkipSpaces();
        if (pos_ >= input_.size()) {
            return L"";
        }
        if (input_[pos_] == L'{') {
            return ParseRequiredGroup();
        }
        if (input_[pos_] == L'(') {
            return ParseRequiredParentheses();
        }
        return L"";
    }

    void SkipSpaces() {
        while (pos_ < input_.size() && std::iswspace(input_[pos_])) {
            ++pos_;
        }
    }

    void Fail(std::wstring message) {
        if (error_.empty()) {
            error_ = std::move(message);
        }
    }

    std::wstring input_;
    size_t pos_ = 0;
    std::wstring error_;
};

enum class TokenType {
    End,
    Number,
    Identifier,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Caret,
    LParen,
    RParen,
    Comma,
    Unknown,
};

struct Token {
    TokenType type = TokenType::End;
    double number = 0.0;
    std::wstring text;
    size_t pos = 0;
};

class Lexer {
public:
    explicit Lexer(std::wstring input) : input_(std::move(input)) {}

    Token Next() {
        while (pos_ < input_.size() && std::iswspace(input_[pos_])) {
            ++pos_;
        }

        const size_t start = pos_;
        if (pos_ >= input_.size()) {
            return Token{TokenType::End, 0.0, L"", start};
        }

        const wchar_t ch = input_[pos_];
        if (std::iswdigit(ch) || (ch == L'.' && pos_ + 1 < input_.size() &&
                                      std::iswdigit(input_[pos_ + 1]))) {
            return ReadNumber();
        }

        if (std::iswalpha(ch)) {
            ++pos_;
            while (pos_ < input_.size() && std::iswalpha(input_[pos_])) {
                ++pos_;
            }
            return Token{TokenType::Identifier, 0.0, input_.substr(start, pos_ - start), start};
        }

        ++pos_;
        switch (ch) {
            case L'+':
                return Token{TokenType::Plus, 0.0, L"+", start};
            case L'-':
                return Token{TokenType::Minus, 0.0, L"-", start};
            case L'*':
                return Token{TokenType::Star, 0.0, L"*", start};
            case L'/':
                return Token{TokenType::Slash, 0.0, L"/", start};
            case L'%':
                return Token{TokenType::Percent, 0.0, L"%", start};
            case L'^':
                return Token{TokenType::Caret, 0.0, L"^", start};
            case L'(':
                return Token{TokenType::LParen, 0.0, L"(", start};
            case L')':
                return Token{TokenType::RParen, 0.0, L")", start};
            case L',':
                return Token{TokenType::Comma, 0.0, L",", start};
            default:
                return Token{TokenType::Unknown, 0.0, std::wstring(1, ch), start};
        }
    }

private:
    Token ReadNumber() {
        const size_t start = pos_;

        while (pos_ < input_.size() && std::iswdigit(input_[pos_])) {
            ++pos_;
        }
        if (pos_ < input_.size() && input_[pos_] == L'.') {
            ++pos_;
            while (pos_ < input_.size() && std::iswdigit(input_[pos_])) {
                ++pos_;
            }
        }
        if (pos_ < input_.size() && (input_[pos_] == L'e' || input_[pos_] == L'E')) {
            const size_t exponent = pos_;
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == L'+' || input_[pos_] == L'-')) {
                ++pos_;
            }

            const size_t digits = pos_;
            while (pos_ < input_.size() && std::iswdigit(input_[pos_])) {
                ++pos_;
            }
            if (digits == pos_) {
                pos_ = exponent;
            }
        }

        const std::wstring number_text = input_.substr(start, pos_ - start);
        wchar_t* end = nullptr;
        const double value = std::wcstod(number_text.c_str(), &end);
        if (end == number_text.c_str() || !std::isfinite(value)) {
            return Token{TokenType::Unknown, 0.0, number_text, start};
        }

        return Token{TokenType::Number, value, number_text, start};
    }

    std::wstring input_;
    size_t pos_ = 0;
};

class Parser {
public:
    explicit Parser(std::wstring input) : lexer_(std::move(input)) {
        current_ = lexer_.Next();
    }

    Evaluation Parse() {
        Evaluation result;
        const double value = ParseExpression();
        if (error_.empty() && current_.type != TokenType::End) {
            Fail(L"unexpected token: " + current_.text);
        }

        if (!error_.empty()) {
            result.ok = false;
            result.error = error_;
            return result;
        }

        result.ok = true;
        result.value = value;
        result.output = FormatNumber(value);
        return result;
    }

private:
    double ParseExpression() {
        return ParseAdditive();
    }

    double ParseAdditive() {
        double left = ParseMultiplicative();

        while (error_.empty() &&
               (current_.type == TokenType::Plus || current_.type == TokenType::Minus)) {
            const TokenType op = current_.type;
            Advance();
            const double right = ParseMultiplicative();
            left = (op == TokenType::Plus) ? left + right : left - right;
            CheckFinite(left);
        }

        return left;
    }

    double ParseMultiplicative() {
        double left = ParseUnary();

        while (error_.empty()) {
            if (current_.type == TokenType::Star || current_.type == TokenType::Slash ||
                current_.type == TokenType::Percent) {
                const TokenType op = current_.type;
                Advance();
                const double right = ParseUnary();
                if (op == TokenType::Slash) {
                    if (std::abs(right) < kZeroTolerance) {
                        Fail(L"division by zero");
                        return 0.0;
                    }
                    left /= right;
                } else if (op == TokenType::Percent) {
                    if (std::abs(right) < kZeroTolerance) {
                        Fail(L"modulo by zero");
                        return 0.0;
                    }
                    left = std::fmod(left, right);
                } else {
                    left *= right;
                }
                CheckFinite(left);
                continue;
            }

            if (StartsPrimary(current_.type)) {
                const double right = ParseUnary();
                left *= right;
                CheckFinite(left);
                continue;
            }

            break;
        }

        return left;
    }

    double ParseUnary() {
        if (current_.type == TokenType::Plus) {
            Advance();
            return ParseUnary();
        }
        if (current_.type == TokenType::Minus) {
            Advance();
            return -ParseUnary();
        }
        return ParsePower();
    }

    double ParsePower() {
        double base = ParsePrimary();

        if (error_.empty() && current_.type == TokenType::Caret) {
            Advance();
            const double exponent = ParseUnary();
            base = std::pow(base, exponent);
            CheckFinite(base);
        }

        return base;
    }

    double ParsePrimary() {
        if (current_.type == TokenType::Number) {
            const double value = current_.number;
            Advance();
            return value;
        }

        if (current_.type == TokenType::Identifier) {
            const std::wstring name = current_.text;
            Advance();

            if (name == L"pi") {
                return kPi;
            }
            if (name == L"e") {
                return kE;
            }

            return ParseFunction(name);
        }

        if (current_.type == TokenType::LParen) {
            Advance();
            const double value = ParseExpression();
            Expect(TokenType::RParen, L"expected ')'");
            return value;
        }

        if (current_.type == TokenType::Unknown) {
            Fail(L"unsupported character: " + current_.text);
            return 0.0;
        }

        Fail(L"expected a number, constant, function, or '('");
        return 0.0;
    }

    double ParseFunction(const std::wstring& name) {
        if (current_.type != TokenType::LParen) {
            Fail(L"function requires parentheses: " + name);
            return 0.0;
        }

        Advance();
        std::vector<double> args;
        if (current_.type != TokenType::RParen) {
            while (error_.empty()) {
                args.push_back(ParseExpression());
                if (current_.type != TokenType::Comma) {
                    break;
                }
                Advance();
            }
        }
        Expect(TokenType::RParen, L"expected ')'");

        if (!error_.empty()) {
            return 0.0;
        }

        if (name == L"sqrt") {
            RequireArgCount(name, args, 1);
            if (!error_.empty()) {
                return 0.0;
            }
            if (args[0] < 0.0) {
                Fail(L"sqrt requires a non-negative value");
                return 0.0;
            }
            return std::sqrt(args[0]);
        }

        if (name == L"root") {
            RequireArgCount(name, args, 2);
            if (!error_.empty()) {
                return 0.0;
            }
            return EvaluateRoot(args[0], args[1]);
        }

        if (name == L"ln") {
            RequireArgCount(name, args, 1);
            if (!error_.empty()) {
                return 0.0;
            }
            if (args[0] <= 0.0) {
                Fail(L"ln requires a positive value");
                return 0.0;
            }
            return std::log(args[0]);
        }

        if (name == L"log") {
            if (args.size() == 1) {
                if (args[0] <= 0.0) {
                    Fail(L"log requires a positive value");
                    return 0.0;
                }
                return std::log10(args[0]);
            }
            if (args.size() == 2) {
                const double base = args[0];
                const double value = args[1];
                if (base <= 0.0 || std::abs(base - 1.0) < kZeroTolerance) {
                    Fail(L"log base must be positive and not 1");
                    return 0.0;
                }
                if (value <= 0.0) {
                    Fail(L"log value must be positive");
                    return 0.0;
                }
                return std::log(value) / std::log(base);
            }
            Fail(L"log expects 1 or 2 arguments");
            return 0.0;
        }

        if (name == L"sin" || name == L"cos" || name == L"tan" || name == L"asin" ||
            name == L"acos" || name == L"atan") {
            RequireArgCount(name, args, 1);
            if (!error_.empty()) {
                return 0.0;
            }
            if ((name == L"asin" || name == L"acos") &&
                (args[0] < -1.0 || args[0] > 1.0)) {
                Fail(name + L" requires a value from -1 to 1");
                return 0.0;
            }
            if (name == L"sin") {
                return std::sin(args[0]);
            }
            if (name == L"cos") {
                return std::cos(args[0]);
            }
            if (name == L"tan") {
                return std::tan(args[0]);
            }
            if (name == L"asin") {
                return std::asin(args[0]);
            }
            if (name == L"acos") {
                return std::acos(args[0]);
            }
            return std::atan(args[0]);
        }

        if (name == L"mod" || name == L"rem" || name == L"remainder") {
            RequireArgCount(name, args, 2);
            if (!error_.empty()) {
                return 0.0;
            }
            if (std::abs(args[1]) < kZeroTolerance) {
                Fail(name + L" divisor cannot be zero");
                return 0.0;
            }
            if (name == L"mod") {
                return PositiveModulo(args[0], args[1]);
            }
            return std::fmod(args[0], args[1]);
        }

        Fail(L"unknown function: " + name);
        return 0.0;
    }

    double EvaluateRoot(double degree, double value) {
        if (std::abs(degree) < kZeroTolerance) {
            Fail(L"root degree cannot be zero");
            return 0.0;
        }

        if (value < 0.0) {
            if (!IsInteger(degree)) {
                Fail(L"negative roots require an integer degree");
                return 0.0;
            }

            const long long rounded = static_cast<long long>(std::llround(degree));
            if (rounded % 2 == 0) {
                Fail(L"even root of a negative value is not real");
                return 0.0;
            }

            const double result = -std::pow(-value, 1.0 / degree);
            CheckFinite(result);
            return result;
        }

        const double result = std::pow(value, 1.0 / degree);
        CheckFinite(result);
        return result;
    }

    void RequireArgCount(const std::wstring& name, const std::vector<double>& args, size_t count) {
        if (args.size() != count) {
            std::wostringstream stream;
            stream << name << L" expects " << count << L" argument";
            if (count != 1) {
                stream << L"s";
            }
            Fail(stream.str());
        }
    }

    void CheckFinite(double value) {
        if (!std::isfinite(value)) {
            Fail(L"result is outside the supported real-number range");
        }
    }

    bool StartsPrimary(TokenType type) const {
        return type == TokenType::Number || type == TokenType::Identifier ||
               type == TokenType::LParen;
    }

    void Expect(TokenType type, const std::wstring& message) {
        if (current_.type != type) {
            Fail(message);
            return;
        }
        Advance();
    }

    void Advance() {
        current_ = lexer_.Next();
    }

    void Fail(std::wstring message) {
        if (error_.empty()) {
            error_ = std::move(message);
        }
    }

    Lexer lexer_;
    Token current_;
    std::wstring error_;
};

}  // namespace

Evaluation EvaluateExpression(std::wstring_view line) {
    Evaluation result;
    const std::wstring trimmed = Trim(line);
    if (trimmed.empty()) {
        result.ok = false;
        result.error = L"empty expression";
        return result;
    }

    LatexNormalizer normalizer(trimmed);
    const std::wstring normalized = normalizer.Run();
    if (!normalizer.error().empty()) {
        result.ok = false;
        result.error = normalizer.error();
        return result;
    }

    Parser parser(normalized);
    return parser.Parse();
}

Evaluation EvaluateExpressionScientific(std::wstring_view line) {
    Evaluation result = EvaluateExpression(line);
    if (result.ok) {
        result.output = FormatScientificNumber(result.value);
    }
    return result;
}

}  // namespace textcalc
