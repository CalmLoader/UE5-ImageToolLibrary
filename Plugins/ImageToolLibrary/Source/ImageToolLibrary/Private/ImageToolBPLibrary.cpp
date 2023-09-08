// Fill out your copyright notice in the Description page of Project Settings.


#include "ImageToolBPLibrary.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture.h"
#include "Components/Widget.h"

void UImageToolBPLibrary::GetImageResolution(const FString& ImagePath, FVector2D& Resolution)
{
	int32 Width = 0;
	int32 Height = 0;
	TArray64<uint8> OutData;
	if (LoadImageToData(ImagePath, OutData, Width, Height))
	{
		Resolution.X = Width;
		Resolution.Y = Height;
	}
}

void UImageToolBPLibrary::GetTextureResolution(UTexture2DDynamic* InTex, FVector2D& Resolution)
{
	if (InTex)
	{
		Resolution.X = InTex->GetSurfaceWidth();
		Resolution.Y = InTex->GetSurfaceHeight();
	}
}

UTexture2D* UImageToolBPLibrary::LoadImageToTexture2D(const FString& ImagePath)
{
	TArray64<uint8> OutRawData;
	int32 Width = 0;
	int32 Height = 0;
	if (LoadImageToData(ImagePath, OutRawData, Width, Height))
	{
		if (UTexture2D* InTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8))
		{
			void* TextureData = InTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, OutRawData.GetData(), OutRawData.Num());
			InTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
			InTexture->UpdateResource();
			return InTexture;
		}
	}
	return nullptr;
}

UTexture2DDynamic* UImageToolBPLibrary::LoadImageToTexture2DDy(const FString& ImagePath)
{
	TArray64<uint8>* OutData = new TArray64<uint8>(); //跟格式无关的颜色数据	
	int32 Width = 0;
	int32 Height = 0;
	if (LoadImageToData(ImagePath, *OutData, Width, Height))
	{
		UTexture2DDynamic* InDyTexture = UTexture2DDynamic::Create(Width, Height);
		if (InDyTexture && OutData)
		{
			//如果为false，表示单独使用Alpha通道作为遮罩
			InDyTexture->SRGB = true;
			//为纹理创建一个新的资源，并且更新对改资源的所有缓存引用
			InDyTexture->UpdateResource();
			FTexture2DDynamicResource* Texture2DDyRes = static_cast<FTexture2DDynamicResource*>(InDyTexture->
				GetResource());
			ENQUEUE_RENDER_COMMAND(FWriterRawDataToDyTexture2D)
			([Texture2DDyRes, OutData](FRHICommandListImmediate& RHICommandList) mutable
				{
					//给Texture2DDyRes赋值
					check(IsInRenderingThread());
					FRHITexture2D* RHITexture2D = Texture2DDyRes->GetTexture2DRHI();
					const int64 CurrentWidth = RHITexture2D->GetSizeX();
					const int64 CurrentHeight = RHITexture2D->GetSizeY();
					//给动态贴图赋值
					uint32 DestStride = 0; //目标步长
					//加锁
					uint8* DestData = static_cast<uint8*>(RHILockTexture2D(
						RHITexture2D, 0, RLM_WriteOnly, DestStride, false, false));
					for (int64 y = 0; y < CurrentHeight; ++y)
					{
						const int64 DestPtrStride = y * DestStride;
						uint8* DestPtr = &DestData[DestPtrStride];
						uint8* StrData = OutData->GetData();
						const int64 SrcPtrStride = y * CurrentWidth;
						const FColor* SrcPtr = &(reinterpret_cast<FColor*>(StrData))[SrcPtrStride];
						for (int64 x = 0; x < CurrentWidth; ++x)
						{
							*DestPtr++ = SrcPtr->B;
							*DestPtr++ = SrcPtr->G;
							*DestPtr++ = SrcPtr->R;
							*DestPtr++ = SrcPtr->A;
							SrcPtr++;
						}
					}
					RHIUnlockTexture2D(RHITexture2D, 0, false, false);
					if (OutData)
					{
						delete OutData;
						OutData = nullptr;
					}
				}
			);
			return InDyTexture;
		}
	}
	if (OutData)
	{
		delete OutData;
		OutData = nullptr;
	}
	return nullptr;
}

