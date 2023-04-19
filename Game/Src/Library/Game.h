#pragma once

#include "Common.h"
#include "GameTimer.h"
#include "Singleton.h"

using namespace Library;

namespace Library
{
    class Game
    {
    public:
        Game(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand);
        ~Game();

        HINSTANCE Instance() const;
        HWND WindowHandle() const;
        const WNDCLASSEX& Window() const;
        const std::wstring& WindowClass() const;
        const std::wstring& WindowTitle() const;
        int ScreenWidth() const;
        int ScreenHeight() const;

        virtual void Run();
        virtual void Exit();
        virtual void Initialize();
        virtual void Update(const GameTimer& gt);
        virtual void Draw();

    protected:
        virtual void InitializeWindow();
        virtual void InitDirectX();
        virtual void Shutdown();
        virtual void OnResize();
        void CreateCommandObjects();
        void CreateSwapChain();
        void CreateRtvAndDsvDescriptorHeaps();
        void FlushCommandQueue();

        ID3D12Resource* CurrentBackBuffer()const;
        D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
        D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

        static const UINT DefaultScreenWidth;
        static const UINT DefaultScreenHeight;
        static const UINT DefaultFrameRate;
        static const UINT DefaultMultiSamplingCount;

        HINSTANCE mInstance;
        std::wstring mWindowClass;
        std::wstring mWindowTitle;
        int mShowCommand;

        HWND mWindowHandle;
        WNDCLASSEX mWindow;

        UINT mScreenWidth;
        UINT mScreenHeight;

        GameTimer mGameTimer;

        ComPtr<ID3D12Device> mDirect3DDevice;
        ComPtr<IDXGIFactory4> mDxgiFactory;
        ComPtr<IDXGISwapChain> mSwapChain;

        ComPtr<ID3D12Fence> mFence;
        UINT64 mCurrentFence = 0;

        ComPtr<ID3D12CommandQueue> mCommandQueue;
        ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
        ComPtr<ID3D12GraphicsCommandList> mCommandList;

        static const int mSwapChainBufferCount = 2;
        int mCurrBackBuffer = 0;
        ComPtr<ID3D12Resource> mSwapChainBuffer[mSwapChainBufferCount];
        ComPtr<ID3D12Resource> mDepthStencilBuffer;

        ComPtr<ID3D12DescriptorHeap> mRtvHeap;
        ComPtr<ID3D12DescriptorHeap> mDsvHeap;

        D3D12_VIEWPORT mScreenViewport;
        D3D12_RECT mScissorRect;

        UINT mRtvDescriptorSize = 0;
        UINT mDsvDescriptorSize = 0;
        UINT mCbvSrvUavDescriptorSize = 0;

        DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        bool m4xMsaaEnable = false;
        UINT m4xMsaaQuality = 0;// quality level of 4X MSAA

        void LogAdapters();
        void LogAdapterOutputs(IDXGIAdapter* adapter);
        void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

    private:
        Game(const Game& rhs) = delete;
        Game& operator=(const Game& rhs) = delete;

        POINT CenterWindow(int windowWidth, int windowHeight);
        static LRESULT WINAPI WndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
    };
}