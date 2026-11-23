#include "calculator_tool.hpp"

#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace oopagent {
namespace {

class ExpressionParser {
public:
    explicit ExpressionParser(const std::string& expression) : expression_(expression) {}

    double parse() {
        const double value = parse_expression();
        skip_spaces();
        if (position_ != expression_.size()) {
            throw std::invalid_argument("Unexpected character in calculator expression");
        }
        if (!std::isfinite(value)) {
            throw std::invalid_argument("Calculator result is not finite");
        }
        return value;
    }

private:
    double parse_expression() {
        double value = parse_term();
        while (true) {
            skip_spaces();
            if (consume('+')) {
                value += parse_term();
            } else if (consume('-')) {
                value -= parse_term();
            } else {
                return value;
            }
        }
    }

    double parse_term() {
        double value = parse_unary();
        while (true) {
            skip_spaces();
            if (consume('*')) {
                value *= parse_unary();
            } else if (consume('/')) {
                const double divisor = parse_unary();
                if (divisor == 0.0) {
                    throw std::invalid_argument("Division by zero");
                }
                value /= divisor;
            } else {
                return value;
            }
        }
    }

    double parse_unary() {
        skip_spaces();
        if (consume('+')) {
            return parse_unary();
        }
        if (consume('-')) {
            return -parse_unary();
        }
        return parse_primary();
    }

    double parse_primary() {
        skip_spaces();
        if (consume('(')) {
            const double value = parse_expression();
            skip_spaces();
            if (!consume(')')) {
                throw std::invalid_argument("Missing closing parenthesis");
            }
            return value;
        }

        const char* start = expression_.c_str() + position_;
        char* end = nullptr;
        const double value = std::strtod(start, &end);
        if (end == start) {
            throw std::invalid_argument("Expected a number in calculator expression");
        }
        position_ += static_cast<std::size_t>(end - start);
        return value;
    }

    void skip_spaces() {
        while (position_ < expression_.size() &&
               (expression_[position_] == ' ' || expression_[position_] == '\t' ||
                expression_[position_] == '\n' || expression_[position_] == '\r')) {
            ++position_;
        }
    }

    bool consume(char expected) {
        if (position_ < expression_.size() && expression_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    const std::string& expression_;
    std::size_t position_ = 0;
};

} // namespace

std::string CalculatorTool::name() const {
    return "calculator";
}

std::string CalculatorTool::description() const {
    return "Evaluate a basic arithmetic expression using numbers, parentheses, +, -, *, and /.";
}

nlohmann::json CalculatorTool::parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"expression", {
                {"type", "string"},
                {"description", "Arithmetic expression, for example (15 * 17) + 2"}
            }}
        }},
        {"required", {"expression"}},
        {"additionalProperties", false}
    };
}

ToolResult CalculatorTool::execute(const nlohmann::json& arguments) {
    if (!arguments.is_object() || !arguments.contains("expression") ||
        !arguments.at("expression").is_string()) {
        throw std::invalid_argument("calculator requires a string argument named 'expression'");
    }

    const std::string expression = arguments.at("expression").get<std::string>();
    const double value = ExpressionParser(expression).parse();
    return {true, std::to_string(value)};
}

} // namespace oopagent
