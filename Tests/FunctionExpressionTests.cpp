#include "../Source/FuncGame/Math/FunctionExpression.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
    int Failures = 0;

    void ExpectValue(const std::string& Source, double X, double Expected, double Tolerance = 1e-9)
    {
        FuncMath::Expression Expression;
        FuncMath::Error Error;
        double Actual = 0.0;
        if (!FuncMath::Expression::Compile(Source, Expression, Error) || !Expression.Evaluate(X, Actual, Error) || std::abs(Actual - Expected) > Tolerance)
        {
            std::cerr << "FAIL value: " << Source << " at " << X << " expected " << Expected << " got " << Actual << ": " << Error.Message << '\n';
            ++Failures;
        }
    }

    void ExpectInvalid(const std::string& Source)
    {
        FuncMath::Expression Expression;
        FuncMath::Error Error;
        if (FuncMath::Expression::Compile(Source, Expression, Error))
        {
            std::cerr << "FAIL syntax accepted: " << Source << '\n';
            ++Failures;
        }
    }

    void ExpectDomainError(const std::string& Source, double X)
    {
        FuncMath::Expression Expression;
        FuncMath::Error Error;
        double Result = 0.0;
        if (!FuncMath::Expression::Compile(Source, Expression, Error) || Expression.Evaluate(X, Result, Error))
        {
            std::cerr << "FAIL domain: " << Source << '\n';
            ++Failures;
        }
    }
}

int main()
{
    ExpectValue("2x+1", 3, 7);
    ExpectValue("2(x+1)", 3, 8);
    ExpectValue("(x+1)(x-1)", 3, 8);
    ExpectValue("2sin(x)", 0, 0);
    ExpectValue("-x^2", 3, -9);
    ExpectValue("(-x)^2", 3, 9);
    ExpectValue("2^3^2", 0, 512);
    ExpectValue("2^-2", 0, 0.25);
    ExpectValue("1e-3+1E+2", 0, 100.001);
    ExpectValue("log(2,8)+log10(100)+log2(8)+lg(100)", 0, 10);
    ExpectValue("sin(pi/2)+cos(0)", 0, 2);
    ExpectValue("root(-8,3)", 0, -2);
    ExpectValue("root(-8,-3)", 0, -0.5);
    ExpectValue("if(x>=0,sqrt(x),sqrt(-x))", 4, 2);
    ExpectValue("if(x>=0,sqrt(x),sqrt(-x))", -9, 3);
    ExpectValue("clamp(x,-1,1)", 3, 1);
    ExpectValue("SIN(x)+floor(1.8)+ceil(1.2)", 0, 3);
    ExpectValue("\xCF\x80" "x", 2, 6.283185307179586);
    ExpectValue("x\xC2\xB2", 3, 9);
    ExpectInvalid("abcdefg");
    ExpectInvalid("sinx");
    ExpectInvalid("x+");
    ExpectInvalid("x+y");
    ExpectInvalid("sin(x");
    ExpectInvalid("log(2,3,4)");
    ExpectInvalid("sqrt()");
    ExpectInvalid("1.2.3");
    ExpectDomainError("sqrt(-1)", 0);
    ExpectDomainError("1/(x-x)", 1);
    ExpectDomainError("0^0", 0);
    ExpectDomainError("root(-8,2)", 0);
    ExpectDomainError("log(1,2)", 0);
    ExpectDomainError("atanh(1)", 0);
    FuncMath::Expression LimitedExpression;
    FuncMath::Error LimitedError;
    FuncMath::ExpressionLimits Limits;
    Limits.MaxLength=3;
    if (FuncMath::Expression::Compile("x+123",LimitedExpression,LimitedError,Limits))
    { std::cerr << "FAIL expression length limit\n"; ++Failures; }
    Limits.MaxLength=512;
    Limits.EvaluationBudget=1;
    double LimitedValue=0.0;
    if (!FuncMath::Expression::Compile("x+1",LimitedExpression,LimitedError,Limits) ||
        LimitedExpression.Evaluate(0,LimitedValue,LimitedError) || LimitedError.Message!="求值预算已用尽")
    { std::cerr << "FAIL expression evaluation budget\n"; ++Failures; }
    if (Failures == 0) std::cout << "All expression tests passed.\n";
    return Failures == 0 ? 0 : 1;
}
