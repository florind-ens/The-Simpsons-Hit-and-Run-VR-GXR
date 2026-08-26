#ifndef SHAR_OPENXR_MANAGER_H
#define SHAR_OPENXR_MANAGER_H

#if defined(RAD_ANDROID)

#include <radmath/radmath.hpp>

class tCamera;

namespace SharOpenXR
{
    bool Initialize();
    void Shutdown();
    void PollEvents();
    bool BeginFrame();
    unsigned GetEyeCount();
    bool BeginEye(unsigned eye);
    void EndEye(unsigned eye);
    bool IsMultiviewAvailable();
    bool IsMultiviewRendering();
    void SetMultiviewTargetActive(bool active);
    bool BeginMultiview();
    bool BeginMultiviewGuiEye(unsigned eye);
    void EndMultiview();
    bool PrepareMultiviewCamera(tCamera* baseCamera);
    bool GetMultiviewMatrices(rmt::Matrix* projections,
                              rmt::Matrix* viewAdjustments);
    void SetWorldRendering(bool enabled);
    void SetEmbeddedHudRendering(bool enabled);
    bool IsEmbeddedHudRendering() __attribute__((weak));
    void SetRadarRendering(bool enabled);
    bool BeginRadarCapture(int xMin,int yMin,int xMax,int yMax);
    void EndRadarCapture();
    bool BeginMissionHudCapture(unsigned slot, int xMin, int yMin,
                                int xMax, int yMax);
    void UpdateMissionHudLayout(unsigned slot,const rmt::Matrix& layout);
    void EndMissionHudCapture();
    void ResetMissionHudSlot(unsigned slot);
    void CaptureSpatialCoinIcon();
    bool IsRadarRendering() __attribute__((weak));
    bool IsRightEyeRendering();
    void PrepareRadarDraw() __attribute__((weak));
    bool GetActiveRadarProjection(rmt::Matrix* projection, int* width,
                                  int* height) __attribute__((weak));
    void SetMovieRendering(bool enabled);
    bool IsMovieRendering() __attribute__((weak));
    void BeginMoviePlane();
    void EndMoviePlane();
    bool GetActiveMovieProjection(rmt::Matrix* projection, int* width,
                                  int* height) __attribute__((weak));
    void SetFrontendPlaneActive(bool active);
    void SetFrontendPlaneRendering(bool rendering);
    bool IsFrontendPlaneRendering() __attribute__((weak));
    bool GetActiveFrontendProjection(rmt::Matrix* projection, int* width,
                                     int* height) __attribute__((weak));
    void SetPauseCoinVisible(bool visible);
    void DrawPauseCoinIcon();
    void SetIrisBlackout(bool black);
    void SetEnhancedUiConvergence(bool enabled);
    bool HasEnhancedUiConvergence() __attribute__((weak));
    bool GetEyeCamera(unsigned eye, tCamera* baseCamera,
                      rmt::Matrix* cameraToWorld);
    bool GetActiveEyeCamera(tCamera* baseCamera,
                            rmt::Matrix* cameraToWorld);
    bool GetActiveCullingCamera(rmt::Matrix* cameraToWorld);
    bool GetLatestCullingCamera(rmt::Matrix* cameraToWorld);
    void SetVrModeEnabled(bool enabled);
    bool IsVrModeEnabled();
    void SetSpatialHudEnabled(bool enabled);
    bool IsSpatialHudEnabled();
    bool IsSpatialHudConfigured();
    void SetDeveloperMenusEnabled(bool enabled);
    bool IsDeveloperMenusEnabled();
    void SetVrBaseHeading(const rmt::Vector& heading);
    bool RecenterVrPose();
    bool GetPhysicalHeadHeight(float* heightMetres);
    bool ConsumeRoomscaleMovement(rmt::Vector* worldDelta);
    void SetSeatedMode(bool enabled);
    bool IsSeatedMode();
    void SetSnapTurnEnabled(bool enabled);
    bool IsSnapTurnEnabled();
    void SetSmoothTurnSpeed(float degreesPerSecond);
    float GetSmoothTurnSpeed();
    void SetSnapTurnAngle(float degrees);
    float GetSnapTurnAngle();
    void SetCsmEnabled(bool enabled);
    bool IsCsmEnabled();
    void SetEnhancedMaterialsEnabled(bool enabled);
    bool IsEnhancedMaterialsEnabled();
    void SetGtaoEnabled(bool enabled);
    bool IsGtaoEnabled();
    void SetVehicleLightMode(int mode);
    int GetVehicleLightMode();
    void SetVrSteeringWheelEnabled(bool enabled);
    bool IsVrSteeringWheelEnabled();
    bool GetVrSteeringWheelValue(float* value);
    void SetRenderScale(float scale);
    float GetRenderScale();
    void SetRefreshRate(float hz);
    float GetRefreshRate();
    void ApplyGtao();
    bool IsHorizontalMenuInputDominant();
    bool IsVerticalMenuInputDominant();
    bool IsRightEyeRendering();
    bool GetHeadForward(rmt::Vector* forward);
    bool GetControllerWorldPose(unsigned hand, tCamera* baseCamera,
                                rmt::Matrix* controllerToWorld);
    bool GetControllerLocalPose(unsigned hand, rmt::Matrix* controllerPose);
    void RenderControllerHands(tCamera* baseCamera);
    void EndFrame();

    // Queried by the GLES PDDI backend whenever a view changes projection.
    bool GetActiveProjection(rmt::Matrix* projection, int* width, int* height)
        __attribute__((weak));
    bool GetActiveViewport(int* width, int* height)
        __attribute__((weak));
    bool GetActiveUiHorizontalOffset(float* offset)
        __attribute__((weak));
}

#endif
#endif
