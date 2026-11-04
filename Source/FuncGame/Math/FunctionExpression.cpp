#include "FunctionExpression.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <utility>

namespace FuncMath
{
    namespace
    {
        constexpr double Pi = 3.141592653589793238462643383279502884;
        constexpr double E = 2.718281828459045235360287471352662498;
        constexpr double ZeroTolerance = 1.0e-12;
        enum class Kind { Number, Variable, Unary, Binary, Function };
        enum class TokenKind { End, Number, Name, Plus, Minus, Star, Slash, Power, Left, Right, Comma, Comparison, Invalid };

        struct Token
        {
            TokenKind Type = TokenKind::End;
            std::string Text;
            double Number = 0.0;
            std::size_t Position = 0;
        };

        std::string Normalize(std::string Text)
        {
            const std::pair<const char*, const char*> Replacements[] = {
                {"\xCF\x80", "(pi)"}, {"\xCE\xA0", "(pi)"}, {"\xEF\xBC\x88", "("}, {"\xEF\xBC\x89", ")"},
                {"\xEF\xBC\x8C", ","}, {"\xC3\x97", "*"}, {"\xC3\xB7", "/"}, {"\xE2\x88\x92", "-"},
                {"\xEF\xBC\x8B", "+"}, {"\xEF\xBC\x8D", "-"}, {"\xC2\xB2", "^2"}, {"\xC2\xB3", "^3"}
            };
            for (const auto& Item : Replacements)
            {
                std::size_t At = 0;
                while ((At = Text.find(Item.first, At)) != std::string::npos)
                {
                    Text.replace(At, std::char_traits<char>::length(Item.first), Item.second);
                    At += std::char_traits<char>::length(Item.second);
                }
            }
            return Text;
        }

        const FunctionInfo* FindFunction(const std::string& Name)
        {
            const auto& Functions = Expression::Functions();
            const auto It = std::find_if(Functions.begin(), Functions.end(), [&](const FunctionInfo& Info) { return Name == Info.Name; });
            return It == Functions.end() ? nullptr : &*It;
        }
    }

    struct Expression::Node
    {
        Kind Type = Kind::Number;
        std::string Op;
        double Number = 0.0;
        std::size_t Position = 0;
        std::vector<std::shared_ptr<const Node>> Children;
    };

    namespace
    {
        using NodePtr = std::shared_ptr<const Expression::Node>;

        class Parser
        {
        public:
            Parser(std::string InText, const ExpressionLimits& InLimits)
                : Text(Normalize(std::move(InText))), Limits(InLimits) { Next(); }

            NodePtr Parse()
            {
                if (Text.size() > Limits.MaxLength) { Fail("表达式过长", 0); return {}; }
                NodePtr Result = Compare(0);
                if (!Failed && Current.Type != TokenKind::End) Fail("无法识别的字符或多余内容", Current.Position);
                return Failed ? NodePtr{} : Result;
            }
            Error GetError() const { return Failure; }

        private:
            std::string Text;
            ExpressionLimits Limits;
            std::size_t Cursor = 0;
            Token Current;
            bool Failed = false;
            int NodeCount = 0;
            Error Failure;

            void Fail(const std::string& Message, std::size_t Position)
            {
                if (!Failed) { Failed = true; Failure = {Message, Position}; }
            }

