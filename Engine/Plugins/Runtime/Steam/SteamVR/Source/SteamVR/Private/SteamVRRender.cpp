// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//
//#include "CoreMinimal.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"

#include "Core.h"
#include "ISteamVRPlugin.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"

#if STEAMVR_SUPPORTED_PLATFORMS

static TAutoConsoleVariable<int32> CUsePostPresentHandoff(TEXT("vr.SteamVR.UsePostPresentHandoff"), 0, TEXT("Whether or not to use PostPresentHandoff.  If true, more GPU time will be available, but this relies on no SceneCaptureComponent2D or WidgetComponents being active in the scene.  Otherwise, it will break async reprojection."));

void FSteamVRHMD::DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize)
{
	check(0);
}

static TAutoConsoleVariable<float> CTopCropOffset(TEXT("vr.TCropOffset"), 0.5f, TEXT("Precentage to offset the top of the letterboxed view"));
static TAutoConsoleVariable<float> CAllowSpectatorTexture(TEXT("vr.bAllowSpectatorTexture"), 1.f, TEXT("Whether to allow rendering of spectator texture in window"));

void FSteamVRHMD::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture) const
{
	check(IsInRenderingThread());

	UpdateLayerTextures();

	if (bSplashIsShown)
	{
		SetRenderTarget(RHICmdList, SrcTexture, FTextureRHIRef());
		RHICmdList.ClearColorTexture(SrcTexture, FLinearColor(0, 0, 0, 0), FIntRect());
	}

	const uint32 ViewportWidth = BackBuffer->GetSizeX();
	const uint32 ViewportHeight = BackBuffer->GetSizeY();

	const FIntRect BackBufferRect = FIntRect(0, 0, ViewportWidth, ViewportHeight);
	FIntRect DstViewRect = BackBufferRect;
	FIntRect SrcViewRect = FIntRect(0, 0, ViewportWidth * 2, ViewportHeight * 2);
	FRHITexture2D* SpectatorTexture = nullptr;
	FIntRect SpectatorDstViewRect;
	FIntRect SpectatorTextureRect;

	if ((CAllowSpectatorTexture.GetValueOnRenderThread() != 0) && MirrorRenderDelegate.IsBound())
	{
		SpectatorTexture = MirrorRenderDelegate.Execute(DstViewRect, /*DstViewRect*/ SrcViewRect, SpectatorDstViewRect, SpectatorTextureRect);
	}

	SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());

	const auto FeatureLevel = GMaxRHIFeatureLevel;
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

	if (SpectatorTexture)
	{
		FIntRect DstRect = SpectatorDstViewRect;
		FTexture2DRHIParamRef DstTexture = BackBuffer;
		FIntRect SrcRect = SpectatorTextureRect;
		if (DstRect.IsEmpty())
		{
			DstRect = FIntRect(0, 0, DstTexture->GetSizeX(), DstTexture->GetSizeY());
		}

		const uint32 ViewportWidth = DstRect.Width();
		const uint32 ViewportHeight = DstRect.Height();
		const FIntPoint TargetSize(ViewportWidth, ViewportHeight);

		const float SrcTextureWidth = SpectatorTexture->GetTexture2D()->GetSizeX();
		const float SrctextureHeight = SpectatorTexture->GetTexture2D()->GetSizeY();
		float U = 0.f, V = 0.f, USize = 1.f, VSize = 1.f;
		if (!SrcRect.IsEmpty())
		{
			U = SrcRect.Min.X / SrcTextureWidth;
			V = SrcRect.Min.Y / SrctextureHeight;
			USize = SrcRect.Width() / SrcTextureWidth;
			VSize = SrcRect.Height() / SrctextureHeight;
		}

		FRHITexture* SrcTextureRHI = SpectatorTexture;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, &SrcTextureRHI, 1);
		SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
		RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0, DstRect.Max.X, DstRect.Max.Y, 1.0f);
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTextureRHI);

		if (WindowMirrorMode == 1)
		{
			RendererModule->DrawRectangle(
				RHICmdList,
				0, 0,
				ViewportWidth, ViewportHeight,
				U, V,
				USize, VSize,
				TargetSize,
				FIntPoint(1, 1),
				*VertexShader,
				EDRF_Default);
		}
	}

	RHICmdList.SetViewport(0, 0, 0, ViewportWidth, ViewportHeight, 1.0f);

	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);

	if (WindowMirrorMode != 0)
	{

		if (WindowMirrorMode == 1)
		{
			// need to clear when rendering only one eye since the borders won't be touched by the DrawRect below
			RHICmdList.ClearColorTexture(BackBuffer, FLinearColor::Black, FIntRect());

			RendererModule->DrawRectangle(
				RHICmdList,
				ViewportWidth / 4, 0,
				ViewportWidth / 2, ViewportHeight,
				0.1f, 0.2f,
				0.3f, 0.6f,
				FIntPoint(ViewportWidth, ViewportHeight),
				FIntPoint(1, 1),
				*VertexShader,
				EDRF_Default);
		}
		else if (WindowMirrorMode == 2)
		{
			RendererModule->DrawRectangle(
				RHICmdList,
				0, 0,
				ViewportWidth, ViewportHeight,
				0.0f, 0.0f,
				1.0f, 1.0f,
				FIntPoint(ViewportWidth, ViewportHeight),
				FIntPoint(1, 1),
				*VertexShader,
				EDRF_Default);
		}
	}
}

