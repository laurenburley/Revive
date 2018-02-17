#include "Session.h"
#include "TextureBase.h"
#include "CompositorD3D.h"
#include "FrameList.h"
#include "Win32Window.h"

#include "OVR_CAPI.h"
#include "OVR_Version.h"
#include "Extras/OVR_Math.h"
#include "REM_MATH.h"
#include "microprofile.h"

#include <Windows.h>
#include <list>
#include <algorithm>
#include <assert.h>
#include <dxgi.h>

#include <winrt/Windows.Foundation.h>
using namespace winrt::Windows::Foundation;

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

#include <winrt/Windows.Perception.h>
using namespace winrt::Windows::Perception;

#include <winrt/Windows.Perception.Spatial.h>
using namespace winrt::Windows::Perception::Spatial;

#if 0
#define REM_TRACE(x) OutputDebugStringA(#x "\n");
#else
#define REM_TRACE(x) MICROPROFILE_SCOPEI("Revive", #x, 0xff0000);
#endif
#define REM_DEFAULT_TIMEOUT 10000
#define REM_HAPTICS_SAMPLE_RATE 320
#define REM_DEFAULT_IPD 62.715f

uint32_t g_MinorVersion = OVR_MINOR_VERSION;
std::list<ovrHmdStruct> g_Sessions;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Initialize(const ovrInitParams* params)
{
	MicroProfileOnThreadCreate("Main");
	MicroProfileSetForceEnable(true);
	MicroProfileSetEnableAllGroups(true);
	MicroProfileSetForceMetaCounters(true);
	MicroProfileWebServerStart();

	winrt::init_apartment(winrt::apartment_type::single_threaded);

	g_MinorVersion = params->RequestedMinorVersion;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_Shutdown()
{
	g_Sessions.clear();
	winrt::clear_factory_cache();
	winrt::uninit_apartment();
}

OVR_PUBLIC_FUNCTION(void) ovr_GetLastErrorInfo(ovrErrorInfo* errorInfo)
{
	REM_TRACE(ovr_GetLastErrorInfo);

	if (!errorInfo)
		return;

	return;
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetVersionString()
{
	REM_TRACE(ovr_GetVersionString);

	return OVR_VERSION_STRING;
}

OVR_PUBLIC_FUNCTION(int) ovr_TraceMessage(int level, const char* message) { return 0; /* Debugging feature */ }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_IdentifyClient(const char* identity) { return ovrSuccess; /* Debugging feature */ }

OVR_PUBLIC_FUNCTION(ovrHmdDesc) ovr_GetHmdDesc(ovrSession session)
{
	REM_TRACE(ovr_GetHmdDesc);

	ovrHmdDesc desc = {};
	desc.Type = HolographicSpace::IsAvailable() ? ovrHmd_CV1 : ovrHmd_None;

	if (!session)
		return desc;

	HolographicDisplay display = HolographicDisplay::GetDefault();

	// Get HMD name
	strncpy(desc.ProductName, "Oculus Rift", 64);
	std::wstring_view name = display.DisplayName();
	wcstombs(desc.Manufacturer, name.data(), 64);
	desc.Manufacturer[63] = '\0';

	// TODO: Get HID information
	desc.VendorId = 0;
	desc.ProductId = 0;

	// TODO: Get serial number
	desc.SerialNumber[0] = '\0';

	// TODO: Get firmware version
	desc.FirmwareMajor = 0;
	desc.FirmwareMinor = 0;

	// Get capabilities
	desc.AvailableHmdCaps = 0;
	desc.DefaultHmdCaps = 0;
	desc.AvailableTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection;
	if (display.SpatialLocator().Locatability() == SpatialLocatability::PositionalTrackingActive)
		desc.AvailableTrackingCaps |= ovrTrackingCap_Position;
	desc.DefaultTrackingCaps = ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection | ovrTrackingCap_Position;

	// Get the render target size
	Size size = display.MaxViewportSize();
	HolographicCameraPose pose = session->Frames->GetPose();
	HolographicStereoTransform transform = pose.ProjectionTransform();
	Rect viewport = pose.Viewport();

	// Update the render descriptors
	for (int i = 0; i < ovrEye_Count; i++)
	{
		ovrEyeRenderDesc eyeDesc = {};

		REM::Matrix4f matrix = i == ovrEye_Left ? REM::Matrix4f(transform.Left) : REM::Matrix4f(transform.Right);
		OVR::FovPort eyeFov(1.0f / matrix.M[1][1], 1.0f / matrix.M[1][1], 1.0f / matrix.M[0][0], 1.0f / matrix.M[0][0]);

		eyeDesc.Eye = (ovrEyeType)i;
		eyeDesc.Fov = eyeFov;

		eyeDesc.DistortedViewport = OVR::Recti((int)viewport.X, (int)viewport.Y, (int)viewport.Width, (int)viewport.Height);
		eyeDesc.PixelsPerTanAngleAtCenter = OVR::Vector2f(size.Width * (MATH_FLOAT_PIOVER4 / eyeFov.GetHorizontalFovRadians()),
			size.Height * (MATH_FLOAT_PIOVER4 / eyeFov.GetVerticalFovRadians()));

		eyeDesc.HmdToEyePose = OVR::Posef::Identity();
		if (i == ovrEye_Right)
		{
			OVR::Matrix4f offset = REM::Matrix4f(transform.Left).Inverted() * REM::Matrix4f(transform.Right);
			//eyeDesc.HmdToEyePose.Orientation = OVR::Quatf(offset);
			eyeDesc.HmdToEyePose.Position = offset.GetTranslation();
		}

		// Update the HMD descriptor
		desc.DefaultEyeFov[i] = eyeFov;
		desc.MaxEyeFov[i] = eyeFov;
	}

	// Get the display properties
	desc.Resolution = OVR::Sizei((int)size.Width * 2, (int)size.Height);
	desc.DisplayRefreshRate = (float)display.RefreshRate();

	return desc;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackerCount(ovrSession session)
{
	REM_TRACE(ovr_GetTrackerCount);

	if (!session)
		return ovrError_InvalidSession;

	return 2; // Spoofing sensors
}

OVR_PUBLIC_FUNCTION(ovrTrackerDesc) ovr_GetTrackerDesc(ovrSession session, unsigned int trackerDescIndex)
{
	REM_TRACE(ovr_GetTrackerDesc);

	if (!session)
		return ovrTrackerDesc();

	return ovrTrackerDesc();
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Create(ovrSession* pSession, ovrGraphicsLuid* pLuid)
{
	REM_TRACE(ovr_Create);

	if (!pSession)
		return ovrError_InvalidParameter;

	*pSession = nullptr;

	// Initialize the opaque pointer with our own MR-specific struct
	g_Sessions.emplace_back();
	ovrSession session = &g_Sessions.back();

	session->Window.reset(new Win32Window());
	session->Compositor.reset(CompositorD3D::Create());
	if (!session->Compositor)
		return ovrError_RuntimeException;

	try
	{
		session->Space = HolographicSpace::CreateForHWND(session->Window->GetWindowHandle());
		session->Space.SetDirect3D11Device(session->Compositor->GetDevice());
		session->Reference = SpatialLocator::GetDefault().CreateStationaryFrameOfReferenceAtCurrentLocation();
		session->Frames.reset(new FrameList(session->Space));
	}
	catch (winrt::hresult_invalid_argument& ex)
	{
		OutputDebugStringW(ex.message().c_str());
		OutputDebugStringW(L"\n");
		return ovrError_RuntimeException;
	}

	HolographicAdapterId luid = session->Space.PrimaryAdapterId();
	memcpy(&pLuid->Reserved[0], &luid.LowPart, sizeof(luid.LowPart));
	memcpy(&pLuid->Reserved[4], &luid.HighPart, sizeof(luid.HighPart));

	session->Window->Show();

	*pSession = session;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_Destroy(ovrSession session)
{
	REM_TRACE(ovr_Destroy);

	// Delete the session from the list of sessions
	g_Sessions.erase(std::find_if(g_Sessions.begin(), g_Sessions.end(), [session](ovrHmdStruct const& o) { return &o == session; }));
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetSessionStatus(ovrSession session, ovrSessionStatus* sessionStatus)
{
	REM_TRACE(ovr_GetSessionStatus);

	if (!session)
		return ovrError_InvalidSession;

	if (!sessionStatus)
		return ovrError_InvalidParameter;

	// Detect if the application has focus, but only return false the first time the status is requested.
	// If this is true from the first call then Airmech will assume the Health-and-Safety warning
	// is still being displayed.
	static bool firstCall = true;
	sessionStatus->IsVisible = !firstCall;
	firstCall = false;

	// Don't use the activity level while debugging, so I don't have to put on the HMD
	sessionStatus->HmdPresent = HolographicSpace::IsAvailable();
	sessionStatus->HmdMounted = sessionStatus->HmdPresent;

	// TODO: Detect these bits
	sessionStatus->DisplayLost = false;
	sessionStatus->ShouldQuit = false;
	sessionStatus->ShouldRecenter = false;
	sessionStatus->HasInputFocus = true;
	sessionStatus->OverlayPresent = false;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetTrackingOriginType(ovrSession session, ovrTrackingOrigin origin)
{
	REM_TRACE(ovr_SetTrackingOriginType);

	if (!session)
		return ovrError_InvalidSession;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrTrackingOrigin) ovr_GetTrackingOriginType(ovrSession session)
{
	REM_TRACE(ovr_GetTrackingOriginType);

	if (!session)
		return ovrTrackingOrigin_EyeLevel;

	return ovrTrackingOrigin_EyeLevel;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RecenterTrackingOrigin(ovrSession session)
{
	REM_TRACE(ovr_RecenterTrackingOrigin);

	if (!session)
		return ovrError_InvalidSession;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SpecifyTrackingOrigin(ovrSession session, ovrPosef originPose)
{
	// TODO: Implement through ApplyTransform()
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_ClearShouldRecenterFlag(ovrSession session) { /* No such flag, do nothing */ }

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingState(ovrSession session, double absTime, ovrBool latencyMarker)
{
	REM_TRACE(ovr_GetTrackingState);

	ovrTrackingState state = { 0 };

	if (!session)
		return state;

	SpatialLocator locator = SpatialLocator::GetDefault();
	state.HeadPose.ThePose = OVR::Posef::Identity();
	state.HeadPose.TimeInSeconds = absTime;

	// Even though it's called FromHistoricalTargetTime() it seems to accept future target times as well.
	// This is important as it's the only convenient way to go back from DateTime to PerceptionTimestamp.
	// TODO: This can be replaced by GetFrameAtTime() now.
	DateTime target(TimeSpan((int64_t)(absTime * 1.0e+7)));
	PerceptionTimestamp timestamp = PerceptionTimestampHelper::FromHistoricalTargetTime(target);

	SpatialLocation location = locator.TryLocateAtTimestamp(timestamp, session->Reference.CoordinateSystem());
	if (location)
	{
		// TODO: Figure out a good way to convert the angular quaternions to vectors.
		//state.HeadPose.AngularVelocity = REM::Quatf(location.AbsoluteAngularVelocity());
		state.HeadPose.LinearVelocity = REM::Vector3f(location.AbsoluteLinearVelocity());
		//state.HeadPose.AngularAcceleration = REM::Quatf(location.AbsoluteAngularAcceleration());
		state.HeadPose.LinearAcceleration = REM::Vector3f(location.AbsoluteLinearAcceleration());
	}

	HolographicFrame frame = session->Frames->GetFrameAtTime(absTime);
	HolographicFramePrediction prediction = frame.CurrentPrediction();
	HolographicCameraPose pose = prediction.CameraPoses().GetAt(0);
	HolographicStereoTransform transform = pose.ProjectionTransform();
	REM::Matrix4f leftEye(transform.Left);
	leftEye.M[0][1] = 0.0f;
	leftEye.M[0][2] = 0.0f;
	leftEye.M[1][2] = 0.0f;
	state.HeadPose.ThePose.Orientation = REM::Quatf(leftEye);
	state.HeadPose.ThePose.Position = leftEye.GetTranslation();

	// TODO: Get the hand tracking state
	state.HandPoses[ovrHand_Left].ThePose = OVR::Posef::Identity();
	state.HandPoses[ovrHand_Right].ThePose = OVR::Posef::Identity();
	return state;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetDevicePoses(ovrSession session, ovrTrackedDeviceType* deviceTypes, int deviceCount, double absTime, ovrPoseStatef* outDevicePoses)
{
	REM_TRACE(ovr_GetDevicePoses);

	if (!session)
		return ovrError_InvalidSession;

	// TODO: Get the device poses
	return ovrSuccess;
}

struct ovrSensorData_;
typedef struct ovrSensorData_ ovrSensorData;

OVR_PUBLIC_FUNCTION(ovrTrackingState) ovr_GetTrackingStateWithSensorData(ovrSession session, double absTime, ovrBool latencyMarker, ovrSensorData* sensorData)
{
	REM_TRACE(ovr_GetTrackingStateWithSensorData);

	// This is a private API, ignore the raw sensor data request and hope for the best.
	assert(sensorData == nullptr);

	return ovr_GetTrackingState(session, absTime, latencyMarker);
}

OVR_PUBLIC_FUNCTION(ovrTrackerPose) ovr_GetTrackerPose(ovrSession session, unsigned int trackerPoseIndex)
{
	REM_TRACE(ovr_GetTrackerPose);

	ovrTrackerPose tracker = { 0 };

	if (!session)
		return tracker;

	tracker.LeveledPose = OVR::Posef::Identity();
	tracker.Pose = OVR::Posef::Identity();
	tracker.TrackerFlags = ovrTracker_Connected;
	return tracker;
}

// Pre-1.7 input state
typedef struct ovrInputState1_
{
	double              TimeInSeconds;
	unsigned int        Buttons;
	unsigned int        Touches;
	float               IndexTrigger[ovrHand_Count];
	float               HandTrigger[ovrHand_Count];
	ovrVector2f         Thumbstick[ovrHand_Count];
	ovrControllerType   ControllerType;
} ovrInputState1;

// Pre-1.11 input state
typedef struct ovrInputState2_
{
	double              TimeInSeconds;
	unsigned int        Buttons;
	unsigned int        Touches;
	float               IndexTrigger[ovrHand_Count];
	float               HandTrigger[ovrHand_Count];
	ovrVector2f         Thumbstick[ovrHand_Count];
	ovrControllerType   ControllerType;
	float               IndexTriggerNoDeadzone[ovrHand_Count];
	float               HandTriggerNoDeadzone[ovrHand_Count];
	ovrVector2f         ThumbstickNoDeadzone[ovrHand_Count];
} ovrInputState2;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetInputState(ovrSession session, ovrControllerType controllerType, ovrInputState* inputState)
{
	REM_TRACE(ovr_GetInputState);

	if (!session)
		return ovrError_InvalidSession;

	if (!inputState)
		return ovrError_InvalidParameter;

	ovrInputState state = { 0 };

	// We need to make sure we don't write outside of the bounds of the struct
	// when the client expects a pre-1.7 version of LibOVR.
	if (g_MinorVersion < 7)
		memcpy(inputState, &state, sizeof(ovrInputState1));
	else if (g_MinorVersion < 11)
		memcpy(inputState, &state, sizeof(ovrInputState2));
	else
		memcpy(inputState, &state, sizeof(ovrInputState));

	// TODO: Get the input state
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetConnectedControllerTypes(ovrSession session)
{
	REM_TRACE(ovr_GetConnectedControllerTypes);

	// TODO: Get the connected controllers
	return ovrControllerType_Touch;
}

OVR_PUBLIC_FUNCTION(ovrTouchHapticsDesc) ovr_GetTouchHapticsDesc(ovrSession session, ovrControllerType controllerType)
{
	REM_TRACE(ovr_GetTouchHapticsDesc);

	ovrTouchHapticsDesc desc = { 0 };

	if (controllerType & ovrControllerType_Touch)
	{
		desc.SampleRateHz = REM_HAPTICS_SAMPLE_RATE;
		desc.SampleSizeInBytes = sizeof(uint8_t);
		desc.SubmitMaxSamples = OVR_HAPTICS_BUFFER_SAMPLES_MAX;
		desc.SubmitMinSamples = 1;
		desc.SubmitOptimalSamples = 20;
		desc.QueueMinSizeToAvoidStarvation = 5;
	}

	return desc;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetControllerVibration(ovrSession session, ovrControllerType controllerType, float frequency, float amplitude)
{
	REM_TRACE(ovr_SetControllerVibration);

	if (!session)
		return ovrError_InvalidSession;

	// TODO: Vibrate controllers
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitControllerVibration(ovrSession session, ovrControllerType controllerType, const ovrHapticsBuffer* buffer)
{
	REM_TRACE(ovr_SubmitControllerVibration);

	if (!session)
		return ovrError_InvalidSession;

	// TODO: Vibrate controllers
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetControllerVibrationState(ovrSession session, ovrControllerType controllerType, ovrHapticsPlaybackState* outState)
{
	REM_TRACE(ovr_GetControllerVibrationState);

	if (!session)
		return ovrError_InvalidSession;

	// TODO: Vibrate controllers
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_TestBoundary(ovrSession session, ovrTrackedDeviceType deviceBitmask,
	ovrBoundaryType boundaryType, ovrBoundaryTestResult* outTestResult)
{
	REM_TRACE(ovr_TestBoundary);

	// TODO: Boundary support
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_TestBoundaryPoint(ovrSession session, const ovrVector3f* point,
	ovrBoundaryType singleBoundaryType, ovrBoundaryTestResult* outTestResult)
{
	REM_TRACE(ovr_TestBoundaryPoint);

	// TODO: Boundary support
	return ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetBoundaryLookAndFeel(ovrSession session, const ovrBoundaryLookAndFeel* lookAndFeel)
{
	REM_TRACE(ovr_SetBoundaryLookAndFeel);

	// TODO: Boundary support
	return ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_ResetBoundaryLookAndFeel(ovrSession session)
{
	REM_TRACE(ovr_ResetBoundaryLookAndFeel);

	// TODO: Boundary support
	return ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryGeometry(ovrSession session, ovrBoundaryType boundaryType, ovrVector3f* outFloorPoints, int* outFloorPointsCount)
{
	REM_TRACE(ovr_GetBoundaryGeometry);

	// TODO: Boundary support
	return ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryDimensions(ovrSession session, ovrBoundaryType boundaryType, ovrVector3f* outDimensions)
{
	REM_TRACE(ovr_GetBoundaryDimensions);

	// TODO: Boundary support
	return ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetBoundaryVisible(ovrSession session, ovrBool* outIsVisible)
{
	REM_TRACE(ovr_GetBoundaryVisible);

	// TODO: Boundary support
	return ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_RequestBoundaryVisible(ovrSession session, ovrBool visible)
{
	REM_TRACE(ovr_RequestBoundaryVisible);

	// TODO: Boundary support
	return ovrSuccess_BoundaryInvalid;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainLength(ovrSession session, ovrTextureSwapChain chain, int* out_Length)
{
	REM_TRACE(ovr_GetTextureSwapChainLength);

	if (!chain || !out_Length)
		return ovrError_InvalidParameter;

	*out_Length = chain->Length;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainCurrentIndex(ovrSession session, ovrTextureSwapChain chain, int* out_Index)
{
	REM_TRACE(ovr_GetTextureSwapChainCurrentIndex);

	if (!chain || !out_Index)
		return ovrError_InvalidParameter;

	*out_Index = chain->CurrentIndex;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainDesc(ovrSession session, ovrTextureSwapChain chain, ovrTextureSwapChainDesc* out_Desc)
{
	REM_TRACE(ovr_GetTextureSwapChainDesc);

	if (!chain || !out_Desc)
		return ovrError_InvalidParameter;

	*out_Desc = chain->Desc;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CommitTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	REM_TRACE(ovr_CommitTextureSwapChain);

	if (!chain)
		return ovrError_InvalidParameter;

	if (chain->Full())
		return ovrError_TextureSwapChainFull;

	chain->Commit();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyTextureSwapChain(ovrSession session, ovrTextureSwapChain chain)
{
	REM_TRACE(ovr_DestroyTextureSwapChain);

	if (!chain)
		return;

	delete chain;
}

OVR_PUBLIC_FUNCTION(void) ovr_DestroyMirrorTexture(ovrSession session, ovrMirrorTexture mirrorTexture)
{
	REM_TRACE(ovr_DestroyMirrorTexture);

	if (!mirrorTexture)
		return;

	session->Compositor->SetMirrorTexture(nullptr);
	delete mirrorTexture;
}

OVR_PUBLIC_FUNCTION(ovrSizei) ovr_GetFovTextureSize(ovrSession session, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel)
{
	REM_TRACE(ovr_GetFovTextureSize);

	HolographicCameraPose pose = session->Frames->GetPose();
	HolographicCamera cam = pose.HolographicCamera();

	cam.ViewportScaleFactor(std::min(pixelsPerDisplayPixel, 1.0f));
	Size size = cam.RenderTargetSize();
	return OVR::Sizei((int)size.Width, (int)size.Height);
}

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc) ovr_GetRenderDesc2(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	REM_TRACE(ovr_GetRenderDesc);

	ovrEyeRenderDesc desc = {};

	HolographicDisplay display = HolographicDisplay::GetDefault();
	Size size = display.MaxViewportSize();

	// TODO: Get the FOV, currently we just assume 105 degrees
	OVR::FovPort eyeFov(fov);
	desc.Eye = eyeType;
	desc.DistortedViewport = OVR::Recti(eyeType == ovrEye_Right ? (int)size.Width : 0, 0, (int)size.Width, (int)size.Height);
	desc.Fov = eyeFov;
	desc.PixelsPerTanAngleAtCenter = OVR::Vector2f(size.Width * (MATH_FLOAT_PIOVER4 / eyeFov.GetHorizontalFovRadians()),
		size.Height * (MATH_FLOAT_PIOVER4 / eyeFov.GetVerticalFovRadians()));

	HolographicCameraPose pose = session->Frames->GetPose();
	HolographicStereoTransform transform = pose.ProjectionTransform();
	desc.HmdToEyePose = OVR::Posef::Identity();
	if (eyeType == ovrEye_Right)
	{
		OVR::Matrix4f offset = REM::Matrix4f(transform.Left).Inverted() * REM::Matrix4f(transform.Right);
		//desc.HmdToEyePose.Orientation = OVR::Quatf(offset);
		desc.HmdToEyePose.Position = offset.GetTranslation();
	}
	return desc;
}

typedef struct OVR_ALIGNAS(4) ovrEyeRenderDesc1_ {
	ovrEyeType Eye;
	ovrFovPort Fov;
	ovrRecti DistortedViewport;
	ovrVector2f PixelsPerTanAngleAtCenter;
	ovrVector3f HmdToEyeOffset;
} ovrEyeRenderDesc1;

OVR_PUBLIC_FUNCTION(ovrEyeRenderDesc1) ovr_GetRenderDesc(ovrSession session, ovrEyeType eyeType, ovrFovPort fov)
{
	ovrEyeRenderDesc1 legacy = {};
	ovrEyeRenderDesc desc = ovr_GetRenderDesc2(session, eyeType, fov);
	memcpy(&legacy, &desc, sizeof(ovrEyeRenderDesc1));
	legacy.HmdToEyeOffset = desc.HmdToEyePose.Position;
	return legacy;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_WaitToBeginFrame(ovrSession session, long long frameIndex)
{
	REM_TRACE(ovr_WaitToBeginFrame);

	if (!session || !session->Compositor)
		return ovrError_InvalidSession;

	return session->Compositor->WaitToBeginFrame(session, frameIndex);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_BeginFrame(ovrSession session, long long frameIndex)
{
	REM_TRACE(ovr_BeginFrame);

	if (!session || !session->Compositor)
		return ovrError_InvalidSession;

	return session->Compositor->BeginFrame(session, frameIndex);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_EndFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REM_TRACE(ovr_EndFrame);

	if (!session || !session->Compositor)
		return ovrError_InvalidSession;

	// Use our own intermediate compositor to convert the frame to OpenVR.
	return session->Compositor->EndFrame(session, frameIndex, layerPtrList, layerCount);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame2(ovrSession session, long long frameIndex, const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	REM_TRACE(ovr_SubmitFrame);

	if (!session || !session->Compositor)
		return ovrError_InvalidSession;

	// Use our own intermediate compositor to convert the frame to OpenVR.
	ovrResult result = session->Compositor->EndFrame(session, frameIndex, layerPtrList, layerCount);

	// Wait for the current frame to finish
	session->Compositor->WaitToBeginFrame(session, frameIndex);

	// Begin the next frame
	session->Compositor->BeginFrame(session, frameIndex + 1);

	return result;
}

typedef struct OVR_ALIGNAS(4) ovrViewScaleDesc1_ {
	ovrVector3f HmdToEyeOffset[ovrEye_Count]; ///< Translation of each eye.
	float HmdSpaceToWorldScaleInMeters; ///< Ratio of viewer units to meter units.
} ovrViewScaleDesc1;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SubmitFrame(ovrSession session, long long frameIndex, const ovrViewScaleDesc1* viewScaleDesc,
	ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	// TODO: We don't ever use viewScaleDesc so no need to do any conversion.
	return ovr_SubmitFrame2(session, frameIndex, nullptr, layerPtrList, layerCount);
}

typedef struct OVR_ALIGNAS(4) ovrPerfStatsPerCompositorFrame1_
{
	int     HmdVsyncIndex;
	int     AppFrameIndex;
	int     AppDroppedFrameCount;
	float   AppMotionToPhotonLatency;
	float   AppQueueAheadTime;
	float   AppCpuElapsedTime;
	float   AppGpuElapsedTime;
	int     CompositorFrameIndex;
	int     CompositorDroppedFrameCount;
	float   CompositorLatency;
	float   CompositorCpuElapsedTime;
	float   CompositorGpuElapsedTime;
	float   CompositorCpuStartToGpuEndElapsedTime;
	float   CompositorGpuEndToVsyncElapsedTime;
} ovrPerfStatsPerCompositorFrame1;

typedef struct OVR_ALIGNAS(4) ovrPerfStats1_
{
	ovrPerfStatsPerCompositorFrame1  FrameStats[ovrMaxProvidedFrameStats];
	int                             FrameStatsCount;
	ovrBool                         AnyFrameStatsDropped;
	float                           AdaptiveGpuPerformanceScale;
} ovrPerfStats1;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetPerfStats(ovrSession session, ovrPerfStats* outStats)
{
	REM_TRACE(ovr_GetPerfStats);

	// TODO: Performance statistics
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_ResetPerfStats(ovrSession session)
{
	REM_TRACE(ovr_ResetPerfStats);

	// TODO: Performance statistics
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetPredictedDisplayTime(ovrSession session, long long frameIndex)
{
	REM_TRACE(ovr_GetPredictedDisplayTime);

	HolographicFrame frame = session->Frames->GetFrame(frameIndex);
	HolographicFramePrediction prediction = frame.CurrentPrediction();
	PerceptionTimestamp timestamp = prediction.Timestamp();
	DateTime target = timestamp.TargetTime();
	return double(target.time_since_epoch().count()) * 1.0e-7;
}

OVR_PUBLIC_FUNCTION(double) ovr_GetTimeInSeconds()
{
	REM_TRACE(ovr_GetTimeInSeconds);

	DateTime time = winrt::clock::now();
	return double(time.time_since_epoch().count()) * 1.0e-7;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_GetBool(ovrSession session, const char* propertyName, ovrBool defaultVal)
{
	REM_TRACE(ovr_GetBool);

	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetBool(ovrSession session, const char* propertyName, ovrBool value)
{
	REM_TRACE(ovr_SetBool);

	// TODO: Should we handle QueueAheadEnabled with always-on reprojection?
	return false;
}

OVR_PUBLIC_FUNCTION(int) ovr_GetInt(ovrSession session, const char* propertyName, int defaultVal)
{
	REM_TRACE(ovr_GetInt);

	if (strcmp("TextureSwapChainDepth", propertyName) == 0)
		return REV_SWAPCHAIN_MAX_LENGTH;

	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetInt(ovrSession session, const char* propertyName, int value)
{
	REM_TRACE(ovr_SetInt);

	return false;
}

OVR_PUBLIC_FUNCTION(float) ovr_GetFloat(ovrSession session, const char* propertyName, float defaultVal)
{
	REM_TRACE(ovr_GetFloat);

	//if (strcmp(propertyName, "IPD") == 0)
	//	return vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserIpdMeters_Float);

	// Override defaults, we should always return a valid value for these
	if (strcmp(propertyName, OVR_KEY_PLAYER_HEIGHT) == 0)
		defaultVal = OVR_DEFAULT_PLAYER_HEIGHT;
	else if (strcmp(propertyName, OVR_KEY_EYE_HEIGHT) == 0)
		defaultVal = OVR_DEFAULT_EYE_HEIGHT;

	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloat(ovrSession session, const char* propertyName, float value)
{
	REM_TRACE(ovr_SetFloat);

	return false;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetFloatArray(ovrSession session, const char* propertyName, float values[], unsigned int valuesCapacity)
{
	REM_TRACE(ovr_GetFloatArray);

	if (strcmp(propertyName, OVR_KEY_NECK_TO_EYE_DISTANCE) == 0)
	{
		if (valuesCapacity < 2)
			return 0;

		// We only know the horizontal depth
		values[0] = OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL;
		values[1] = OVR_DEFAULT_NECK_TO_EYE_VERTICAL;
		return 2;
	}

	return 0;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetFloatArray(ovrSession session, const char* propertyName, const float values[], unsigned int valuesSize)
{
	REM_TRACE(ovr_SetFloatArray);

	return false;
}

OVR_PUBLIC_FUNCTION(const char*) ovr_GetString(ovrSession session, const char* propertyName, const char* defaultVal)
{
	REM_TRACE(ovr_GetString);

	if (!session)
		return defaultVal;

	// Override defaults, we should always return a valid value for these
	if (strcmp(propertyName, OVR_KEY_GENDER) == 0)
		defaultVal = OVR_DEFAULT_GENDER;

	return defaultVal;
}

OVR_PUBLIC_FUNCTION(ovrBool) ovr_SetString(ovrSession session, const char* propertyName, const char* value)
{
	REM_TRACE(ovr_SetString);

	return false;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_Lookup(const char* name, void** data)
{
	// We don't communicate with the ovrServer.
	return ovrError_ServiceError;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetExternalCameras(ovrSession session, ovrExternalCamera* cameras, unsigned int* inoutCameraCount)
{
	// TODO: Support externalcamera.cfg used by the SteamVR Unity plugin
	return ovrError_NoExternalCameraInfo;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetExternalCameraProperties(ovrSession session, const char* name, const ovrCameraIntrinsics* const intrinsics, const ovrCameraExtrinsics* const extrinsics)
{
	return ovrError_NoExternalCameraInfo;
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetEnabledCaps(ovrSession session)
{
	return 0;
}

OVR_PUBLIC_FUNCTION(void) ovr_SetEnabledCaps(ovrSession session, unsigned int hmdCaps)
{
}

OVR_PUBLIC_FUNCTION(unsigned int) ovr_GetTrackingCaps(ovrSession session)
{
	return 0;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_ConfigureTracking(
	ovrSession session,
	unsigned int requestedTrackingCaps,
	unsigned int requiredTrackingCaps)
{
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_IsExtensionSupported(
	ovrSession session,
	ovrExtensions extension,
	ovrBool* outExtensionSupported)
{
	if (!outExtensionSupported)
		return ovrError_InvalidParameter;

	// TODO: Extensions support
	*outExtensionSupported = false;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_EnableExtension(ovrSession session, ovrExtensions extension)
{
	// TODO: Extensions support
	return ovrError_InvalidOperation;
}
