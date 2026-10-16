#pragma once

#include "Engine/Source/Tempest/C3Vector.h"
#include <cfloat>
#include <cmath>
#include <cstdint>

namespace Duskhaven::Editor
{
    namespace VectorMath
    {
        inline float Dot(const C3Vector& a, const C3Vector& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        inline C3Vector Cross(const C3Vector& a, const C3Vector& b)
        {
            return C3Vector::Cross(a, b);
        }

        inline C3Vector Add(const C3Vector& a, const C3Vector& b)
        {
            return a + b;
        }

        inline C3Vector Subtract(const C3Vector& a, const C3Vector& b)
        {
            return a - b;
        }

        inline C3Vector Scale(const C3Vector& a, float s)
        {
            return a * s;
        }

        inline float Length(const C3Vector& a)
        {
            return sqrtf(Dot(a, a));
        }

        inline C3Vector Normalize(const C3Vector& a)
        {
            float len = Length(a);
            if (len > 1e-6f)
                return a * (1.0f / len);
            return {};
        }

        inline C3Vector ProjectionOnPlane(const C3Vector& v, const C3Vector& normal)
        {
            return Subtract(v, Scale(normal, Dot(v, normal)));
        }
    } // namespace VectorMath

    enum class Axis : int32_t
    {
        None = -1,
        X    = 0,
        Y    = 1,
        Z    = 2,
    };

    struct PickResult
    {
        bool hit;
        float tRay;
        float dist;
    };

    inline C3Vector AxisDirection(Axis axis)
    {
        switch (axis)
        {
            case Axis::X:
                return {1.0f, 0.0f, 0.0f};
            case Axis::Y:
                return {0.0f, 1.0f, 0.0f};
            case Axis::Z:
                return {0.0f, 0.0f, 1.0f};
            default:
                return {};
        }
    }

    inline void RingBasis(Axis axis, C3Vector& ringAxis, C3Vector& refDir, C3Vector& refPerp)
    {
        switch (axis)
        {
            case Axis::X:
                ringAxis = {1.0f, 0.0f, 0.0f};
                refDir   = {0.0f, 1.0f, 0.0f};
                refPerp  = {0.0f, 0.0f, 1.0f};
                break;
            case Axis::Y:
                ringAxis = {0.0f, 1.0f, 0.0f};
                refDir   = {0.0f, 0.0f, 1.0f};
                refPerp  = {1.0f, 0.0f, 0.0f};
                break;
            case Axis::Z:
                ringAxis = {0.0f, 0.0f, 1.0f};
                refDir   = {1.0f, 0.0f, 0.0f};
                refPerp  = {0.0f, 1.0f, 0.0f};
                break;
            default:
                ringAxis = {};
                refDir   = {};
                refPerp  = {};
                break;
        }
    }

    inline float ClosestRayAxisParameter(const C3Vector& start, const C3Vector& end,
                                         const C3Vector& axisOrigin, const C3Vector& axisDir)
    {
        C3Vector dir = VectorMath::Normalize(VectorMath::Subtract(end, start));
        C3Vector w0  = VectorMath::Subtract(start, axisOrigin);
        float b      = VectorMath::Dot(dir, axisDir);
        float d      = VectorMath::Dot(dir, w0);
        float e      = VectorMath::Dot(axisDir, w0);
        float denom  = 1.0f - b * b;
        if (denom < 1e-6f)
            return e;

        return (e - b * d) / denom;
    }

    inline bool RayAngleOnRing(const C3Vector& start, const C3Vector& end,
                               const C3Vector& ringOrigin, const C3Vector& axis,
                               const C3Vector& refDir, const C3Vector& refPerp, float& outAngle)
    {
        C3Vector dir = VectorMath::Normalize(VectorMath::Subtract(end, start));
        float denom  = VectorMath::Dot(dir, axis);
        if (fabsf(denom) < 1e-4f)
            return false;

        C3Vector toOrigin = VectorMath::Subtract(ringOrigin, start);
        float tRay        = VectorMath::Dot(toOrigin, axis) / denom;
        if (tRay < 0.0f)
            return false;

        C3Vector hit = VectorMath::Add(start, VectorMath::Scale(dir, tRay));
        C3Vector v   = VectorMath::Subtract(hit, ringOrigin);
        float x      = VectorMath::Dot(v, refDir);
        float y      = VectorMath::Dot(v, refPerp);
        outAngle     = atan2f(y, x);
        return true;
    }

    inline float NormalizeSignedAngleDelta(float delta)
    {
        constexpr float kPi = 3.14159265358979323846f;
        while (delta > kPi)
            delta -= 2.0f * kPi;
        while (delta < -kPi)
            delta += 2.0f * kPi;
        return delta;
    }

