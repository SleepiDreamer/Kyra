#include "NGXWrapper.h"
#include "Window.h"
#include "CommandQueue.h"
#include "CommonDX.h"

#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx_defs_dlssd.h>
#include <nvsdk_ngx_params.h>
#include <nvsdk_ngx_params_dlssd.h>
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_dlssd.h>

NGXWrapper::NGXWrapper(RenderContext& context, Window& window)
	: m_context(context), m_window(window)
{
}

NGXWrapper::~NGXWrapper()
{
    if (m_dlssFeature)
    {
        NVSDK_NGX_D3D12_ReleaseFeature(m_dlssFeature);
        m_dlssFeature = nullptr;
    }

    NVSDK_NGX_D3D12_Shutdown1(m_context.device);
}

void NGXWrapper::Initialize()
{
    NVSDK_NGX_Result result = NVSDK_NGX_D3D12_Init_with_ProjectID(
        m_projectId.c_str(),
        NVSDK_NGX_ENGINE_TYPE_CUSTOM,
        "1.0",
        L"./x64/Debug/",
        m_context.device,
        nullptr,
        NVSDK_NGX_Version_API);

    if (result != NVSDK_NGX_Result_Success)
    {
        ThrowError("Failed to initialize DLSS!");
    }

    result = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_ngxParameters);
    if (NVSDK_NGX_FAILED(result))
    {
		ThrowError("GetCapabilityParameters failed: " + std::to_string(result));
        ThrowError("Failed to get DLSS capability parameters!");
        return;
    }

    int dlssAvailable = 0;
    result = m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSamplingDenoising_Available, &dlssAvailable);
    if (NVSDK_NGX_FAILED(result) || !dlssAvailable)
    {
        int needsUpdatedDriver = 0;
        m_ngxParameters->Get(NVSDK_NGX_Parameter_SuperSamplingDenoising_NeedsUpdatedDriver, &needsUpdatedDriver);

        if (needsUpdatedDriver)
        {
	        ThrowError("DLSS-RR requires a newer NVIDIA driver!");
        }
        else
        {
	        ThrowError("DLSS-RR not supported (unsupported GPU or missing nvngx_dlss.dll)");
        }

        m_dlssSupported = false;
        return;
    }
    m_dlssSupported = true;

    SetOptimalInputRes();
}

void NGXWrapper::CreateDLSSFeature()
{
    if (!m_dlssSupported) return;

    if (m_dlssFeature)
    {
        m_context.commandQueue->Flush();
        NVSDK_NGX_D3D12_ReleaseFeature(m_dlssFeature);
        m_dlssFeature = nullptr;
    }

    bool MVLowRes = true;
    bool MVJittered = false;
    bool isHDR = true;
    bool depthInverted = false;
    bool doSharpening = false;
    bool autoExposure = false;

    int featureFlags = NVSDK_NGX_DLSS_Feature_Flags_None;
    featureFlags |= MVLowRes ? NVSDK_NGX_DLSS_Feature_Flags_MVLowRes : 0;
    featureFlags |= MVJittered ? NVSDK_NGX_DLSS_Feature_Flags_MVJittered : 0;
    featureFlags |= isHDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0;
    featureFlags |= depthInverted ? NVSDK_NGX_DLSS_Feature_Flags_DepthInverted : 0;
    featureFlags |= doSharpening ? NVSDK_NGX_DLSS_Feature_Flags_DoSharpening : 0;
    featureFlags |= autoExposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0;

    NVSDK_NGX_DLSSD_Create_Params dlssdCreateParams = {};
    dlssdCreateParams.InWidth = m_renderWidth;
    dlssdCreateParams.InHeight = m_renderHeight;
    dlssdCreateParams.InTargetWidth = m_window.GetWidth();
    dlssdCreateParams.InTargetHeight = m_window.GetHeight();
    dlssdCreateParams.InPerfQualityValue = m_dlssQuality;
    dlssdCreateParams.InFeatureCreateFlags = featureFlags;
    dlssdCreateParams.InRoughnessMode = NVSDK_NGX_DLSS_Roughness_Mode_Packed;
    dlssdCreateParams.InUseHWDepth = NVSDK_NGX_DLSS_Depth_Type_Linear;
    dlssdCreateParams.InDenoiseMode = NVSDK_NGX_DLSS_Denoise_Mode_DLUnified;

    NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Performance, NVSDK_NGX_RayReconstruction_Hint_Render_Preset_Default);
    NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Balanced, NVSDK_NGX_RayReconstruction_Hint_Render_Preset_Default);
    NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_Quality, NVSDK_NGX_RayReconstruction_Hint_Render_Preset_Default);
    NVSDK_NGX_Parameter_SetUI(m_ngxParameters, NVSDK_NGX_Parameter_RayReconstruction_Hint_Render_Preset_DLAA, NVSDK_NGX_RayReconstruction_Hint_Render_Preset_Default);

    auto commandList = m_context.commandQueue->GetCommandList();
    NVSDK_NGX_Result result = NGX_D3D12_CREATE_DLSSD_EXT(commandList.Get(), 1, 1, &m_dlssFeature, m_ngxParameters, &dlssdCreateParams);
    if (result != NVSDK_NGX_Result_Success)
    {
        ThrowError("Failed to create DLSS feature!");
    }
    else
    {
        std::cout << "Succesfully created DLSS Feature\n";
    }
    m_context.commandQueue->ExecuteCommandList(commandList);
    m_context.commandQueue->Flush();
}

