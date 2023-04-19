#include "Game.h"

namespace Library
{
    const UINT Game::DefaultScreenWidth = 1024;
    const UINT Game::DefaultScreenHeight = 768;

    Game::Game(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand)
        : mInstance(instance), mWindowClass(windowClass), mWindowTitle(windowTitle), mShowCommand(showCommand),
        mWindowHandle(), mWindow(),
        mScreenWidth(DefaultScreenWidth), mScreenHeight(DefaultScreenHeight),
        mGameTimer()
    {
    }

    Game::~Game()
    {
        if (mDirect3DDevice != nullptr)
            FlushCommandQueue();
    }

    HINSTANCE Game::Instance() const
    {
        return mInstance;
    }

    HWND Game::WindowHandle() const
    {
        return mWindowHandle;
    }

    const WNDCLASSEX& Game::Window() const
    {
        return mWindow;
    }

    const std::wstring& Game::WindowClass() const
    {
        return mWindowClass;
    }

    const std::wstring& Game::WindowTitle() const
    {
        return mWindowTitle;
    }

    int Game::ScreenWidth() const
    {
        return mScreenWidth;
    }

    int Game::ScreenHeight() const
    {
        return mScreenHeight;
    }

    void Game::Run()
    {
        Initialize();

        MSG message;
        ZeroMemory(&message, sizeof(message));

        mGameTimer.Start();

        while (message.message != WM_QUIT)
        {
            if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            else
            {
                mGameTimer.Tick();
                Update(mGameTimer);
                Draw();
            }
        }

        Shutdown();
    }

    void Game::Exit()
    {
        PostQuitMessage(0);
    }

    void Game::Initialize()
    {
        InitializeWindow();
        InitDirectX();
        OnResize();
    }

    void Game::Update(const GameTimer& gt)
    {
    }

    void Game::Draw()
    {
    }

    void Game::InitializeWindow()
    {
        ZeroMemory(&mWindow, sizeof(mWindow));
        mWindow.cbSize = sizeof(WNDCLASSEX);
        mWindow.style = CS_CLASSDC;
        mWindow.lpfnWndProc = WndProc;
        mWindow.hInstance = mInstance;
        mWindow.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        mWindow.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
        mWindow.hCursor = LoadCursor(nullptr, IDC_ARROW);
        mWindow.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        mWindow.lpszClassName = mWindowClass.c_str();

        RECT windowRectangle = { 0, 0, mScreenWidth, mScreenHeight };
        AdjustWindowRect(&windowRectangle, WS_OVERLAPPEDWINDOW, FALSE);

        RegisterClassEx(&mWindow);
        POINT center = CenterWindow(mScreenWidth, mScreenHeight);
        mWindowHandle = CreateWindow(mWindowClass.c_str(), mWindowTitle.c_str(), WS_OVERLAPPEDWINDOW, center.x, center.y, windowRectangle.right - windowRectangle.left, windowRectangle.bottom - windowRectangle.top, nullptr, nullptr, mInstance, nullptr);

        ShowWindow(mWindowHandle, mShowCommand);
        UpdateWindow(mWindowHandle);
    }

    void Game::InitDirectX()
    {
#if defined(DEBUG) || defined(_DEBUG) 
        // Enable the D3D12 debug layer.
        {
            ComPtr<ID3D12Debug> debugController;
            ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),"D3D12GetDebugInterface() failed");
            debugController->EnableDebugLayer();
        }
#endif
        ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mDxgiFactory)), "CreateDXGIFactory1() failed");
        // Try to create hardware device.
        HRESULT hardwareResult = D3D12CreateDevice(
            nullptr,             // default adapter
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&mDirect3DDevice));

        // Fallback to WARP device.
        if (FAILED(hardwareResult))
        {
            ComPtr<IDXGIAdapter> pWarpAdapter;
            ThrowIfFailed(mDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)), "EnumWarpAdapter() failed");

            ThrowIfFailed(D3D12CreateDevice(
                pWarpAdapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&mDirect3DDevice)), "D3D12CreateDevice() failed");
        }

        ThrowIfFailed(mDirect3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&mFence)), "CreateFence() failed");

        mRtvDescriptorSize = mDirect3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        mDsvDescriptorSize = mDirect3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        mCbvSrvUavDescriptorSize = mDirect3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        
        // Check 4X MSAA quality support for our back buffer format.
        // All Direct3D 11 capable devices support 4X MSAA for all render 
        // target formats, so we only need to check quality support.

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Format = mBackBufferFormat;
        msQualityLevels.SampleCount = 4;
        msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.NumQualityLevels = 0;
        ThrowIfFailed(mDirect3DDevice->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &msQualityLevels,
            sizeof(msQualityLevels)), "CheckFeatureSupport() failed");

        m4xMsaaQuality = msQualityLevels.NumQualityLevels;
        assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
        LogAdapters();
