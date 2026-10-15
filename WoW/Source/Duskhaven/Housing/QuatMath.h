#pragma once

#include <cstdint>

namespace Duskhaven
{
    namespace Housing
    {
        struct Quat
        {
            float w, x, y, z;
        };

        Quat NormalizeQuat(Quat q);
        Quat NlerpQuat(float ratio, Quat q1, Quat q2);
        Quat MultiplyQuat(Quat a, Quat b);
        Quat AxisAngleToQuat(float x, float y, float z, float angle);
        uint64_t PackQuat(Quat q);
        Quat UnpackQuat(uint64_t packed);
        Quat EulerToQuat(float roll, float pitch, float yaw);
        void QuatToEuler(Quat q, float& roll, float& pitch, float& yaw);
        uint64_t AddEulerDeltaToPackedQuat(uint64_t packed, float deltaRoll, float deltaPitch, float deltaYaw);
        uint64_t AddWorldEulerDeltaToPackedQuat(uint64_t packed, float deltaRoll, float deltaPitch, float deltaYaw);
        uint64_t AddAxisDeltaToPackedQuat(uint64_t packed, float axisX, float axisY, float axisZ, float deltaAngle);
    } // namespace Housing
} // namespace Duskhaven