static void DrawOcclusionMesh(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass, const FHMDViewMesh MeshAssets[])
{
	check(IsInRenderingThread());
	check(StereoPass != eSSP_FULL);

	const uint32 MeshIndex = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	const FHMDViewMesh& Mesh = MeshAssets[MeshIndex];
	check(Mesh.IsValid());

	DrawIndexedPrimitiveUP(
		RHICmdList,
		PT_TriangleList,
		0,
		Mesh.NumVertices,
		Mesh.NumTriangles,
		Mesh.pIndices,
		sizeof(Mesh.pIndices[0]),
		Mesh.pVertices,
		sizeof(Mesh.pVertices[0])
		);
}

void FSteamVRHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, HiddenAreaMeshes);
}

void FSteamVRHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, VisibleAreaMeshes);
}

#if PLATFORM_WINDOWS

FSteamVRHMD::D3D11Bridge::D3D11Bridge(FSteamVRHMD* plugin):
	BridgeBaseImpl(plugin),
	RenderTargetTexture(nullptr)
{
}

void FSteamVRHMD::D3D11Bridge::BeginRendering()
{
	check(IsInRenderingThread());

	static bool Inited = false;
	if (!Inited)
	{
		Inited = true;
	}
}

void FSteamVRHMD::D3D11Bridge::FinishRendering()
{
	vr::VRTextureBounds_t LeftBounds;
	LeftBounds.uMin = 0.0f;
	LeftBounds.uMax = 0.5f;
	LeftBounds.vMin = 0.0f;
	LeftBounds.vMax = 1.0f;

	vr::Texture_t Texture;
	Texture.handle = RenderTargetTexture;
	Texture.eType = vr::API_DirectX;
	Texture.eColorSpace = vr::ColorSpace_Auto;
	vr::EVRCompositorError Error = Plugin->VRCompositor->Submit(vr::Eye_Left, &Texture, &LeftBounds);

	vr::VRTextureBounds_t RightBounds;
	RightBounds.uMin = 0.5f;
	RightBounds.uMax = 1.0f;
	RightBounds.vMin = 0.0f;
	RightBounds.vMax = 1.0f;

	Texture.handle = RenderTargetTexture;
	Error = Plugin->VRCompositor->Submit(vr::Eye_Right, &Texture, &RightBounds);
	if (Error != vr::VRCompositorError_None)
	{
		UE_LOG(LogHMD, Log, TEXT("Warning:  SteamVR Compositor had an error on present (%d)"), (int32)Error);
	}
}

void FSteamVRHMD::D3D11Bridge::Reset()
{
}

void FSteamVRHMD::D3D11Bridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI)
{
	check(IsInGameThread());
	check(InViewportRHI);

	const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
	check(IsValidRef(RT));

	if (RenderTargetTexture != nullptr)
	{
		RenderTargetTexture->Release();	//@todo steamvr: need to also release in reset
	}

	RenderTargetTexture = (ID3D11Texture2D*)RT->GetNativeResource();
	RenderTargetTexture->AddRef();

	InViewportRHI->SetCustomPresent(this);
}


void FSteamVRHMD::D3D11Bridge::OnBackBufferResize()
{
}

bool FSteamVRHMD::D3D11Bridge::Present(int& SyncInterval)
{
	check(IsInRenderingThread());

	if (Plugin->VRCompositor == nullptr)
	{
		return false;
	}

	FinishRendering();

	return true;
}

void FSteamVRHMD::D3D11Bridge::PostPresent()
{
	if (CUsePostPresentHandoff.GetValueOnRenderThread() == 1)
	{
		Plugin->VRCompositor->PostPresentHandoff();
	}
}

#endif // PLATFORM_WINDOWS

#endif // STEAMVR_SUPPORTED_PLATFORMS
