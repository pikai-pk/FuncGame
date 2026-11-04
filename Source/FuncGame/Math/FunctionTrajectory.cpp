#include "FunctionTrajectory.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace FuncMath
{
    namespace
    {
        class Sampler
        {
        public:
            Sampler(const Expression& InExpression, const TrajectoryOptions& InOptions, double InStart, double InOrigin, Trajectory& InTrajectory)
                : Expr(InExpression), Options(InOptions), Start(InStart), Origin(InOrigin), Result(InTrajectory) {}

            bool Sample(double From, double To, double YFrom, double YTo, int Depth)
            {
                if (static_cast<int>(Result.Points.size()) >= Options.MaxPoints)
                {
                    Result.StopReason = "轨迹采样预算不足";
                    return false;
                }
                double Low=0.0, High=0.0, MidY=0.0;
                Error EvalError;
                const double Middle=(From+To)*0.5;
                const bool Bounded=Expr.Bounds(Start+From, Start+To, Low, High);
                const bool MidValid=Expr.Evaluate(Start+Middle, MidY, EvalError);
                const bool EndValid=std::isfinite(YTo);
                const bool WithinRange=Bounded && std::abs(Low-Origin)<=Options.MaxAbsHeight && std::abs(High-Origin)<=Options.MaxAbsHeight;
                const bool Smooth=WithinRange && MidValid && EndValid &&
                    (High-Low)<=Options.MaxIntervalVariation &&
                    std::abs(MidY-(YFrom+YTo)*0.5)<=Options.MaxChordError;
                if (Smooth)
                {
                    Result.Points.push_back({To,YTo-Origin});
                    return true;
                }
                if (Depth>=Options.MaxSubdivisions || To-From<=Options.MinStep || Middle<=From || Middle>=To)
                {
                    Result.StopReason=Bounded && MidValid && EndValid ? "轨迹精度不足" : "轨迹在定义域或不连续处结束";
                    return false;
                }
                if (!MidValid)
                {
                    // Still retain the safe part before the invalid point.
                    const double Quarter=(From+Middle)*0.5;
                    double QuarterY=0.0;
                    if (!Expr.Evaluate(Start+Quarter,QuarterY,EvalError)) { Result.StopReason="轨迹在定义域或不连续处结束"; return false; }
                    if (!Sample(From,Quarter,YFrom,QuarterY,Depth+1)) return false;
                    return Sample(Quarter,Middle,QuarterY,std::numeric_limits<double>::quiet_NaN(),Depth+1);
                }
                if (!Sample(From,Middle,YFrom,MidY,Depth+1)) return false;
                return Sample(Middle,To,MidY,YTo,Depth+1);
            }

        private:
            const Expression& Expr;
            const TrajectoryOptions& Options;
            double Start;
            double Origin;
            Trajectory& Result;
        };

        bool TryStart(const Expression& Expr, const TrajectoryOptions& Options, double Start, Trajectory& Result, Error& Failure)
        {
            Error Err;
            double Origin=0.0;
            if (!Expr.Evaluate(Start,Origin,Err)) { if (Err.Message=="求值预算已用尽") Failure=Err; return false; }
            if (std::abs(Origin)>Options.MaxAbsHeight) return false;
            Result={};
            Result.StartX=Start;
            Result.Points.push_back({0.0,0.0});
            Sampler S(Expr,Options,Start,Origin,Result);
            double From=0.0,YFrom=Origin;
            while (From<Options.MaxProgress)
            {
                const double To=std::min(Options.MaxProgress,From+Options.InitialStep);
                double YTo=0.0;
                if (!Expr.Evaluate(Start+To,YTo,Err)) YTo=std::numeric_limits<double>::quiet_NaN();
                if (!S.Sample(From,To,YFrom,YTo,0))
                {
                    Result.StoppedEarly=true;
                    break;
                }
                From=To;
                YFrom=YTo;
            }
            return Result.Points.size()>1 && Result.Points.back().Progress>=Options.MinUsableProgress;
        }
    }

    bool BuildTrajectory(const Expression& Expr, const TrajectoryOptions& Options, Trajectory& Out, Error& OutError)
    {
        Out={}; OutError={};
        if (!Expr.IsValid() || Options.MaxProgress<=0 || Options.InitialStep<=0 || Options.MinStep<=0 ||
            Options.MaxPoints<2 || Options.MinUsableProgress<=0 || Options.MinUsableProgress>Options.MaxProgress || Options.MaxStartMagnitude<0)
        {
            OutError={"轨迹设置无效",0};
            return false;
        }
        if (Options.UseManualStart)
        {
            if (TryStart(Expr,Options,Options.ManualStart,Out,OutError)) return true;
            if (OutError.Message.empty()) OutError={"指定起点附近没有足够的连续实数轨迹",0};
            return false;
        }
        const double Candidates[]={0.0,0.01,0.025,0.05,0.1,0.25,0.5,1.0,2.0,4.0,8.0,
                                   -0.01,-0.025,-0.05,-0.1,-0.25,-0.5,-1.0,-2.0,-4.0,-8.0};
        for (double Candidate : Candidates)
        {
            if (std::abs(Candidate)>Options.MaxStartMagnitude) continue;
            if (TryStart(Expr,Options,Candidate,Out,OutError)) { OutError={}; return true; }
        }
        Out={};
        if (OutError.Message.empty()) OutError={"当前搜索范围内找不到足够长的连续实数轨迹",0};
        return false;
    }
}
