#pragma once

#include "FunctionExpression.h"

#include <string>
#include <vector>

namespace FuncMath
{
    struct TrajectoryPoint
    {
        double Progress = 0.0;
        double Height = 0.0;
    };

    struct TrajectoryOptions
    {
        double MaxProgress = 16.0;
        double InitialStep = 0.20;
        double MinUsableProgress = 0.40;
        double MaxIntervalVariation = 0.20;
        double MaxChordError = 0.025;
        double MinStep = 1.0e-6;
        double MaxAbsHeight = 100000.0;
        double MaxStartMagnitude = 8.0;
        int MaxPoints = 8192;
        int MaxSubdivisions = 20;
        bool UseManualStart = false;
        double ManualStart = 0.0;
    };

    struct Trajectory
    {
        double StartX = 0.0;
        std::vector<TrajectoryPoint> Points;
        bool StoppedEarly = false;
        std::string StopReason;
    };

    // Preview, collision and movement all consume these same immutable points.
    bool BuildTrajectory(const Expression& Expression, const TrajectoryOptions& Options,
                         Trajectory& Out, Error& OutError);
}
