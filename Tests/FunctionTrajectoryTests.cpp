#include "../Source/FuncGame/Math/FunctionTrajectory.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
    int Failures=0;
    void Check(bool Condition, const char* Message)
    {
        if (!Condition) { std::cerr << "FAIL: " << Message << '\n'; ++Failures; }
    }
    FuncMath::Trajectory Build(const char* Source, bool Expected=true)
    {
        FuncMath::Expression E;
        FuncMath::Error Err;
        FuncMath::Trajectory T;
        const bool Compiled=FuncMath::Expression::Compile(Source,E,Err);
        const bool Built=Compiled && FuncMath::BuildTrajectory(E,{},T,Err);
        Check(Built==Expected,Source);
        return T;
    }
}

int main()
{
    const auto Linear=Build("x");
    const auto Shifted=Build("x-2");
    Check(Linear.StartX==0 && Shifted.StartX==0,"linear functions start at zero");
    Check(Linear.Points.size()==Shifted.Points.size(),"translated curves share samples");
    if (Linear.Points.size()==Shifted.Points.size())
        for (std::size_t I=0; I<Linear.Points.size(); ++I)
            Check(std::abs(Linear.Points[I].Height-Shifted.Points[I].Height)<1e-9,"translated curves coincide");
    const auto Log=Build("ln(x)");
    Check(Log.StartX>0 && Log.Points.front().Height==0,"logarithm finds a positive start and translates to origin");
    FuncMath::Expression LogExpression;
    FuncMath::Error Error;
    FuncMath::Trajectory LimitedPath;
    FuncMath::TrajectoryOptions LimitedOptions;
    Check(FuncMath::Expression::Compile("ln(x)",LogExpression,Error),"compile logarithm for start range test");
    LimitedOptions.MaxStartMagnitude=0;
    Check(!FuncMath::BuildTrajectory(LogExpression,LimitedOptions,LimitedPath,Error),"automatic search obeys configured range");
    LimitedOptions.UseManualStart=true;
    LimitedOptions.ManualStart=1;
    Check(FuncMath::BuildTrajectory(LogExpression,LimitedOptions,LimitedPath,Error) && LimitedPath.StartX==1,
          "manual start remains available outside automatic search range");
    const auto Reciprocal=Build("1/x");
    Check(Reciprocal.StartX>0,"reciprocal finds a positive start");
    const auto Pole=Build("1/(x-2)");
    Check(Pole.StoppedEarly && Pole.StartX+Pole.Points.back().Progress<2.0,"reciprocal does not cross its pole");
    const auto Tangent=Build("tan(x)");
    Check(Tangent.StoppedEarly && Tangent.StartX+Tangent.Points.back().Progress<1.5707963267948967,"tangent does not cross its pole");
    const auto Step=Build("floor(x)");
    Check(Step.StoppedEarly && Step.StartX+Step.Points.back().Progress<1.0,"step function does not cross its jump");
    Build("sqrt(-1)",false);
    Build("1/(x-x)",false);
    const auto Piecewise=Build("if(x>=0,sqrt(x),sqrt(-x))");
    Check(Piecewise.StartX==0 && !Piecewise.Points.empty(),"lazy piecewise branch is usable at zero");
    const auto Oscillating=Build("3*sin(5x)");
    Check(!Oscillating.Points.empty(),"oscillating trajectory is sampled");
    Build("sin(10000x)",false);
    const auto IrrationalPole=Build("1/(x-sqrt(2))");
    Check(IrrationalPole.StoppedEarly && IrrationalPole.StartX+IrrationalPole.Points.back().Progress<std::sqrt(2.0),
          "a pole between initial sample points is not crossed");
    if (Failures==0) std::cout << "All trajectory tests passed.\n";
    return Failures==0 ? 0 : 1;
}