bool UImageToolBPLibrary::SaveImageFromTexture2D(UTexture2D* InTex, const FString& SavePath)
{
	if (InTex)
	{
#if WITH_EDITOR
		TArray64<uint8> OutData;
		InTex->Source.GetMipData(OutData, 0);
		int32 Width = InTex->GetSizeX();
		int32 Height = InTex->GetSizeY();
		const bool bSave = SaveImageFromRawData(OutData, SavePath, Width, Height);
		return bSave;
#endif
	}
	return false;
}

void UImageToolBPLibrary::SaveImageFromTexture2DDy(UTexture2DDynamic* InDyTex, const FString& SavePath)
{
	TArray64<uint8>* RawData = new TArray64<uint8>();
	FTexture2DDynamicResource* TextureResource = static_cast<FTexture2DDynamicResource*>(InDyTex->GetResource());
	ENQUEUE_RENDER_COMMAND(FWriteRawDataToTexture)(
		[TextureResource, RawData, SavePath](FRHICommandListImmediate& RHICmdList) mutable
		{
			check(IsInRenderingThread());
			FRHITexture2D* TextureRHI = TextureResource->GetTexture2DRHI();
			int32 Width = TextureRHI->GetSizeX();
			int32 Height = TextureRHI->GetSizeY();
			uint32 DestStride = 0;
			const uint8* DestData = static_cast<uint8*>(RHILockTexture2D(TextureRHI, 0, RLM_ReadOnly, DestStride, false,
																		 false));
			RawData->SetNum(Width * Height * 4);
			for (int32 y = 0; y < Height; y++)
			{
				const uint8* DestPtr = &DestData[(static_cast<int64>(Height) - 1 - y) * DestStride];
				FColor* SrcPtr = &(reinterpret_cast<FColor*>(RawData->GetData()))[(static_cast<int64>(Height) - 1 - y) * Width];
				for (int32 x = 0; x < Width; x++)
				{
					SrcPtr->B = *DestPtr++;
					SrcPtr->G = *DestPtr++;
					SrcPtr->R = *DestPtr++;
					SrcPtr->A = *DestPtr++;
					SrcPtr++;
				}
			}
			SaveImageFromRawData(*RawData, SavePath, Width, Height);
			RHIUnlockTexture2D(TextureRHI, 0, false, false);
			delete RawData;
		});
}

UTextureRenderTarget2D* UImageToolBPLibrary::RenderWidget(UWidget* Widget, const FVector2D& DrawSize)
{
	if (FSlateApplication::IsInitialized() && Widget)
	{
		FWidgetRenderer* WidgetRenderer = new FWidgetRenderer(false);
		UTextureRenderTarget2D* TextureRenderTarget = NewObject<UTextureRenderTarget2D>();
		TextureRenderTarget->InitAutoFormat(DrawSize.X, DrawSize.Y);
		const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
		WidgetRenderer->DrawWidget(TextureRenderTarget, SlateWidget, DrawSize, 1.0f);
		return TextureRenderTarget;
	}
	return nullptr;
}

bool UImageToolBPLibrary::SaveRenderTarget2D(UTextureRenderTarget2D* RenderTarget2D, const FString& SavePath)
{
	FTextureRenderTargetResource* RTResource = RenderTarget2D->GameThread_GetRenderTargetResource();
	TArray<FColor> OutImageData;
	const float Width = RenderTarget2D->GetSurfaceWidth();
	const float Height = RenderTarget2D->GetSurfaceHeight();
	OutImageData.AddUninitialized(Width * Height);
	RTResource->ReadPixels(OutImageData, FReadSurfaceDataFlags(RCM_UNorm));
	TArray<uint8> CompressedBitmap;
	FImageUtils::ThumbnailCompressImageArray(Width, Height, OutImageData, CompressedBitmap);
	return FFileHelper::SaveArrayToFile(CompressedBitmap, *SavePath);
}