#endif

        CreateCommandObjects();
        CreateSwapChain();
        CreateRtvAndDsvDescriptorHeaps();
    }

    void Game::Shutdown()
    {
        UnregisterClass(mWindowClass.c_str(), mWindow.hInstance);
    }
    
    void Game::OnResize()
    {
        assert(mDirect3DDevice);
        assert(mSwapChain);
        assert(mDirectCmdListAlloc);

        // Flush before changing any resources.
        FlushCommandQueue();

        ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr), "mCommandList->Reset() failed");

        // Release the previous resources we will be recreating.
        for (int i = 0; i < mSwapChainBufferCount; ++i)
            mSwapChainBuffer[i].Reset();
        mDepthStencilBuffer.Reset();

        // Resize the swap chain.
        ThrowIfFailed(mSwapChain->ResizeBuffers(
            mSwapChainBufferCount,
            mScreenWidth, mScreenHeight,
            mBackBufferFormat,
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH), "mSwapChain->ResizeBuffers() failed");

        mCurrBackBuffer = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT i = 0; i < mSwapChainBufferCount; i++)
        {
            ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])), "mSwapChain->GetBuffer() failed");
            mDirect3DDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
            rtvHeapHandle.Offset(1, mRtvDescriptorSize);
        }

        // Create the depth/stencil buffer and view.
        D3D12_RESOURCE_DESC depthStencilDesc;
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = mScreenWidth;
        depthStencilDesc.Height = mScreenHeight;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;

        // Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
        // the depth buffer.  Therefore, because we need to create two views to the same resource:
        //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
        //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
        // we need to create the depth buffer resource with a typeless format.  
        depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

        depthStencilDesc.SampleDesc.Count = m4xMsaaEnable ? 4 : 1;
        depthStencilDesc.SampleDesc.Quality = m4xMsaaEnable ? (m4xMsaaQuality - 1) : 0;
        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear;
        optClear.Format = mDepthStencilFormat;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0;
        ThrowIfFailed(mDirect3DDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &optClear,
            IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())), "mDirect3DDevice->CreateCommittedResource() failed");

        // Create descriptor to mip level 0 of entire resource using the format of the resource.
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format = mDepthStencilFormat;
        dsvDesc.Texture2D.MipSlice = 0;
        mDirect3DDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

        // Transition the resource from its initial state to be used as a depth buffer.
        mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

        // Execute the resize commands.
        ThrowIfFailed(mCommandList->Close(), "mCommandList->Close() failed");
        ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
        mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

        // Wait until resize is complete.
        FlushCommandQueue();

        // Update the viewport transform to cover the client area.
        mScreenViewport.TopLeftX = 0;
        mScreenViewport.TopLeftY = 0;
        mScreenViewport.Width = static_cast<float>(mScreenWidth);
        mScreenViewport.Height = static_cast<float>(mScreenHeight);
        mScreenViewport.MinDepth = 0.0f;
        mScreenViewport.MaxDepth = 1.0f;

        mScissorRect = { 0, 0, (long)mScreenWidth, (long)mScreenHeight };
    }
    
    void Game::CreateCommandObjects()
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        ThrowIfFailed(mDirect3DDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)), "CreateCommandQueue() failed");

        ThrowIfFailed(mDirect3DDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())), "CreateCommandAllocator() failed");

        ThrowIfFailed(mDirect3DDevice->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            mDirectCmdListAlloc.Get(), // Associated command allocator
            nullptr,                   // Initial PipelineStateObject
            IID_PPV_ARGS(mCommandList.GetAddressOf())), "CreateCommandList() failed");

        // Start off in a closed state.  This is because the first time we refer 
        // to the command list we will Reset it, and it needs to be closed before
        // calling Reset.
        mCommandList->Close();
    }

    void Game::CreateSwapChain()
    {
        // Release the previous swapchain we will be recreating.
        mSwapChain.Reset();

        DXGI_SWAP_CHAIN_DESC sd;
        sd.BufferDesc.Width = mScreenWidth;
        sd.BufferDesc.Height = mScreenHeight;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = mBackBufferFormat;
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        sd.SampleDesc.Count = m4xMsaaEnable ? 4 : 1;
        sd.SampleDesc.Quality = m4xMsaaEnable ? (m4xMsaaQuality - 1) : 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = mSwapChainBufferCount;
        sd.OutputWindow = mWindowHandle;
        sd.Windowed = true;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        // Note: Swap chain uses queue to perform flush.
        ThrowIfFailed(mDxgiFactory->CreateSwapChain(
            mCommandQueue.Get(),
            &sd,
            mSwapChain.GetAddressOf()), "CreateSwapChain() failed");
    }

    void Game::CreateRtvAndDsvDescriptorHeaps()
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
        rtvHeapDesc.NumDescriptors = mSwapChainBufferCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDesc.NodeMask = 0;
        ThrowIfFailed(mDirect3DDevice->CreateDescriptorHeap(
            &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())), "CreateDescriptorHeap() failed");


        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;
        ThrowIfFailed(mDirect3DDevice->CreateDescriptorHeap(
            &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())), "CreateDescriptorHeap() failed");
    }

    void Game::FlushCommandQueue()
    {
        // Advance the fence value to mark commands up to this fence point.
        mCurrentFence++;

        // Add an instruction to the command queue to set a new fence point.  Because we 
        // are on the GPU timeline, the new fence point won't be set until the GPU finishes
        // processing all the commands prior to this Signal().
        ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence), "mCommandQueue->Signal() failed");

        // Wait until the GPU has completed commands up to this fence point.
        if (mFence->GetCompletedValue() < mCurrentFence)
        {
            HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

            // Fire event when GPU hits current fence.  
            ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle), "mFence->SetEventOnCompletion() failed");

            // Wait until the GPU hits current fence event is fired.
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }

    ID3D12Resource* Game::CurrentBackBuffer() const
    {
        return mSwapChainBuffer[mCurrBackBuffer].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Game::CurrentBackBufferView() const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
            mCurrBackBuffer,
            mRtvDescriptorSize);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Game::DepthStencilView() const
    {
        return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
    }

    LRESULT WINAPI Game::WndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProc(windowHandle, message, wParam, lParam);
    }

    void Game::LogAdapters()
    {
        UINT i = 0;
        IDXGIAdapter* adapter = nullptr;
        std::vector<IDXGIAdapter*> adapterList;
        while (mDxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_ADAPTER_DESC desc;
            adapter->GetDesc(&desc);

            std::wstring text = L"***Adapter: ";
            text += desc.Description;
            text += L"\n";

            OutputDebugString(text.c_str());

            adapterList.push_back(adapter);

            ++i;
        }

        for (size_t i = 0; i < adapterList.size(); ++i)
        {
            LogAdapterOutputs(adapterList[i]);
            ReleaseCom(adapterList[i]);
        }
    }

    void Game::LogAdapterOutputs(IDXGIAdapter* adapter)
    {
        UINT i = 0;
        IDXGIOutput* output = nullptr;
        while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_OUTPUT_DESC desc;
            output->GetDesc(&desc);

            std::wstring text = L"***Output: ";
            text += desc.DeviceName;
            text += L"\n";
            OutputDebugString(text.c_str());

            LogOutputDisplayModes(output, mBackBufferFormat);

            ReleaseCom(output);

            ++i;
        }
    }

    void Game::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
    {
        UINT count = 0;
        UINT flags = 0;

        // Call with nullptr to get list count.
        output->GetDisplayModeList(format, flags, &count, nullptr);

        std::vector<DXGI_MODE_DESC> modeList(count);
        output->GetDisplayModeList(format, flags, &count, &modeList[0]);

        for (auto& x : modeList)
        {
            UINT n = x.RefreshRate.Numerator;
            UINT d = x.RefreshRate.Denominator;
            std::wstring text =
                L"Width = " + std::to_wstring(x.Width) + L" " +
                L"Height = " + std::to_wstring(x.Height) + L" " +
                L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
                L"\n";

            ::OutputDebugString(text.c_str());
        }
    }

    POINT Game::CenterWindow(int windowWidth, int windowHeight)
    {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        POINT center;
        center.x = (screenWidth - windowWidth) / 2;
        center.y = (screenHeight - windowHeight) / 2;

        return center;
    }
}