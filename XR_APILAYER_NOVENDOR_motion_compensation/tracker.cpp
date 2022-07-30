// Copyright(c) 2022 Sebastian Veith

#include "pch.h"

#include "layer.h"
#include <log.h>
#include <util.h>

using namespace motion_compensation_layer;
using namespace motion_compensation_layer::log;
using namespace xr::math;

TrackerBase::~TrackerBase()
{
    if (m_TransFilter)
    {
        delete m_TransFilter;
    }
    if (m_RotFilter)
    {
        delete m_RotFilter;
    }
}

bool TrackerBase::Init()
{
    return LoadFilters();
}

bool TrackerBase::LazyInit(XrTime time)
{
    m_SkipLazyInit = true;
    return true;
}

bool TrackerBase::LoadFilters()
{
    // set up filters
    int orderTrans = 2, orderRot = 2;
    float strengthTrans = 0.0f, strengthRot = 0.0f;
    if (!GetConfig()->GetInt(Cfg::TransOrder, orderTrans) || !GetConfig()->GetInt(Cfg::RotOrder, orderRot) ||
        !GetConfig()->GetFloat(Cfg::TransStrength, strengthTrans) ||
        !GetConfig()->GetFloat(Cfg::RotStrength, strengthRot))
    {
        ErrorLog("%s: error reading configured values for filters\n", __FUNCTION__);
    }
    if (1 > orderTrans || 3 < orderTrans)
    {
        ErrorLog("%s: invalid order for translational filter: %d\n", __FUNCTION__, orderTrans);
        return false;
    }
    if (1 > orderRot || 3 < orderRot)
    {
        ErrorLog("%s: invalid order for rotational filter: %d\n", __FUNCTION__, orderRot);
        return false;
    }
    // remove previous filter objects
    if (m_TransFilter)
    {
        delete m_TransFilter;
    }
    if (m_RotFilter)
    {
        delete m_RotFilter;
    }

    m_TransStrength = strengthTrans;
    m_RotStrength = strengthRot;

    Log("translational filter stages: %d\n", orderTrans);
    Log("translational filter strength: %f\n", m_TransStrength);
    m_TransFilter = 1 == orderTrans   ? new utility::SingleEmaFilter(m_TransStrength)
                    : 2 == orderTrans ? new utility::DoubleEmaFilter(m_TransStrength)
                                      : new utility::TripleEmaFilter(m_TransStrength);

    Log("rotational filter stages: %d\n", orderRot);
    Log("rotational filter strength: %f\n", m_TransStrength);
    m_RotFilter = 1 == orderRot   ? new utility::SingleSlerpFilter(m_RotStrength)
                  : 2 == orderRot ? new utility::DoubleSlerpFilter(m_RotStrength)
                                  : new utility::TripleSlerpFilter(m_RotStrength);

    return true;
}

void TrackerBase::ModifyFilterStrength(bool trans, bool increase)
{
    float* currentValue = trans ? &m_TransStrength : &m_RotStrength;
    float prevValue = *currentValue;
    float amount = (1.1f - *currentValue) * 0.05f;
    float newValue = *currentValue + (increase ? amount : -amount);
    if (trans)
    {
        *currentValue = m_TransFilter->SetStrength(newValue);
        GetConfig()->SetValue(Cfg::TransStrength, *currentValue);
        Log("translational filter strength %screased to %f\n", increase ? "in" : "de", *currentValue);
    }
    else
    {
        *currentValue = m_RotFilter->SetStrength(newValue);
        GetConfig()->SetValue(Cfg::RotStrength, *currentValue);
        Log("rotational filter strength %screased to %f\n", increase ? "in" : "de", *currentValue);
    }
    MessageBeep(*currentValue != prevValue ? MB_OK : MB_ICONERROR);
}

void TrackerBase::SetReferencePose(XrPosef pose)
{
    m_TransFilter->Reset(pose.position);
    m_RotFilter->Reset(pose.orientation);
    m_ReferencePose = pose;
    m_Calibrated = true;
}

bool TrackerBase::GetPoseDelta(XrPosef& poseDelta, XrSession session, XrTime time)
{
    // pose already calulated for requested time or unable to calculate
    if (time == m_LastPoseTime)
    {
        // already calulated for requested time;
        poseDelta = m_LastPoseDelta;
        TraceLoggingWrite(g_traceProvider, "GetPoseDelta", TLArg(xr::ToString(m_LastPoseDelta).c_str(), "Last_Delta"));
        return true;
    }
    if (m_ResetReferencePose)
    {
        m_ResetReferencePose = !ResetReferencePose(session, time);
    }
    XrPosef curPose;
    if (GetPose(curPose, session, time))
    {
        // apply translational filter
        m_TransFilter->Filter(curPose.position);

        // apply rotational filter
        m_RotFilter->Filter(curPose.orientation);

        TraceLoggingWrite(g_traceProvider,
                          "GetPoseDelta",
                          TLArg(xr::ToString(curPose).c_str(), "Location_After_Filter"),
                          TLArg(time, "Time"));

        // calculate difference toward reference pose
        poseDelta = Pose::Multiply(Pose::Invert(curPose), m_ReferencePose);

        TraceLoggingWrite(g_traceProvider, "GetPoseDelta", TLArg(xr::ToString(poseDelta).c_str(), "Delta"));

        m_LastPoseTime = time;
        m_LastPoseDelta = poseDelta;
        return true;
    }
    else
    {
        return false;
    }
}

