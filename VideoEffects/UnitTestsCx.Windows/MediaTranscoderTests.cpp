#include "pch.h"

using namespace concurrency;
using namespace Microsoft::Graphics::Canvas;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Lumia::Imaging;
using namespace Lumia::Imaging::Artistic;
using namespace Lumia::Imaging::Transforms;
using namespace Platform;
using namespace Platform::Collections;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Media::Transcoding;
using namespace Windows::Storage;
using namespace Windows::Storage::FileProperties;
using namespace Windows::Storage::Streams;
using namespace Windows::UI;

// Note: this does not work - ExampleFilterChain must be in a separate WinRT DLL
//namespace UnitTests
//{
//    public ref class ExampleFilterChain sealed : IFilterChainFactory
//    {
//    public:
//        virtual IIterable<IFilter^>^ Create()
//        {
//            auto filters = ref new Vector<IFilter^>();
//            filters->Append(ref new AntiqueFilter());
//            filters->Append(ref new FlipFilter(FlipMode::Horizontal));
//            return filters;
//        }
//    };
//}
//using namespace UnitTests;

ref class AnimatedWarp sealed : public IAnimatedFilterChain
{
public:

    virtual property IIterable<IFilter^>^ Filters;

    virtual void UpdateTime(TimeSpan time)
    {
        _filter->Level = .5 * (sin(2. * M_PI * time.Duration / 10000000.) + 1); // 1Hz oscillation between 0 and 1
    }

    AnimatedWarp()
    {
        _filter = ref new WarpFilter(WarpEffect::Twister, 0.);
        auto filters = ref new Vector<IFilter^>();
        filters->Append(_filter);
        Filters = filters;
    }

private:
    WarpFilter^ _filter;
};

ref class TextEffect sealed : public ICanvasVideoEffect
{
public:

    virtual void Process(_In_ CanvasBitmap^ input, _In_ CanvasRenderTarget^ output, TimeSpan time)
    {
        CanvasDrawingSession^ session = output->CreateDrawingSession();
        session->DrawImage(input);
        float p = 2.f * (float)M_PI * time.Duration / 10000000.f;
        float x = (cosf(p) + 1.f) * (float)input->SizeInPixels.Width / 4.f;
        float y = (sinf(p) + 1.f) * (float)input->SizeInPixels.Height / 4.f;
        session->DrawText(L"Canvas Effect test", x, y, Colors::Red);
    }
};

