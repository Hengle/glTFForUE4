// Copyright 2017 - 2018 Code 4 Game, Org. All Rights Reserved.

#pragma once

#include <vector>
#include <map>
#include <memory>

#include <libgltf/libgltf.h>

#include "Engine/Texture.h"

namespace glTFForUE4
{
    class GLTFFORUE4_API FFeedbackTaskWrapper
    {
    public:
        explicit FFeedbackTaskWrapper(class FFeedbackContext* InFeedbackContext, const FText& InTask, bool InShowProgressDialog);
        virtual ~FFeedbackTaskWrapper();

    public:
        const FFeedbackTaskWrapper& Log(ELogVerbosity::Type InLogVerbosity, const FText& InMessge) const;
        const FFeedbackTaskWrapper& UpdateProgress(int32 InNumerator, int32 InDenominator) const;
        const FFeedbackTaskWrapper& StatusUpdate(int32 InNumerator, int32 InDenominator, const FText& InStatusText) const;
        const FFeedbackTaskWrapper& StatusForceUpdate(int32 InNumerator, int32 InDenominator, const FText& InStatusText) const;

    private:
        class FFeedbackContext* FeedbackContext;
    };
}

class GLTFFORUE4_API FglTFBufferData
{
public:
    FglTFBufferData(const TArray<uint8>& InData);
    FglTFBufferData(const FString& InFileFolderRoot, const FString& InUri);
    virtual ~FglTFBufferData();

public:
    operator bool() const;
    const TArray<uint8>& GetData() const;
    bool IsFromFile() const;
    const FString& GetFilePath() const;

private:
    TArray<uint8> Data;
    FString FilePath;
    FString StreamType;
    FString StreamEncoding;
};

namespace EglTFBufferSource
{
    enum Type
    {
        Binaries,
        Images,
        Buffers,
        Max,
    };
}

class GLTFFORUE4_API FglTFBuffers
{
public:
    FglTFBuffers(bool InConstructByBinary = false);
    virtual ~FglTFBuffers();

public:
    bool CacheBinary(uint32 InIndex, const TArray<uint8>& InData);
    bool CacheImages(uint32 InIndex, const FString& InFileFolderRoot, const std::shared_ptr<libgltf::SImage>& InImage);
    bool CacheBuffers(uint32 InIndex, const FString& InFileFolderRoot, const std::shared_ptr<libgltf::SBuffer>& InBuffer);
    bool Cache(const FString& InFileFolderRoot, const std::shared_ptr<libgltf::SGlTF>& InglTF);

public:
    template<EglTFBufferSource::Type SourceType>
    const TArray<uint8>& GetData(int32 InIndex, FString& OutFilePath) const
    {
        if (!IndexToIndex[SourceType].Contains(InIndex)) return DataEmpty;
        uint32 DataIndex = IndexToIndex[SourceType][InIndex];
        if (DataIndex >= static_cast<uint32>(Datas.Num())) return DataEmpty;
        const TSharedPtr<FglTFBufferData>& Data = Datas[DataIndex];
        if (!Data.IsValid()) return DataEmpty;
        OutFilePath = Data->GetFilePath();
        return Data->GetData();
    }

    template<typename TElem, EglTFBufferSource::Type SourceType>
    bool Get(int32 InIndex, TArray<TElem>& OutBufferSegment, FString& OutFilePath, int32 InStart = 0, int32 InCount = 0, int32 InStride = 0) const
    {
        if (InStride == 0) InStride = sizeof(TElem);
        checkfSlow(sizeof(TElem) > InStride, TEXT("Stride is too smaller!"));
        if (sizeof(TElem) > InStride) return false;
        if (InStart < 0) return false;
        const TArray<uint8>& BufferSegment = GetData<SourceType>(InIndex, OutFilePath);
        if (BufferSegment.Num() <= 0) return false;
        if (InCount <= 0) InCount = BufferSegment.Num();
        if (BufferSegment.Num() < (InStart + InCount * InStride)) return false;

        OutBufferSegment.SetNumUninitialized(InCount);
        if (InStride == sizeof(TElem))
        {
            FMemory::Memcpy((void*)OutBufferSegment.GetData(), (void*)(BufferSegment.GetData() + InStart), InCount * sizeof(TElem));
        }
        else
        {
            for (int32 i = 0; i < InCount; ++i)
            {
                FMemory::Memcpy((void*)(OutBufferSegment.GetData() + i), (void*)(BufferSegment.GetData() + InStart + i * InStride), InStride);
            }
        }
        return true;
    }