            void Next()
            {
                while (Cursor < Text.size() && std::isspace(static_cast<unsigned char>(Text[Cursor]))) ++Cursor;
                Current = {};
                Current.Position = Cursor;
                if (Cursor >= Text.size()) return;
                const char Ch = Text[Cursor];
                if (std::isdigit(static_cast<unsigned char>(Ch)) || (Ch == '.' && Cursor + 1 < Text.size() && std::isdigit(static_cast<unsigned char>(Text[Cursor + 1]))))
                {
                    const char* Begin = Text.c_str() + Cursor;
                    char* End = nullptr;
                    Current.Number = std::strtod(Begin, &End);
                    Cursor += static_cast<std::size_t>(End - Begin);
                    Current.Text.assign(Begin, static_cast<std::size_t>(End - Begin));
                    Current.Type = TokenKind::Number;
                    if (!std::isfinite(Current.Number)) Fail("数字超出可计算范围", Current.Position);
                    return;
                }
                if (std::isalpha(static_cast<unsigned char>(Ch)) || Ch == '_')
                {
                    const std::size_t Begin = Cursor++;
                    while (Cursor < Text.size() && (std::isalnum(static_cast<unsigned char>(Text[Cursor])) || Text[Cursor] == '_')) ++Cursor;
                    Current.Text = Text.substr(Begin, Cursor - Begin);
                    std::transform(Current.Text.begin(), Current.Text.end(), Current.Text.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
                    Current.Type = TokenKind::Name;
                    return;
                }
                ++Cursor;
                Current.Text.assign(1, Ch);
                switch (Ch)
                {
                    case '+': Current.Type = TokenKind::Plus; break;
                    case '-': Current.Type = TokenKind::Minus; break;
                    case '*': Current.Type = TokenKind::Star; break;
                    case '/': Current.Type = TokenKind::Slash; break;
                    case '^': Current.Type = TokenKind::Power; break;
                    case '(': Current.Type = TokenKind::Left; break;
                    case ')': Current.Type = TokenKind::Right; break;
                    case ',': Current.Type = TokenKind::Comma; break;
                    case '<': case '>': case '=': case '!':
                        Current.Type = TokenKind::Comparison;
                        if (Cursor < Text.size() && Text[Cursor] == '=') Current.Text += Text[Cursor++];
                        if ((Ch == '=' || Ch == '!') && Current.Text.size() == 1) Current.Type = TokenKind::Invalid;
                        break;
                    default: Current.Type = TokenKind::Invalid; break;
                }
            }

            NodePtr Make(Kind Type, std::string Op, std::size_t Position, std::vector<NodePtr> Children = {}, double Number = 0.0)
            {
                if (++NodeCount > Limits.MaxNodes) { Fail("表达式过于复杂", Position); return {}; }
                auto N = std::make_shared<Expression::Node>();
                N->Type = Type; N->Op = std::move(Op); N->Position = Position; N->Children = std::move(Children); N->Number = Number;
                return N;
            }
            bool CheckDepth(int Depth)
            {
                if (Depth <= Limits.MaxDepth) return true;
                Fail("表达式嵌套过深", Current.Position); return false;
            }
            NodePtr Compare(int Depth)
            {
                NodePtr Left = Add(Depth + 1);
                while (!Failed && Current.Type == TokenKind::Comparison)
                {
                    const Token Op = Current; Next();
                    NodePtr Right = Add(Depth + 1);
                    Left = Make(Kind::Binary, Op.Text, Op.Position, {Left, Right});
                }
                return Left;
            }
            NodePtr Add(int Depth)
            {
                NodePtr Left = Multiply(Depth + 1);
                while (!Failed && (Current.Type == TokenKind::Plus || Current.Type == TokenKind::Minus))
                {
                    const Token Op = Current; Next();
                    NodePtr Right = Multiply(Depth + 1);
                    Left = Make(Kind::Binary, Op.Text, Op.Position, {Left, Right});
                }
                return Left;
            }
            NodePtr Multiply(int Depth)
            {
                NodePtr Left = Unary(Depth + 1);
                while (!Failed)
                {
                    const bool Implicit = Current.Type == TokenKind::Name || Current.Type == TokenKind::Left;
                    if (!Implicit && Current.Type != TokenKind::Star && Current.Type != TokenKind::Slash) break;
                    Token Op = Current;
                    if (!Implicit) Next();
                    NodePtr Right = Unary(Depth + 1);
                    Left = Make(Kind::Binary, Implicit ? "*" : Op.Text, Op.Position, {Left, Right});
                }
                return Left;
            }
            NodePtr Unary(int Depth)
            {
                if (!CheckDepth(Depth)) return {};
                if (Current.Type == TokenKind::Plus || Current.Type == TokenKind::Minus)
                {
                    const Token Op = Current; Next();
                    return Make(Kind::Unary, Op.Text, Op.Position, {Unary(Depth + 1)});
                }
                return Power(Depth + 1);
            }
            NodePtr Power(int Depth)
            {
                NodePtr Left = Primary(Depth + 1);
                if (!Failed && Current.Type == TokenKind::Power)
                {
                    const Token Op = Current; Next();
                    Left = Make(Kind::Binary, "^", Op.Position, {Left, Unary(Depth + 1)});
                }
                return Left;
            }
            NodePtr Primary(int Depth)
            {
                if (!CheckDepth(Depth)) return {};
                const Token T = Current;
                if (T.Type == TokenKind::Number) { Next(); return Make(Kind::Number, "", T.Position, {}, T.Number); }
                if (T.Type == TokenKind::Left)
                {
                    Next(); NodePtr N = Compare(Depth + 1);
                    if (Current.Type != TokenKind::Right) { Fail("缺少右括号", Current.Position); return {}; }
                    Next(); return N;
                }
                if (T.Type != TokenKind::Name) { Fail("缺少数字、x 或函数", T.Position); return {}; }
                Next();
                if (T.Text == "x") return Make(Kind::Variable, "", T.Position);
                if (T.Text == "pi") return Make(Kind::Number, "", T.Position, {}, Pi);
                if (T.Text == "e") return Make(Kind::Number, "", T.Position, {}, E);
                const FunctionInfo* Info = FindFunction(T.Text);
                if (!Info) { Fail("未知变量或函数: " + T.Text, T.Position); return {}; }
                if (Current.Type != TokenKind::Left) { Fail("函数名后必须使用括号", Current.Position); return {}; }
                Next(); std::vector<NodePtr> Args;
                if (Current.Type != TokenKind::Right)
                {
                    for (;;)
                    {
                        Args.push_back(Compare(Depth + 1));
                        if (Failed || Current.Type != TokenKind::Comma) break;
                        Next();
                    }
                }
                if (Current.Type != TokenKind::Right) { Fail("函数缺少右括号", Current.Position); return {}; }
                Next();
                if (static_cast<int>(Args.size()) < Info->MinArguments || static_cast<int>(Args.size()) > Info->MaxArguments)
                {
                    Fail("函数参数数量不正确: " + T.Text, T.Position); return {};
                }
                return Make(Kind::Function, T.Text, T.Position, std::move(Args));
            }
        };

