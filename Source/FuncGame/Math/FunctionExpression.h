#pragma once

#include <memory>
#include <string>
#include <vector>

namespace FuncMath
{
    struct Error
    {
        std::string Message;
        std::size_t Position = 0;
    };

    struct FunctionInfo
    {
        const char* Name;
        int MinArguments;
        int MaxArguments;
    };

    struct ExpressionLimits
    {
        std::size_t MaxLength = 512;
        int MaxNodes = 256;
        // Includes parser grammar frames as well as parenthesis nesting.
        int MaxDepth = 512;
        int EvaluationBudget = 257;
    };

    // Immutable after compilation, so in-flight attacks retain their own expression.
    class Expression
    {
    public:
        struct Node;

        static bool Compile(const std::string& Source, Expression& Out, Error& OutError,
                            const ExpressionLimits& Limits = {});
        static const std::vector<FunctionInfo>& Functions();
        bool Evaluate(double X, double& OutValue, Error& OutError) const;
        // Conservative interval bounds. False means the interval is undefined,
        // discontinuous or cannot be proven safe with the current rules.
        bool Bounds(double From, double To, double& OutMin, double& OutMax) const;
        bool IsValid() const { return static_cast<bool>(Root); }

    private:
        std::shared_ptr<const Node> Root;
        int EvaluationBudget = 257;
    };
}