    template<typename TElem>
    bool GetImageData(const std::shared_ptr<libgltf::SGlTF>& InglTF, int32 InImageIndex, TArray<TElem>& OutBufferSegment, FString& OutFilePath) const
    {
        if (!InglTF) return false;
        if (InImageIndex < 0 || static_cast<uint32>(InImageIndex) >= InglTF->images.size()) return false;
        const std::shared_ptr<libgltf::SImage>& Image = InglTF->images[InImageIndex];
        if (!Image) return false;
        if (Image->uri.empty())
        {
            if (!!(Image->bufferView))
            {
                return GetBufferViewData<TElem>(InglTF, (int32)(*Image->bufferView), OutBufferSegment, OutFilePath);
            }
        }
        else
        {
            return Get<TElem, EglTFBufferSource::Images>(InImageIndex, OutBufferSegment, OutFilePath);
        }
        return false;
    }

    template<typename TElem>
    bool GetBufferViewData(const std::shared_ptr<libgltf::SGlTF>& InglTF, int32 InBufferViewIndex, TArray<TElem>& OutBufferSegment, FString& OutFilePath, int32 InOffset = 0, int32 InCount = 0) const
    {
        if (!InglTF) return false;
        if (InBufferViewIndex < 0 || static_cast<uint32>(InBufferViewIndex) >= InglTF->bufferViews.size()) return false;
        const std::shared_ptr<libgltf::SBufferView>& BufferView = InglTF->bufferViews[InBufferViewIndex];
        if (!BufferView || !BufferView->buffer) return false;
        int32 BufferIndex = (int32)(*BufferView->buffer);
        return (bConstructByBinary
            ? Get<TElem, EglTFBufferSource::Binaries>(BufferIndex, OutBufferSegment, OutFilePath, BufferView->byteOffset + InOffset, InCount != 0 ? InCount : BufferView->byteLength, BufferView->byteStride)
            : Get<TElem, EglTFBufferSource::Buffers>(BufferIndex, OutBufferSegment, OutFilePath, BufferView->byteOffset + InOffset, InCount != 0 ? InCount : BufferView->byteLength, BufferView->byteStride));
    }

private:
    bool bConstructByBinary;
    TMap<uint32, uint32> IndexToIndex[EglTFBufferSource::Max];
    TArray<TSharedPtr<FglTFBufferData>> Datas;
    const TArray<uint8> DataEmpty;
};

class GLTFFORUE4_API FglTFImporter
{
public:
    static TSharedPtr<FglTFImporter> Get(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, class FFeedbackContext* InFeedbackContext);

public:
    FglTFImporter();
    virtual ~FglTFImporter();

public:
    virtual FglTFImporter& Set(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, class FFeedbackContext* InFeedbackContext);
    virtual UObject* Create(const TWeakPtr<struct FglTFImportOptions>& InglTFImportOptions, const std::shared_ptr<libgltf::SGlTF>& InGlTF, const FglTFBuffers& InglTFBuffers) const;

protected:
    UClass* InputClass;
    UObject* InputParent;
    FName InputName;
    EObjectFlags InputFlags;
    class FFeedbackContext* FeedbackContext;

public:
    static bool GetMeshData(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SMeshPrimitive>& InMeshPrimitive, const FglTFBuffers& InBufferFiles, TArray<uint32>& OutTriangleIndices, TArray<FVector>& OutVertexPositions, TArray<FVector>& OutVertexNormals, TArray<FVector4>& OutVertexTangents, TArray<FVector2D> OutVertexTexcoords[MAX_STATIC_TEXCOORDS]);

protected:
    static bool GetTriangleIndices(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SMeshPrimitive>& InMeshPrimitive, const FglTFBuffers& InBufferFiles, TArray<uint32>& OutTriangleIndices);
    static bool GetVertexPositions(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SMeshPrimitive>& InMeshPrimitive, const FglTFBuffers& InBufferFiles, TArray<FVector>& OutVertexPositions);
    static bool GetVertexNormals(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SMeshPrimitive>& InMeshPrimitive, const FglTFBuffers& InBufferFiles, TArray<FVector>& OutVertexNormals);
    static bool GetVertexTangents(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SMeshPrimitive>& InMeshPrimitive, const FglTFBuffers& InBufferFiles, TArray<FVector4>& OutVertexTangents);
    static bool GetVertexTexcoords(const std::shared_ptr<libgltf::SGlTF>& InGlTF, const std::shared_ptr<libgltf::SMeshPrimitive>& InMeshPrimitive, const FglTFBuffers& InBufferFiles, TArray<FVector2D> OutVertexTexcoords[MAX_STATIC_TEXCOORDS]);

public:
    static void SwapYZ(FVector& InOutValue);
    static void SwapYZ(TArray<FVector>& InOutValues);
    static FString SanitizeObjectName(const FString& InObjectName);

    static TextureFilter MagFilterToTextureFilter(int32 InValue);
    static TextureFilter MinFilterToTextureFilter(int32 InValue);
    static TextureAddress WrapSToTextureAddress(int32 InValue);
    static TextureAddress WrapTToTextureAddress(int32 InValue);
};