UTexture2D* UImageToolBPLibrary::RenderWidgetToUTexture2D(UWidget* Widget, const FVector2D& DrawSize)
{
	if (FSlateApplication::IsInitialized() && Widget)
	{
		if(FWidgetRenderer* WidgetRenderer = new FWidgetRenderer(false))
		{
			UTextureRenderTarget2D* TextureRenderTarget = NewObject<UTextureRenderTarget2D>();
			TextureRenderTarget->ClearColor = FLinearColor::Transparent;
			TextureRenderTarget->InitAutoFormat(DrawSize.X, DrawSize.Y);
			const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
			WidgetRenderer->DrawWidget(TextureRenderTarget, SlateWidget, DrawSize, 1.0f);
			UTexture2D* RenderTex = nullptr;
			if (TextureRenderTarget)
			{
				FTextureRenderTargetResource* RTResource = TextureRenderTarget->GameThread_GetRenderTargetResource();
				TArray<FColor> WindowColor;
				int32 Width = TextureRenderTarget->GetSurfaceWidth();
				int32 Height = TextureRenderTarget->GetSurfaceHeight();
				WindowColor.AddUninitialized(Width * Height);
				RTResource->ReadPixels(WindowColor, FReadSurfaceDataFlags(RCM_UNorm));
				TArray<uint8> ResultData;
				FImageUtils::ThumbnailCompressImageArray(Width, Height, WindowColor, ResultData);

				IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
				TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
				if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(ResultData.GetData(), ResultData.GetAllocatedSize()))
				{
					TArray<uint8> OutRawData;
					ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, OutRawData);
					Width = ImageWrapper->GetWidth();
					Height = ImageWrapper->GetHeight();
					RenderTex = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
					if (RenderTex)
					{
						void* TextureData = RenderTex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
						FMemory::Memcpy(TextureData, OutRawData.GetData(), OutRawData.Num());
						RenderTex->GetPlatformData()->Mips[0].BulkData.Unlock();
						RenderTex->UpdateResource();
					}
				}
			}
			delete WidgetRenderer;
			WidgetRenderer = nullptr;
			return RenderTex;
		}
	}
	return nullptr;
}


//--------------private-------------------


bool UImageToolBPLibrary::SaveImageFromRawData(TArray64<uint8>& OutData, const FString& SavePath, const int32& Width, const int32& Height)
{
	EImageFormat ImageFormat;
	GetImageFormatFromPath(SavePath, ImageFormat);
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	TSharedPtr<IImageWrapper> ImageWrapperPtr = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (ImageWrapperPtr.IsValid() && ImageWrapperPtr->SetRaw(OutData.GetData(), OutData.GetAllocatedSize(), Width,
	                                                         Height, ERGBFormat::BGRA, 8))
	{
		const TArray64<uint8> CompressedData = ImageWrapperPtr->GetCompressed(100);
		FFileHelper::SaveArrayToFile(CompressedData, *SavePath);
		return true;
	}
	return false;
}

bool UImageToolBPLibrary::LoadImageToData(const FString& ImagePath, TArray64<uint8>& OutData, int32& Width, int32& Height)
{
	//取出图片的二进制数据
	TArray<uint8> ImageResultData;
	if (!FFileHelper::LoadFileToArray(ImageResultData, *ImagePath))
	{
		return false;
	}

	//解析图片格式
	EImageFormat ImageFormat = EImageFormat::Invalid;
	GetImageFormatFromPath(ImagePath, ImageFormat);

	//加载图片处理模块
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
	//根据不同的图片格式创建不同的图片处理类的具体实例
	const TSharedPtr<IImageWrapper> ImageWrapperPtr = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	if (ImageWrapperPtr.IsValid() && ImageWrapperPtr->SetCompressed(ImageResultData.GetData(), ImageResultData.GetAllocatedSize()))
	{
		//OutData 与格式无关的颜色数据
		ImageWrapperPtr->GetRaw(ERGBFormat::BGRA, 8, OutData);
		Width = ImageWrapperPtr->GetWidth();
		Height = ImageWrapperPtr->GetHeight();
		return true;
	}
	return false;
}

void UImageToolBPLibrary::GetImageFormatFromPath(const FString& Path, EImageFormat& ImageFormat)
{
	//解析图片格式
	const FString FileExtension = FPaths::GetExtension(Path, false);
	ImageFormat = EImageFormat::Invalid;
	if (FileExtension.Equals(TEXT("jpg"), ESearchCase::IgnoreCase) || FileExtension.Equals(
		TEXT("jpeg"), ESearchCase::IgnoreCase))
	{
		ImageFormat = EImageFormat::JPEG;
	}
	else if (FileExtension.Equals(TEXT("png"), ESearchCase::IgnoreCase))
	{
		ImageFormat = EImageFormat::PNG;
	}
}