        bool EvaluateNode(const NodePtr& N, double X, double& Out, Error& Err, int& Budget)
        {
            if (!N || --Budget < 0) { Err = {"求值预算已用尽", 0}; return false; }
            const auto Domain = [&]() { Err = {"函数在此处没有有限实数值", N->Position}; return false; };
            const auto Eval = [&](std::size_t Index, double& Value) { return EvaluateNode(N->Children[Index], X, Value, Err, Budget); };
            if (N->Type == Kind::Number) { Out = N->Number; return true; }
            if (N->Type == Kind::Variable) { Out = X; return std::isfinite(X) || Domain(); }
            double A = 0.0, B = 0.0, C = 0.0;
            if (!Eval(0, A)) return false;
            if (N->Type == Kind::Unary) { Out = N->Op == "-" ? -A : A; return std::isfinite(Out) || Domain(); }
            if (N->Type == Kind::Function && N->Op == "if")
            {
                if (!Eval(A != 0.0 ? 1 : 2, Out)) return false;
                return std::isfinite(Out) || Domain();
            }
            if (N->Children.size() > 1 && !Eval(1, B)) return false;
            if (N->Children.size() > 2 && !Eval(2, C)) return false;
            const std::string& Op = N->Op;
            if (N->Type == Kind::Binary)
            {
                if (Op == "+") Out = A + B;
                else if (Op == "-") Out = A - B;
                else if (Op == "*") Out = A * B;
                else if (Op == "/") { if (std::abs(B) <= ZeroTolerance) return Domain(); Out = A / B; }
                else if (Op == "^") { if (A == 0.0 && B <= 0.0) return Domain(); Out = std::pow(A, B); }
                else if (Op == "<") Out = A < B;
                else if (Op == "<=") Out = A <= B;
                else if (Op == ">") Out = A > B;
                else if (Op == ">=") Out = A >= B;
                else if (Op == "==") Out = A == B;
                else if (Op == "!=") Out = A != B;
                else return Domain();
                return std::isfinite(Out) || Domain();
            }
            if (Op == "sqrt") { if (A < 0) return Domain(); Out = std::sqrt(A); }
            else if (Op == "cbrt") Out = std::cbrt(A);
            else if (Op == "root")
            {
                if (B == 0.0 || std::trunc(B) != B || std::abs(B) > 1000000.0 || (A == 0.0 && B < 0.0)) return Domain();
                const bool Odd = std::fmod(std::abs(B), 2.0) == 1.0;
                if (A < 0.0 && !Odd) return Domain();
                Out = (A < 0.0 ? -1.0 : 1.0) * std::pow(std::abs(A), 1.0 / B);
            }
            else if (Op == "abs") Out = std::abs(A);
            else if (Op == "sign") Out = (A > 0.0) - (A < 0.0);
            else if (Op == "min") Out = std::min(A, B);
            else if (Op == "max") Out = std::max(A, B);
            else if (Op == "clamp") { if (B > C) return Domain(); Out = std::max(B, std::min(A, C)); }
            else if (Op == "exp") Out = std::exp(A);
            else if (Op == "ln" || (Op == "log" && N->Children.size() == 1)) { if (A <= 0.0) return Domain(); Out = std::log(A); }
            else if (Op == "log") { if (A <= 0.0 || A == 1.0 || B <= 0.0) return Domain(); Out = std::log(B) / std::log(A); }
            else if (Op == "log10" || Op == "lg") { if (A <= 0.0) return Domain(); Out = std::log10(A); }
            else if (Op == "log2") { if (A <= 0.0) return Domain(); Out = std::log2(A); }
            else if (Op == "sin") Out = std::sin(A);
            else if (Op == "cos") Out = std::cos(A);
            else if (Op == "tan") { if (std::abs(std::cos(A)) <= ZeroTolerance) return Domain(); Out = std::tan(A); }
            else if (Op == "cot") { if (std::abs(std::sin(A)) <= ZeroTolerance) return Domain(); Out = std::cos(A) / std::sin(A); }
            else if (Op == "sec") { if (std::abs(std::cos(A)) <= ZeroTolerance) return Domain(); Out = 1.0 / std::cos(A); }
            else if (Op == "csc") { if (std::abs(std::sin(A)) <= ZeroTolerance) return Domain(); Out = 1.0 / std::sin(A); }
            else if (Op == "asin" || Op == "arcsin") { if (std::abs(A) > 1.0) return Domain(); Out = std::asin(A); }
            else if (Op == "acos" || Op == "arccos") { if (std::abs(A) > 1.0) return Domain(); Out = std::acos(A); }
            else if (Op == "atan" || Op == "arctan") Out = std::atan(A);
            else if (Op == "sinh") Out = std::sinh(A);
            else if (Op == "cosh") Out = std::cosh(A);
            else if (Op == "tanh") Out = std::tanh(A);
            else if (Op == "asinh") Out = std::asinh(A);
            else if (Op == "acosh") { if (A < 1.0) return Domain(); Out = std::acosh(A); }
            else if (Op == "atanh") { if (std::abs(A) >= 1.0) return Domain(); Out = std::atanh(A); }
            else if (Op == "floor") Out = std::floor(A);
            else if (Op == "ceil") Out = std::ceil(A);
            else if (Op == "round") Out = std::round(A);
            else if (Op == "trunc") Out = std::trunc(A);
            else return Domain();
            return std::isfinite(Out) || Domain();
        }