    inline PickResult PickAxis(const C3Vector& start, const C3Vector& end,
                               const C3Vector& gizmoOrigin, const C3Vector& axisDir,
                               float length)
    {
        C3Vector dir = VectorMath::Normalize(VectorMath::Subtract(end, start));
        C3Vector w0  = VectorMath::Subtract(start, gizmoOrigin);

        float b     = VectorMath::Dot(dir, axisDir);
        float d     = VectorMath::Dot(dir, w0);
        float e     = VectorMath::Dot(axisDir, w0);
        float denom = 1.0f - b * b;

        float tRay  = 0.0f;
        float tAxis = e;
        if (denom >= 1e-6f)
        {
            tRay  = (b * e - d) / denom;
            tAxis = (e - b * d) / denom;
        }

        if (tRay < 0.0f)
            return {false, 0.0f, 0.0f};

        tAxis = fmaxf(0.0f, fminf(length, tAxis));

        C3Vector pRay  = VectorMath::Add(start, VectorMath::Scale(dir, tRay));
        C3Vector pAxis = VectorMath::Add(gizmoOrigin, VectorMath::Scale(axisDir, tAxis));
        float dist     = VectorMath::Length(VectorMath::Subtract(pRay, pAxis));
        return {true, tRay, dist};
    }

    inline Axis PickTranslationGizmo(const C3Vector& start, const C3Vector& end,
                                     const C3Vector& gizmoOrigin, float scale)
    {
        float length    = 1.0f * scale;
        float tolerance = 0.10f * scale;

        Axis best   = Axis::None;
        float bestT = FLT_MAX;
        Axis axes[] = {Axis::X, Axis::Y, Axis::Z};
        for (Axis axis : axes)
        {
            PickResult result = PickAxis(start, end, gizmoOrigin, AxisDirection(axis), length);
            if (result.hit && result.dist < tolerance && result.tRay < bestT)
            {
                best  = axis;
                bestT = result.tRay;
            }
        }

        return best;
    }

    inline PickResult PickRing(const C3Vector& start, const C3Vector& end,
                               const C3Vector& gizmoOrigin, const C3Vector& axis,
                               float ringRadius, float pickTolerance)
    {
        C3Vector dir = VectorMath::Normalize(VectorMath::Subtract(end, start));
        C3Vector tmp =
            (fabsf(axis.x) < 0.9f) ? C3Vector{1.0f, 0.0f, 0.0f} : C3Vector{0.0f, 1.0f, 0.0f};
        C3Vector side = VectorMath::Normalize(VectorMath::Cross(axis, tmp));
        C3Vector up   = VectorMath::Cross(axis, side);

        constexpr int kSegments = 32;
        constexpr float kPi     = 3.14159265358979323846f;
        float bestDist          = FLT_MAX;
        float bestTRay          = 0.0f;

        C3Vector previous{};
        for (int i = 0; i <= kSegments; ++i)
        {
            float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(kSegments);
            float c     = cosf(angle);
            float s     = sinf(angle);
            C3Vector point = VectorMath::Add(
                gizmoOrigin,
                VectorMath::Scale(VectorMath::Add(VectorMath::Scale(side, c), VectorMath::Scale(up, s)), ringRadius));

            if (i > 0)
            {
                C3Vector segDir = VectorMath::Subtract(point, previous);
                C3Vector w0     = VectorMath::Subtract(start, previous);

                float b     = VectorMath::Dot(dir, segDir);
                float cSeg  = VectorMath::Dot(segDir, segDir);
                float d     = VectorMath::Dot(dir, w0);
                float e     = VectorMath::Dot(segDir, w0);
                float denom = cSeg - b * b;

                float tRay = -d;
                float tSeg = 0.0f;
                if (denom >= 1e-6f)
                {
                    tRay = (b * e - cSeg * d) / denom;
                    tSeg = (e - b * d) / denom;
                }

                if (tRay >= 0.0f)
                {
                    tSeg = fmaxf(0.0f, fminf(1.0f, tSeg));
                    C3Vector pRay = VectorMath::Add(start, VectorMath::Scale(dir, tRay));
                    C3Vector pSeg = VectorMath::Add(previous, VectorMath::Scale(segDir, tSeg));
                    float dist    = VectorMath::Length(VectorMath::Subtract(pRay, pSeg));
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestTRay = tRay;
                    }
                }
            }

            previous = point;
        }

        if (bestDist > pickTolerance)
            return {false, 0.0f, 0.0f};

        return {true, bestTRay, bestDist};
    }

    inline Axis PickRotationGizmo(const C3Vector& start, const C3Vector& end,
                                  const C3Vector& gizmoOrigin, float scale)
    {
        float ringRadius    = 1.0f * scale;
        float pickTolerance = 0.10f * scale;

        Axis best   = Axis::None;
        float bestT = FLT_MAX;
        Axis axes[] = {Axis::X, Axis::Y, Axis::Z};
        for (Axis axis : axes)
        {
            PickResult result = PickRing(start, end, gizmoOrigin, AxisDirection(axis), ringRadius, pickTolerance);
            if (result.hit && result.tRay < bestT)
            {
                best  = axis;
                bestT = result.tRay;
            }
        }

        return best;
    }
} // namespace Duskhaven::Editor
