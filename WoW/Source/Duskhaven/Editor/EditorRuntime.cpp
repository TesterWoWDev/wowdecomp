#include "EditorRuntime.h"
#include "EditorObject.h"
#include "EditorState.h"
#include "GizmoDraw.h"
#include "GizmoPick.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/Types.h"
#include "Engine/Source/Tempest/C4Quaternion.h"
#include "WoW/Source/Duskhaven/Housing/HousingManager.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/UIUtil/Camera.h"

namespace Duskhaven::Editor::EditorRuntime
{
    using namespace EditorObject;

    namespace
    {
        constexpr float kTranslationGizmoScale = 1.0f;
        constexpr float kRotationGizmoScale    = 1.3f;

        EditorState& State()
        {
            static EditorState state;
            return state;
        }

        bool& Enabled()
        {
            static bool enabled = false;
            return enabled;
        }

        bool& EventsRegistered()
        {
            static bool registered = false;
            return registered;
        }

        bool UpdateMouseRay(const EVENT_DATA_MOUSE* data)
        {
            if (!data)
                return false;

            CGWorldFrame* worldFrame = CGWorldFrame::s_currentWorldFrame;
            if (!worldFrame || !worldFrame->m_camera)
                return false;

            float ddcX = 0.0f;
            float ddcY = 0.0f;
            NDCToDDC(data->m_x, data->m_y, &ddcX, &ddcY);

            CGxDevice* device = g_theGxDevicePtr;
            if (!device)
                return false;

            C44Matrix projection = device->m_projection;
            CGxMatrixStack& stack = device->m_xforms[GxXform_View];
            C44Matrix view = stack.m_mtx[stack.m_level];
            worldFrame->m_camera->SetupWorldProjection(worldFrame->m_viewportDDC);

            bool ok = worldFrame->GetLineSegment(ddcX, ddcY, &State().start, &State().end) != 0;
            if (!ok)
            {
                State().start = {};
                State().end   = {};
            }

            g_theGxDevicePtr->XformSetProjection(projection);
            g_theGxDevicePtr->XformSetView(view);
            return ok;
        }

        int32_t OnMouseDown(const void* rawData, void*)
        {
            if (!Enabled())
                return 1;

            auto const* data = static_cast<const EVENT_DATA_MOUSE*>(rawData);
            if (!data || data->m_button != MOUSE_BUTTON_LEFT)
                return 1;
            if (!UpdateMouseRay(data))
                return 1;

            EditorState& state         = State();
            CGGameObject_C* gameObject = SelectedGameObject();

            if (gameObject && state.gizmoTranslationAxis != Axis::None)
            {
                C3Vector axisDir = AxisDirection(state.gizmoTranslationAxis);
                float t          = ClosestRayAxisParameter(state.start, state.end, state.gizmoPosition, axisDir);

                state.gizmoDragState.active       = true;
                state.gizmoDragState.isRotation   = false;
                state.gizmoDragState.axis         = state.gizmoTranslationAxis;
                state.gizmoDragState.startObjPos  = state.gizmoPosition;
                state.gizmoDragState.anchorOnAxis = VectorMath::Add(state.gizmoPosition, VectorMath::Scale(axisDir, t));
                return 0;
            }

            if (gameObject && state.gizmoRotationAxis != Axis::None)
            {
                C3Vector axis{};
                C3Vector refDir{};
                C3Vector refPerp{};
                RingBasis(state.gizmoRotationAxis, axis, refDir, refPerp);

                float angle = 0.0f;
                if (!RayAngleOnRing(state.start, state.end, state.gizmoPosition, axis, refDir, refPerp, angle))
                    return 1;

                state.gizmoDragState.active      = true;
                state.gizmoDragState.isRotation  = true;
                state.gizmoDragState.axis        = state.gizmoRotationAxis;
                state.gizmoDragState.startObjPos = state.gizmoPosition;
                state.gizmoDragState.anchorAngle = angle;
                state.gizmoDragState.ringAxis    = axis;
                state.gizmoDragState.ringRefDir  = refDir;
                state.gizmoDragState.ringRefPerp = refPerp;
                int64_t packed = reinterpret_cast<const C4QuaternionCompressed*>(&gameObject->m_passenger.m_facing)->m_packed;
                state.gizmoDragState.startRot    = Housing::UnpackQuat(static_cast<uint64_t>(packed));
                return 0;
            }

            return 1;
        }

        int32_t OnMouseUp(const void* rawData, void*)
        {
            auto const* data = static_cast<const EVENT_DATA_MOUSE*>(rawData);
            if (!data || data->m_button != MOUSE_BUTTON_LEFT)
                return 1;

            EditorState& state          = State();
            state.gizmoDragState.active = false;
            state.gizmoDragState.axis   = Axis::None;
            return 1;
        }