        struct Range
        {
            double Low = 0.0;
            double High = 0.0;
        };

        bool CrossesZero(const Range& R) { return R.Low <= ZeroTolerance && R.High >= -ZeroTolerance; }
        bool ContainsPeriodic(double Low, double High, double Offset, double Period)
        {
            return std::ceil((Low - Offset) / Period) <= std::floor((High - Offset) / Period);
        }
        Range MultiplyRange(const Range& A, const Range& B)
        {
            const double P[] = {A.Low * B.Low, A.Low * B.High, A.High * B.Low, A.High * B.High};
            return {*std::min_element(P, P + 4), *std::max_element(P, P + 4)};
        }
        Range SinRange(const Range& A)
        {
            Range R{std::min(std::sin(A.Low), std::sin(A.High)), std::max(std::sin(A.Low), std::sin(A.High))};
            if (ContainsPeriodic(A.Low, A.High, Pi / 2.0, 2.0 * Pi)) R.High = 1.0;
            if (ContainsPeriodic(A.Low, A.High, -Pi / 2.0, 2.0 * Pi)) R.Low = -1.0;
            return R;
        }
        Range CosRange(const Range& A)
        {
            return SinRange({A.Low + Pi / 2.0, A.High + Pi / 2.0});
        }

        bool BoundNode(const NodePtr& N, const Range& X, Range& Out, int& Budget)
        {
            if (!N || --Budget < 0) return false;
            if (N->Type == Kind::Number) { Out = {N->Number, N->Number}; return true; }
            if (N->Type == Kind::Variable) { Out = X; return true; }
            auto Child = [&](std::size_t Index, Range& R) { return BoundNode(N->Children[Index], X, R, Budget); };
            Range A, B, C;
            if (!Child(0, A)) return false;
            if (N->Type == Kind::Unary)
            {
                Out = N->Op == "-" ? Range{-A.High, -A.Low} : A;
                return true;
            }
            if (N->Type == Kind::Function && N->Op == "if")
            {
                // An uncertain condition may hide a jump or an invalid branch.
                if (A.Low == 0.0 && A.High == 0.0) return Child(2, Out);
                if (A.Low > 0.0 || A.High < 0.0) return Child(1, Out);
                return false;
            }
            if (N->Children.size() > 1 && !Child(1, B)) return false;
            if (N->Children.size() > 2 && !Child(2, C)) return false;
            const std::string& Op = N->Op;
            if (N->Type == Kind::Binary)
            {
                if (Op == "+") Out = {A.Low + B.Low, A.High + B.High};
                else if (Op == "-") Out = {A.Low - B.High, A.High - B.Low};
                else if (Op == "*") Out = MultiplyRange(A, B);
                else if (Op == "/")
                {
                    if (CrossesZero(B)) return false;
                    Out = MultiplyRange(A, {1.0 / B.High, 1.0 / B.Low});
                }
                else if (Op == "^")
                {
                    if (B.Low == B.High && std::trunc(B.Low) == B.Low)
                    {
                        if (CrossesZero(A) && B.Low <= 0.0) return false;
                        const double V1 = std::pow(A.Low, B.Low), V2 = std::pow(A.High, B.Low);
                        Out = {std::min(V1, V2), std::max(V1, V2)};
                        if (A.Low <= 0.0 && A.High >= 0.0 && B.Low > 0.0 && std::fmod(std::abs(B.Low), 2.0) == 0.0) Out.Low = 0.0;
                    }
                    else
                    {
                        if (A.Low < 0.0 || (A.Low == 0.0 && B.Low <= 0.0)) return false;
                        const double V[] = {std::pow(A.Low,B.Low),std::pow(A.Low,B.High),std::pow(A.High,B.Low),std::pow(A.High,B.High)};
                        Out = {*std::min_element(V,V+4),*std::max_element(V,V+4)};
                        if (B.Low <= 0.0 && B.High >= 0.0) { Out.Low = std::min(Out.Low, 1.0); Out.High = std::max(Out.High, 1.0); }
                    }
                }
                else
                {
                    bool Always = false, Never = false;
                    if (Op == "<") { Always = A.High < B.Low; Never = A.Low >= B.High; }
                    else if (Op == "<=") { Always = A.High <= B.Low; Never = A.Low > B.High; }
                    else if (Op == ">") { Always = A.Low > B.High; Never = A.High <= B.Low; }
                    else if (Op == ">=") { Always = A.Low >= B.High; Never = A.High < B.Low; }
                    else if (Op == "==") { Always = A.Low == A.High && B.Low == B.High && A.Low == B.Low; Never = A.High < B.Low || A.Low > B.High; }
                    else if (Op == "!=") { Never = A.Low == A.High && B.Low == B.High && A.Low == B.Low; Always = A.High < B.Low || A.Low > B.High; }
                    else return false;
                    Out = Always ? Range{1,1} : (Never ? Range{0,0} : Range{0,1});
                }
            }
            else if (Op == "sqrt") { if (A.Low < 0) return false; Out = {std::sqrt(A.Low),std::sqrt(A.High)}; }
            else if (Op == "cbrt") Out = {std::cbrt(A.Low),std::cbrt(A.High)};
            else if (Op == "root")
            {
                if (B.Low != B.High || B.Low == 0.0 || std::trunc(B.Low) != B.Low || std::abs(B.Low) > 1000000.0) return false;
                if (A.Low < 0.0 && std::fmod(std::abs(B.Low),2.0) != 1.0) return false;
                if (CrossesZero(A) && B.Low < 0.0) return false;
                auto Root = [&](double V) { return (V < 0 ? -1.0 : 1.0) * std::pow(std::abs(V), 1.0 / B.Low); };
                const double V1=Root(A.Low), V2=Root(A.High);
                Out = {std::min(V1,V2),std::max(V1,V2)};
            }
            else if (Op == "abs") Out = {A.Low <= 0 && A.High >= 0 ? 0.0 : std::min(std::abs(A.Low),std::abs(A.High)),std::max(std::abs(A.Low),std::abs(A.High))};
            else if (Op == "sign")
            {
                const double L=(A.Low>0)-(A.Low<0), H=(A.High>0)-(A.High<0);
                if (L != H) return false;
                Out={L,H};
            }
            else if (Op == "min") Out = {std::min(A.Low,B.Low),std::min(A.High,B.High)};
            else if (Op == "max") Out = {std::max(A.Low,B.Low),std::max(A.High,B.High)};
            else if (Op == "clamp")
            {
                if (B.High > C.Low) return false;
                Out = {std::max(B.Low,std::min(A.Low,C.Low)),std::max(B.High,std::min(A.High,C.High))};
            }
            else if (Op == "exp") Out = {std::exp(A.Low),std::exp(A.High)};
            else if (Op == "ln" || (Op == "log" && N->Children.size() == 1))
            { if (A.Low <= 0) return false; Out={std::log(A.Low),std::log(A.High)}; }
            else if (Op == "log")
            {
                if (A.Low <= 0 || B.Low <= 0 || (A.Low <= 1.0 && A.High >= 1.0)) return false;
                Range Denom{std::log(A.Low),std::log(A.High)};
                if (CrossesZero(Denom)) return false;
                Out = MultiplyRange({std::log(B.Low),std::log(B.High)},{1.0/Denom.High,1.0/Denom.Low});
            }
            else if (Op == "log10" || Op == "lg") { if (A.Low <= 0) return false; Out={std::log10(A.Low),std::log10(A.High)}; }
            else if (Op == "log2") { if (A.Low <= 0) return false; Out={std::log2(A.Low),std::log2(A.High)}; }
            else if (Op == "sin") Out=SinRange(A);
            else if (Op == "cos") Out=CosRange(A);
            else if (Op == "tan")
            {
                if (ContainsPeriodic(A.Low,A.High,Pi/2.0,Pi)) return false;
                Out={std::tan(A.Low),std::tan(A.High)};
            }
            else if (Op == "cot" || Op == "sec" || Op == "csc")
            {
                Range Denom = Op == "sec" ? CosRange(A) : SinRange(A);
                if (CrossesZero(Denom)) return false;
                Range Numer = Op == "cot" ? CosRange(A) : Range{1,1};
                Out=MultiplyRange(Numer,{1.0/Denom.High,1.0/Denom.Low});
            }
            else if (Op == "asin" || Op == "arcsin") { if (A.Low < -1 || A.High > 1) return false; Out={std::asin(A.Low),std::asin(A.High)}; }
            else if (Op == "acos" || Op == "arccos") { if (A.Low < -1 || A.High > 1) return false; Out={std::acos(A.High),std::acos(A.Low)}; }
            else if (Op == "atan" || Op == "arctan") Out={std::atan(A.Low),std::atan(A.High)};
            else if (Op == "sinh") Out={std::sinh(A.Low),std::sinh(A.High)};
            else if (Op == "cosh")
            {
                const double L=std::cosh(A.Low),H=std::cosh(A.High);
                Out={A.Low<=0 && A.High>=0 ? 1.0 : std::min(L,H),std::max(L,H)};
            }
            else if (Op == "tanh") Out={std::tanh(A.Low),std::tanh(A.High)};
            else if (Op == "asinh") Out={std::asinh(A.Low),std::asinh(A.High)};
            else if (Op == "acosh") { if (A.Low < 1) return false; Out={std::acosh(A.Low),std::acosh(A.High)}; }
            else if (Op == "atanh") { if (A.Low <= -1 || A.High >= 1) return false; Out={std::atanh(A.Low),std::atanh(A.High)}; }
            else if (Op == "floor" || Op == "ceil" || Op == "round" || Op == "trunc")
            {
                auto Apply=[&](double V) { return Op=="floor" ? std::floor(V) : Op=="ceil" ? std::ceil(V) : Op=="round" ? std::round(V) : std::trunc(V); };
                const double L=Apply(A.Low),H=Apply(A.High);
                if (L != H) return false;
                Out={L,H};
            }
            else return false;
            return std::isfinite(Out.Low) && std::isfinite(Out.High) && Out.Low <= Out.High;
        }
    }

