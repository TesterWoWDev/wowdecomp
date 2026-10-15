#include "QuatMath.h"

#include <cmath>
#include <algorithm>

namespace Duskhaven
{
    namespace Housing
    {
        static inline float clampf(float v, float lo, float hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        Quat NormalizeQuat(Quat q)
        {
            float lenSq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
            if (lenSq <= 0.0f)
                return {1.0f, 0.0f, 0.0f, 0.0f};

            float invLen = 1.0f / sqrtf(lenSq);
            return {q.w * invLen, q.x * invLen, q.y * invLen, q.z * invLen};
        }

        Quat NlerpQuat(float ratio, Quat q1, Quat q2)
        {
            float w = (q2.w - q1.w) * ratio + q1.w;
            float x = (q2.x - q1.x) * ratio + q1.x;
            float y = (q2.y - q1.y) * ratio + q1.y;
            float z = (q2.z - q1.z) * ratio + q1.z;

            float magSq = x * x + y * y + z * z + w * w;
            float scale = ((magSq - 0.95906597f) * -0.532516f) + 1.021435f;
            if (magSq <= 0.91521198f)
            {
                scale *= (((scale * scale * magSq) - 0.95906597f) * -0.532516f) + 1.021435f;
                if (magSq <= 0.6521197f)
                    scale *= (((scale * scale * magSq) - 0.95906597f) * -0.532516f) + 1.021435f;
            }

            return {w * scale, x * scale, y * scale, z * scale};
        }

        Quat MultiplyQuat(Quat a, Quat b)
        {
            return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
                    a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                    a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                    a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
        }

        Quat AxisAngleToQuat(float x, float y, float z, float angle)
        {
            float lenSq = x * x + y * y + z * z;
            if (lenSq <= 0.0f)
                return {1.0f, 0.0f, 0.0f, 0.0f};

            float invLen = 1.0f / sqrtf(lenSq);
            float half = angle * 0.5f;
            float s = sinf(half);
            return NormalizeQuat({cosf(half), x * invLen * s, y * invLen * s, z * invLen * s});
        }

        uint64_t PackQuat(Quat q)
        {
            q = NormalizeQuat(q);

            if (q.w < 0.0f)
            {
                q.w = -q.w;
                q.x = -q.x;
                q.y = -q.y;
                q.z = -q.z;
            }

            float x = clampf(q.x, -1.0f, 1.0f);
            float y = clampf(q.y, -1.0f, 1.0f);
            float z = clampf(q.z, -1.0f, 1.0f);

            int xi = (int)(x * 2097152.0f + (x >= 0.0f ? 0.5f : -0.5f));
            int yi = (int)(y * 1048576.0f + (y >= 0.0f ? 0.5f : -0.5f));
            int zi = (int)(z * 1048576.0f + (z >= 0.0f ? 0.5f : -0.5f));

            if (xi > 2097151) xi = 2097151;
            if (xi < -2097152) xi = -2097152;
            if (yi > 1048575) yi = 1048575;
            if (yi < -1048576) yi = -1048576;
            if (zi > 1048575) zi = 1048575;
            if (zi < -1048576) zi = -1048576;

            uint64_t xu = (uint64_t)(unsigned)xi & 0x3FFFFFULL;
            uint64_t yu = (uint64_t)(unsigned)yi & 0x1FFFFFULL;
            uint64_t zu = (uint64_t)(unsigned)zi & 0x1FFFFFULL;

            return (xu << 42) | (yu << 21) | zu;
        }

        Quat EulerToQuat(float roll, float pitch, float yaw)
        {
            float hr = roll * 0.5f;
            float hp = pitch * 0.5f;
            float hy = yaw * 0.5f;

            float cr = cosf(hr), sr = sinf(hr);
            float cp = cosf(hp), sp = sinf(hp);
            float cy = cosf(hy), sy = sinf(hy);

            Quat q;
            q.w = cr * cp * cy + sr * sp * sy;
            q.x = sr * cp * cy - cr * sp * sy;
            q.y = cr * sp * cy + sr * cp * sy;
            q.z = cr * cp * sy - sr * sp * cy;

            return NormalizeQuat(q);
        }

        Quat UnpackQuat(uint64_t packed)
        {
            int xi = (int)((packed >> 42) & 0x3FFFFFULL);
            int yi = (int)((packed >> 21) & 0x1FFFFFULL);
            int zi = (int)(packed & 0x1FFFFFULL);

            if (xi & 0x200000) xi |= ~0x3FFFFF;
            if (yi & 0x100000) yi |= ~0x1FFFFF;
            if (zi & 0x100000) zi |= ~0x1FFFFF;

            float x = (float)xi / 2097152.0f;
            float y = (float)yi / 1048576.0f;
            float z = (float)zi / 1048576.0f;

            float ww = 1.0f - x * x - y * y - z * z;
            float w = ww > 0.0f ? sqrtf(ww) : 0.0f;

            return NormalizeQuat({w, x, y, z});
        }

        void QuatToEuler(Quat q, float& roll, float& pitch, float& yaw)
        {
            q = NormalizeQuat(q);

            float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
            float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
            roll = atan2f(sinr_cosp, cosr_cosp);

            float sinp = 2.0f * (q.w * q.y - q.z * q.x);
            pitch = fabsf(sinp) >= 1.0f ? copysignf(3.14159265358979323846f * 0.5f, sinp) : asinf(sinp);

            float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
            float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
            yaw = atan2f(siny_cosp, cosy_cosp);
        }

        uint64_t AddEulerDeltaToPackedQuat(uint64_t packed, float deltaRoll, float deltaPitch, float deltaYaw)
        {
            Quat current = UnpackQuat(packed);
            Quat delta = EulerToQuat(deltaRoll, deltaPitch, deltaYaw);
            Quat result = MultiplyQuat(current, delta);
            return PackQuat(result);
        }

        uint64_t AddWorldEulerDeltaToPackedQuat(uint64_t packed, float deltaRoll, float deltaPitch, float deltaYaw)
        {
            Quat current = UnpackQuat(packed);
            Quat delta = EulerToQuat(deltaRoll, deltaPitch, deltaYaw);
            return PackQuat(MultiplyQuat(delta, current));
        }

        uint64_t AddAxisDeltaToPackedQuat(uint64_t packed, float axisX, float axisY, float axisZ, float deltaAngle)
        {
            Quat current = UnpackQuat(packed);
            Quat delta = AxisAngleToQuat(axisX, axisY, axisZ, deltaAngle);
            return PackQuat(MultiplyQuat(delta, current));
        }
    } // namespace Housing
} // namespace Duskhaven