OpenXrTracker::OpenXrTracker()
{}

OpenXrTracker::~OpenXrTracker()
{}

bool OpenXrTracker::ResetReferencePose(XrSession session, XrTime time)
{
    XrPosef curPose;
    if (GetPose(curPose, session, time))
    {
        SetReferencePose(curPose);
        return true;
    }
    else
    {
        ErrorLog("%s: unable to get current pose\n", __FUNCTION__);
        m_Calibrated = false;
        return false;
    }
}

bool OpenXrTracker::GetPose(XrPosef& trackerPose, XrSession session, XrTime time)
{
    OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
    if (layer)
    {
        // Query the latest tracker pose.
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        {
            // TODO: skip xrSyncActions if other actions are active as well?
            XrActiveActionSet activeActionSets;
            activeActionSets.actionSet = layer->m_ActionSet;
            activeActionSets.subactionPath = XR_NULL_PATH;

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr};
            syncInfo.activeActionSets = &activeActionSets;
            syncInfo.countActiveActionSets = 1;

            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLPArg(layer->m_ActionSet, "xrSyncActions"),
                              TLArg(time, "Time"));
            CHECK_XRCMD(GetInstance()->xrSyncActions(session, &syncInfo));
        }
        {
            XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
            XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
            getActionStateInfo.action = layer->m_TrackerPoseAction;

            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLPArg(layer->m_TrackerPoseAction, "xrGetActionStatePose"),
                              TLArg(time, "Time"));
            CHECK_XRCMD(GetInstance()->xrGetActionStatePose(session, &getActionStateInfo, &actionStatePose));

            if (!actionStatePose.isActive)
            {
                ErrorLog("%s: unable to determine tracker pose - XrActionStatePose not active\n", __FUNCTION__);
                return false;
            }
        }

        CHECK_XRCMD(GetInstance()->xrLocateSpace(layer->m_TrackerSpace, layer->m_ReferenceSpace, time, &location));

        if (!Pose::IsPoseValid(location.locationFlags))
        {
            ErrorLog("%s: unable to determine tracker pose - XrSpaceLocation not valid\n", __FUNCTION__);
            return false;
        }
        TraceLoggingWrite(g_traceProvider,
                          "GetPose",
                          TLArg(xr::ToString(location.pose).c_str(), "Location"),
                          TLArg(time, "Time"));
        trackerPose = location.pose;

        return true;
    }
    else
    {
        ErrorLog("unable to cast instance to OpenXrLayer\n");
        return false;
    }
}

YawTracker::YawTracker()
{}

YawTracker::~YawTracker()
{}

bool YawTracker::Init()
{
    bool success{true};
    float value;
    if (GetConfig()->GetFloat(Cfg::TrackerOffsetForward, value))
    {
        m_Offset.position.z = -value / 100.0f;
        DebugLog("Offset z = %f\n", m_Offset.position.z);
    }
    else
    {
        success = false;  
    }
    if (GetConfig()->GetFloat(Cfg::TrackerOffsetDown, value))
    {
        m_Offset.position.y = -value / 100.0f;
        DebugLog("Offset y = %f\n", m_Offset.position.y);
    }
    else
    {
        success = false;
    }
    if (GetConfig()->GetFloat(Cfg::TrackerOffsetRight, value))
    {
        m_Offset.position.x = value / 100.0f;
        DebugLog("Offset x = %f\n", m_Offset.position.x);
    }
    else
    {
        success = false;
    }
    if (!TrackerBase::Init())
    {
        success = false;    
    }
    return success;
}

bool YawTracker::LazyInit(XrTime time)
{
    bool success = true;
    if (!m_SkipLazyInit)
    {
        m_Mmf = memory_mapped_file::read_only_mmf("YawVRGEFile");
        // TODO: activate
        if (true || m_Mmf.is_open())
        {
            
        }
        else
        {
            ErrorLog("unable to open mmf 'YawVRGEFile'. Check if Game Engine is running and motion compensation is "
                     "activated!\n");
            success = false;
        }
    }
    m_SkipLazyInit = success;
    return success;
}