    const std::vector<FunctionInfo>& Expression::Functions()
    {
        static const std::vector<FunctionInfo> Registry = {
            {"sqrt",1,1}, {"cbrt",1,1}, {"root",2,2}, {"abs",1,1}, {"sign",1,1},
            {"min",2,2}, {"max",2,2}, {"clamp",3,3}, {"exp",1,1}, {"ln",1,1},
            {"log",1,2}, {"log10",1,1}, {"lg",1,1}, {"log2",1,1},
            {"sin",1,1}, {"cos",1,1}, {"tan",1,1}, {"cot",1,1}, {"sec",1,1}, {"csc",1,1},
            {"asin",1,1}, {"acos",1,1}, {"atan",1,1}, {"arcsin",1,1}, {"arccos",1,1}, {"arctan",1,1},
            {"sinh",1,1}, {"cosh",1,1}, {"tanh",1,1}, {"asinh",1,1}, {"acosh",1,1}, {"atanh",1,1},
            {"floor",1,1}, {"ceil",1,1}, {"round",1,1}, {"trunc",1,1}, {"if",3,3}
        };
        return Registry;
    }

    bool Expression::Compile(const std::string& Source, Expression& Out, Error& OutError, const ExpressionLimits& Limits)
    {
        Out = {}; OutError = {};
        if (Limits.MaxLength==0 || Limits.MaxLength>4096 || Limits.MaxNodes<1 || Limits.MaxNodes>1024 ||
            Limits.MaxDepth<8 || Limits.MaxDepth>512 || Limits.EvaluationBudget<1 || Limits.EvaluationBudget>4096)
        {
            OutError={"表达式预算设置无效",0};
            return false;
        }
        Parser P(Source,Limits);
        Out.Root = P.Parse();
        if (!Out.Root) { OutError = P.GetError(); return false; }
        Out.EvaluationBudget=Limits.EvaluationBudget;
        return true;
    }

    bool Expression::Evaluate(double X, double& OutValue, Error& OutError) const
    {
        OutValue = 0.0; OutError = {};
        int Budget = EvaluationBudget;
        return EvaluateNode(Root, X, OutValue, OutError, Budget);
    }

    bool Expression::Bounds(double From, double To, double& OutMin, double& OutMax) const
    {
        if (!std::isfinite(From) || !std::isfinite(To) || From > To) return false;
        Range Result;
        int Budget = EvaluationBudget;
        if (!BoundNode(Root, {From,To}, Result, Budget)) return false;
        OutMin=Result.Low;
        OutMax=Result.High;
        return true;
    }
}