        int32_t OnMouseMove(const void* rawData, void*)
        {
            if (!Enabled())
                return 1;

            auto const* data = static_cast<const EVENT_DATA_MOUSE*>(rawData);
            if (!UpdateMouseRay(data))
                return 1;

            EditorState& state         = State();
            CGGameObject_C* gameObject = SelectedGameObject();
            if (!gameObject)
            {
                state.gizmoTranslationAxis  = Axis::None;
                state.gizmoRotationAxis     = Axis::None;
                state.gizmoDragState.active = false;
                return 1;
            }

            if (state.gizmoDragState.active)
            {
                if (!state.gizmoDragState.isRotation)
                {
                    C3Vector axisDir = AxisDirection(state.gizmoDragState.axis);
                    float t =
                        ClosestRayAxisParameter(state.start, state.end, state.gizmoDragState.startObjPos, axisDir);
                    C3Vector currentOnAxis =
                        VectorMath::Add(state.gizmoDragState.startObjPos, VectorMath::Scale(axisDir, t));
                    C3Vector delta      = VectorMath::Subtract(currentOnAxis, state.gizmoDragState.anchorOnAxis);
                    state.gizmoPosition = VectorMath::Add(state.gizmoDragState.startObjPos, delta);

                    gameObject->m_passenger.m_position = state.gizmoPosition;
                    gameObject->UpdateWorldObject(0);
                    return 1;
                }

                float currentAngle = 0.0f;
                if (!RayAngleOnRing(state.start, state.end, state.gizmoDragState.startObjPos,
                                    state.gizmoDragState.ringAxis, state.gizmoDragState.ringRefDir,
                                    state.gizmoDragState.ringRefPerp, currentAngle))
                    return 1;

                float delta   = NormalizeSignedAngleDelta(currentAngle - state.gizmoDragState.anchorAngle);
                Housing::Quat deltaRot = Housing::AxisAngleToQuat(state.gizmoDragState.ringAxis.x,
                                                                 state.gizmoDragState.ringAxis.y,
                                                                 state.gizmoDragState.ringAxis.z,
                                                                 delta);
                Housing::Quat finalRot = Housing::MultiplyQuat(deltaRot, state.gizmoDragState.startRot);
                reinterpret_cast<C4QuaternionCompressed*>(&gameObject->m_passenger.m_facing)->m_packed =
                    static_cast<int64_t>(Housing::PackQuat(finalRot));
                gameObject->UpdateWorldObject(0);
                return 1;
            }

            state.gizmoPosition = gameObject->GetPosition();
            state.gizmoTranslationAxis =
                PickTranslationGizmo(state.start, state.end, state.gizmoPosition, kTranslationGizmoScale);
            state.gizmoRotationAxis =
                PickRotationGizmo(state.start, state.end, state.gizmoPosition, kRotationGizmoScale);
            return 1;
        }
    } // namespace

    CGGameObject_C* SelectedGameObject()
    {
        EditorState& state = State();
        if (state.currentObjectGuid == 0)
            return nullptr;

        return EditorObject::GameObjectByGuid(state.currentObjectGuid);
    }

    uint64_t CurrentSelectedGobGUID()
    {
        EditorState& state = State();
        return state.currentObjectGuid;
    }

    void ClearSelection()
    {
        EditorState& state         = State();
        state.currentObjectGuid    = 0;
        state.gizmoTranslationAxis = Axis::None;
        state.gizmoRotationAxis    = Axis::None;
        state.gizmoDragState       = {};
    }

    bool SelectGameObject(uint64_t guid)
    {
        CGGameObject_C* gameObject = EditorObject::GameObjectByGuid(guid);
        if (!gameObject)
        {
            ClearSelection();
            return false;
        }

        EditorState& state         = State();
        state.currentObjectGuid    = guid;
        state.gizmoPosition        = gameObject->GetPosition();
        state.gizmoTranslationAxis = Axis::None;
        state.gizmoRotationAxis    = Axis::None;
        state.gizmoDragState       = {};
        return true;
    }

    void Apply()
    {
        Enabled() = true;
    }

    void OnGameClientInitialize()
    {
        if (!Enabled() || EventsRegistered())
            return;

        EventRegisterEx(EVENT_ID_MOUSEDOWN, OnMouseDown, nullptr, 10.0f);
        EventRegisterEx(EVENT_ID_MOUSEUP, OnMouseUp, nullptr, 10.0f);
        EventRegisterEx(EVENT_ID_MOUSEMOVE, OnMouseMove, nullptr, 10.0f);
        EventsRegistered() = true;
    }

    void OnGameClientDestroy()
    {
        if (EventsRegistered())
        {
            EventUnregister(EVENT_ID_MOUSEDOWN, OnMouseDown);
            EventUnregister(EVENT_ID_MOUSEUP, OnMouseUp);
            EventUnregister(EVENT_ID_MOUSEMOVE, OnMouseMove);
            EventsRegistered() = false;
        }

        State() = {};
    }

    void OnWorldRender(CGWorldFrame* worldFrame)
    {
        EditorState& state = State();
        if (!Enabled() || !worldFrame || !worldFrame->m_camera || state.currentObjectGuid == 0)
            return;

        CGGameObject_C* gameObject = SelectedGameObject();
        if (!gameObject)
            return;
        if (!state.gizmoDragState.active)
            state.gizmoPosition = gameObject->GetPosition();

        if (!g_theGxDevicePtr)
            return;

        g_theGxDevicePtr->RsPush();
        GxRsSet(GxRs_VertexShader, nullptr);
        GxRsSet(GxRs_PixelShader, nullptr);
        GxRsSet(GxRs_Culling, 0);
        GxRsSet(GxRs_Fog, 0);
        GxRsSet(GxRs_Lighting, 0);

        CImVector transparent(0, 0, 0, 0);
        GxSceneClear(2, transparent);
        GxRsSet(GxRs_DepthTest, 1);

        C3Vector drawPosition = VectorMath::Subtract(state.gizmoPosition, worldFrame->m_camera->m_position);
        GizmoDraw::DrawTranslationGizmo(drawPosition, kTranslationGizmoScale, state.gizmoTranslationAxis);
        GizmoDraw::DrawRotationGizmo(drawPosition, kRotationGizmoScale, state.gizmoRotationAxis);

        g_theGxDevicePtr->RsPop();
    }
} // namespace Duskhaven::Editor::EditorRuntime