bool YawTracker::ResetReferencePose(XrSession session, XrTime time)
{
    // TODO: determine reference pose using saved pose in stage
    bool success = true;
    OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
    if (layer)
    {
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        if (XR_SUCCEEDED(layer->OpenXrApi::xrLocateSpace(layer->m_ViewSpace, layer->m_ReferenceSpace, time, &location)))
        {
            // TODO: eliminate pitch and roll before applying offset
            XrPosef refPose = Pose::Multiply(m_Offset, location.pose);
            // TODO: remove rotation offset before using mmf
            XrPosef controllerPose;
            if (GetControllerPose(controllerPose, session, time))
            {
                refPose.orientation = controllerPose.orientation;
            }
            TrackerBase::SetReferencePose(refPose);
        }
        else
        {
            ErrorLog("%s: xrLocateSpace(view) failed\n", __FUNCTION__);
            success = false;
        }
    }
    else
    {
        ErrorLog("%s: cast of layer failed\n", __FUNCTION__);
        success = false;
    }

    m_Calibrated = success;
    return success;
}

bool YawTracker::GetPose(XrPosef& trackerPose, XrSession session, XrTime time)
{
    // TODO: use mmf and apply to reference pose
    bool success = GetControllerPose(trackerPose, session, time);
    if (success)
    {
        // remove translation towards reference pose
        StoreXrVector3(&trackerPose.position,
                       DirectX::XMVector3Rotate(LoadXrVector3(m_ReferencePose.position),
                                                LoadXrQuaternion(m_ReferencePose.orientation)));
    }
    return success;
}

bool YawTracker::GetControllerPose(XrPosef& trackerPose, XrSession session, XrTime time)
{
    // todo read pose from mmf and apply to reference pose
    OpenXrLayer* layer = reinterpret_cast<OpenXrLayer*>(GetInstance());
    if (layer)
    {
        // Query the latest tracker pose.
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
        {
            // TODO: skip xrSyncActions if other actions are active as well?
            XrActiveActionSet activeActionSets;
            activeActionSets.actionSet = layer->m_ActionSet;
            activeActionSets.subactionPath = XR_NULL_PATH;

            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr};
            syncInfo.activeActionSets = &activeActionSets;
            syncInfo.countActiveActionSets = 1;

            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLPArg(layer->m_ActionSet, "xrSyncActions"),
                              TLArg(time, "Time"));
            CHECK_XRCMD(GetInstance()->xrSyncActions(session, &syncInfo));
        }
        {
            XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
            XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
            getActionStateInfo.action = layer->m_TrackerPoseAction;

            TraceLoggingWrite(g_traceProvider,
                              "GetPose",
                              TLPArg(layer->m_TrackerPoseAction, "xrGetActionStatePose"),
                              TLArg(time, "Time"));
            CHECK_XRCMD(GetInstance()->xrGetActionStatePose(session, &getActionStateInfo, &actionStatePose));

            if (!actionStatePose.isActive)
            {
                ErrorLog("%s: unable to determine tracker pose - XrActionStatePose not active\n", __FUNCTION__);
                return false;
            }
        }

        CHECK_XRCMD(GetInstance()->xrLocateSpace(layer->m_TrackerSpace, layer->m_ReferenceSpace, time, &location));

        if (!Pose::IsPoseValid(location.locationFlags))
        {
            ErrorLog("%s: unable to determine tracker pose - XrSpaceLocation not valid\n", __FUNCTION__);
            return false;
        }
        TraceLoggingWrite(g_traceProvider,
                          "GetPose",
                          TLArg(xr::ToString(location.pose).c_str(), "Location"),
                          TLArg(time, "Time"));

        trackerPose = location.pose;
        return true;
    }
    else
    {
        ErrorLog("unable to cast instance to OpenXrLayer\n");
        return false;
    }
}

void GetTracker(TrackerBase** tracker)
{
    TrackerBase* previousTracker = *tracker;
    std::string trackerType;
    if (GetConfig()->GetString(Cfg::TrackerType, trackerType))
    {
        if ("yaw" == trackerType)
        {
            Log("using yaw mapped memory file as tracker\n");
            if (previousTracker)
            {
                delete previousTracker;
            }
            *tracker = new YawTracker();
            return;
        }
        else if ("controller" == trackerType)
        {
            Log("using motion cotroller as tracker\n");
            if (previousTracker)
            {
                delete previousTracker;
            }
            *tracker = new OpenXrTracker();
            return;
        }
        else
        {
            ErrorLog("unknown tracker type: %s\n", trackerType.c_str());
        }
    }
    else
    {
        ErrorLog("unable to determine tracker type, defaulting to 'controller'\n");
    }
    if (previousTracker)
    {
        ErrorLog("retaining previous tracker type\n");
        return;
    }
    ErrorLog("defaulting to 'controller'\n");
    *tracker = new OpenXrTracker();
}