TEST_CLASS(MediaTranscoderTests)
{
public:
    TEST_METHOD(CX_W_MT_Lumia)
    {
        TestLumia(L"Car.mp4", L"CX_W_MT_Lumia.mp4");
    }

    TEST_METHOD(CX_W_MT_Lumia_RotatedVideo)
    {
        TestLumia(L"OriginalR.mp4", L"CX_W_MT_Lumia_RotatedVideo.mp4");
    }

    void TestLumia(String^ inputFileName, String^ outputFileName)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/" + inputFileName)));
        StorageFile^ destination = CreateDestinationFile(outputFileName);

        // Note: this does not work - ExampleFilterChain must be in a separate WinRT DLL
        // auto definition = ref new LumiaEffectDefinition(ExampleFilterChain().GetType()->FullName);

        auto definition = ref new LumiaEffectDefinition(ref new FilterChainFactory([]()
        {
            auto filters = ref new Vector<IFilter^>();
            filters->Append(ref new AntiqueFilter());
            filters->Append(ref new FlipFilter(FlipMode::Horizontal));
            return filters;
        }));

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Qvga)));
        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_LumiaCropSquare)
    {
        TestLumiaCropSquare(L"Car.mp4", L"CX_W_MT_LumiaCropSquare.mp4");
    }

    TEST_METHOD(CX_W_MT_LumiaCropSquare_RotatedVideo)
    {
        TestLumiaCropSquare(L"OriginalR.mp4", L"CX_W_MT_LumiaCropSquare_RotatedVideo.mp4");
    }

    void TestLumiaCropSquare(String^ inputFileName, String^ outputFileName)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/" + inputFileName)));
        StorageFile^ destination = CreateDestinationFile(outputFileName);

        // Select the largest centered square area in the input video
        auto encodingProfile = Await(TranscodingProfile::CreateFromFileAsync(source));
        unsigned int inputWidth = encodingProfile->Video->Width;
        unsigned int inputHeight = encodingProfile->Video->Height;
        unsigned int outputLength = min(inputWidth, inputHeight);
        Rect cropArea(
            (float)((inputWidth - outputLength) / 2),
            (float)((inputHeight - outputLength) / 2),
            (float)outputLength,
            (float)outputLength
            );
        encodingProfile->Video->Width = outputLength;
        encodingProfile->Video->Height = outputLength;

        auto definition = ref new LumiaEffectDefinition(ref new FilterChainFactory([cropArea]()
        {
            auto filters = ref new Vector<IFilter^>();
            filters->Append(ref new CropFilter(cropArea));
            return filters;
        }));
        definition->InputWidth = inputWidth;
        definition->InputHeight = inputHeight;
        definition->OutputWidth = outputLength;
        definition->OutputHeight = outputLength;

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, encodingProfile));
        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_CanvasEffect)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/Car.mp4")));
        StorageFile^ destination = CreateDestinationFile(L"CX_W_MT_CanvasEffect.mp4");

        auto definition = ref new CanvasEffectDefinition(ref new CanvasVideoEffectFactory([]()
        {
            return ref new TextEffect();
        }));

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Qvga)));

        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_ShaderBgrx8)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/Car.mp4")));
        StorageFile^ destination = CreateDestinationFile(L"CX_W_MT_ShaderBgrx8.mp4");

        auto shader = Await(PathIO::ReadBufferAsync("ms-appx:///Invert_100_RGB32.cso"));
        auto definition = ref new ShaderEffectDefinitionBgrx8(shader);

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Qvga)));

        definition->UpdateShader(shader);

        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_ShaderNv12)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/Car.mp4")));
        StorageFile^ destination = CreateDestinationFile(L"CX_W_MT_ShaderNv12.mp4");

        auto shaderY = Await(PathIO::ReadBufferAsync("ms-appx:///Invert_100_NV12_Y.cso"));
        auto shaderUV = Await(PathIO::ReadBufferAsync("ms-appx:///Invert_100_NV12_UV.cso"));
        auto definition = ref new ShaderEffectDefinitionNv12(shaderY, shaderUV);

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Qvga)));

        definition->UpdateShader(shaderY, shaderUV);

        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_Square)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/Car.mp4")));
        StorageFile^ destination = CreateDestinationFile(L"CX_W_MT_Square_bad.mp4");

        // Compute the output resolution
        auto encodingProfile = Await(TranscodingProfile::CreateFromFileAsync(source));
        unsigned int inputWidth = encodingProfile->Video->Width;
        unsigned int inputHeight = encodingProfile->Video->Height;
        unsigned int outputLength = min(inputWidth, inputHeight);
        encodingProfile->Video->Width = outputLength;
        encodingProfile->Video->Height = outputLength;

        auto definition = ref new SquareEffectDefinition();

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, encodingProfile));
        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_AnimatedWarp)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/Car.mp4")));
        StorageFile^ destination = CreateDestinationFile(L"CX_W_MT_AnimatedWarp.mp4");

        auto definition = ref new LumiaEffectDefinition(ref new AnimatedFilterChainFactory([]()
        {
            return ref new AnimatedWarp();
        }));

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Qvga)));
        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_Analysis_Bgra8888)
    {
        TestAnalysis(L"ms-appx:///Input/Car.mp4", L"CX_W_MT_Analysis_Bgra8888.mp4", ColorMode::Bgra8888);
    }

    TEST_METHOD(CX_W_MT_Analysis_Yuv420Sp)
    {
        TestAnalysis(L"ms-appx:///Input/Car.mp4", L"CX_W_MT_Analysis_Yuv420Sp.mp4", ColorMode::Yuv420Sp);
    }

    TEST_METHOD(CX_W_MT_Analysis_Gray8)
    {
        TestAnalysis(L"ms-appx:///Input/Car.mp4", L"CX_W_MT_Analysis_Gray8.mp4", ColorMode::Gray8);
    }

    void TestAnalysis(String^ inputFileName, String^ outputFileName, ColorMode colorMode)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(inputFileName)));
        StorageFile^ destination = CreateDestinationFile(outputFileName);

        unsigned int callbackCount = 0;
        auto definition = ref new LumiaAnalyzerDefinition(
            colorMode,
            320, 
            ref new BitmapVideoAnalyzer([&callbackCount, colorMode](Bitmap^ bitmap, TimeSpan /*time*/)
        {
            Assert::IsNotNull(bitmap);
            Assert::AreEqual((int)colorMode, (int)bitmap->ColorMode);
            Assert::AreEqual(320, (int)bitmap->Dimensions.Width);
            Assert::AreEqual(240, (int)bitmap->Dimensions.Height);
            InterlockedIncrement(&callbackCount);
        }));

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Vga)));
        Await(transcode->TranscodeAsync());

        Log() << L"Callback count: " << callbackCount;

        Assert::IsTrue(callbackCount > 0);
    }

    StorageFile^ CreateDestinationFile(String^ filename)
    {
        StorageFolder^ folder = Await(KnownFolders::VideosLibrary->CreateFolderAsync(L"UnitTests.VideoEffects", CreationCollisionOption::OpenIfExists));
        return Await(folder->CreateFileAsync(filename, CreationCollisionOption::ReplaceExisting));
    }
};