void NGXWrapper::EvaluateDLSS(ID3D12GraphicsCommandList4* commandList, const DLSSInputs& inputs) const
{
    if (!m_dlssSupported) return;

    int reset = 0;
    NVSDK_NGX_Dimensions renderDimensions = {};
    renderDimensions.Width = m_renderWidth;
    renderDimensions.Height = m_renderHeight;

    NVSDK_NGX_D3D12_DLSSD_Eval_Params D3D12DlssdEvalParams;
    memset(&D3D12DlssdEvalParams, 0, sizeof(D3D12DlssdEvalParams));
    D3D12DlssdEvalParams.pInDiffuseAlbedo = inputs.albedo;
    D3D12DlssdEvalParams.pInSpecularAlbedo = inputs.specularAlbedo;
    D3D12DlssdEvalParams.pInNormals = inputs.normalRoughness;
    D3D12DlssdEvalParams.pInRoughness = nullptr;
    D3D12DlssdEvalParams.pInColor = inputs.input;
    D3D12DlssdEvalParams.pInOutput = inputs.output;
    D3D12DlssdEvalParams.pInDepth = inputs.depth;
    D3D12DlssdEvalParams.pInMotionVectors = inputs.motionVectors;
    D3D12DlssdEvalParams.InJitterOffsetX = -inputs.jitterX;
    D3D12DlssdEvalParams.InJitterOffsetY = -inputs.jitterY;
    D3D12DlssdEvalParams.InRenderSubrectDimensions = renderDimensions;
    D3D12DlssdEvalParams.InReset = reset;

    NVSDK_NGX_Result result = NGX_D3D12_EVALUATE_DLSSD_EXT(commandList, m_dlssFeature, m_ngxParameters, &D3D12DlssdEvalParams);
    if (result != NVSDK_NGX_Result_Success)
    {
        ThrowError("Failed to evaluate DLSS!");
    }
}

glm::ivec2 NGXWrapper::Resize()
{
	return SetOptimalInputRes();
}

glm::ivec2 NGXWrapper::SetOptimalInputRes()
{
    unsigned int outRenderMinWidth, outRenderMinHeight, outRenderMaxWidth, outRenderMaxHeight;
    float outSharpness;

	uint32_t outputWidth = m_window.GetWidth();
	uint32_t outputHeight = m_window.GetHeight();

    if (m_dlssSupported)
    {
        NVSDK_NGX_Result result = NGX_DLSSD_GET_OPTIMAL_SETTINGS(m_ngxParameters,
            outputWidth,
            outputHeight,
            m_dlssQuality,
            &m_renderWidth,
            &m_renderHeight,
            &outRenderMinWidth,
            &outRenderMinHeight,
            &outRenderMaxWidth,
            &outRenderMaxHeight,
            &outSharpness);

        if (result != NVSDK_NGX_Result_Success || m_renderWidth == 0 || m_renderHeight == 0)
        {
            ThrowError("Failed to get optimal DLSS configuration!");
        }

		CreateDLSSFeature();
    }
    else
    {
        m_renderWidth = outputWidth;
        m_renderHeight = outputHeight;
    }

	return { m_renderWidth, m_renderHeight };
}

void NGXWrapper::SetDLSSQuality(const DLSSQuality qualityMode)
{
    switch (qualityMode)
    {
    case UltraPerformance:
        m_dlssQuality = NVSDK_NGX_PerfQuality_Value_UltraPerformance;
		break;
	case Performance:
		m_dlssQuality = NVSDK_NGX_PerfQuality_Value_MaxPerf;
		break;
	case Balanced:
		m_dlssQuality = NVSDK_NGX_PerfQuality_Value_Balanced;
		break;
	case Quality:
		m_dlssQuality = NVSDK_NGX_PerfQuality_Value_MaxQuality;
		break;
	case DLAA:
		m_dlssQuality = NVSDK_NGX_PerfQuality_Value_DLAA;
		break;
    default: 
		m_dlssQuality = NVSDK_NGX_PerfQuality_Value_Balanced;
		break;
    }
    SetOptimalInputRes();
}

DLSSQuality NGXWrapper::GetDLSSQuality() const
{
    switch (m_dlssQuality)
    {
    case NVSDK_NGX_PerfQuality_Value_UltraPerformance:
        return UltraPerformance;
    case NVSDK_NGX_PerfQuality_Value_MaxPerf:
        return Performance;
    case NVSDK_NGX_PerfQuality_Value_Balanced:
        return Balanced;
    case NVSDK_NGX_PerfQuality_Value_MaxQuality:
        return Quality;
    case NVSDK_NGX_PerfQuality_Value_DLAA:
        return DLAA;
    default:
        return Balanced;
	}
}

glm::vec2 NGXWrapper::GetJitter(const int frame)
{
    glm::vec2 Result(0.0f, 0.0f);

    constexpr int BaseX = 2;
    constexpr bool LIMIT_PHASE = false;
    int Index = frame + 1;

    float InvBase = 1.0f / BaseX;
    float Fraction = InvBase;
    while (Index > 0)
    {
        Result.x += (Index % BaseX) * Fraction;
        Index /= BaseX;
        Fraction *= InvBase;
    }

    constexpr int BaseY = 3;
    Index = frame + 1;
    InvBase = 1.0f / BaseY;
    Fraction = InvBase;
    while (Index > 0)
    {
        Result.y += (Index % BaseY) * Fraction;
        Index /= BaseY;
        Fraction *= InvBase;
    }

    Result.x -= 0.5f;
    Result.y -= 0.5f;
    return Result;
}
