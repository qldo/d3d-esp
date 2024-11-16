#include <Windows.h>
#include <d3d9.h>
#include <iostream>
#include "minhook/MinHook.h" // MinHook for hooking
#pragma comment(lib, "d3d9.lib")

typedef HRESULT(APIENTRY* DrawIndexedPrimitiveFn)(
    IDirect3DDevice9*,
    D3DPRIMITIVETYPE,
    INT,
    UINT,
    UINT,
    UINT,
    UINT);

typedef HRESULT(APIENTRY* SetStreamSourceFn)(
    IDirect3DDevice9*,
    UINT,
    IDirect3DVertexBuffer9*,
    UINT,
    UINT);

DrawIndexedPrimitiveFn OriginalDrawIndexedPrimitive = nullptr;
SetStreamSourceFn OriginalSetStreamSource = nullptr;

UINT LastStride = 0; // Store the last known stride for inspection

// Function to create a console for debugging
void SetupConsole() {
    AllocConsole();
    FILE* fpOut;
    FILE* fpErr;

    freopen_s(&fpOut, "CONOUT$", "w", stdout);
    freopen_s(&fpErr, "CONOUT$", "w", stderr);

    std::cout << "[Debug] Console Initialized!" << std::endl;
}

// Hooked SetStreamSource
HRESULT APIENTRY HookedSetStreamSource(
    IDirect3DDevice9* pDevice,
    UINT StreamNumber,
    IDirect3DVertexBuffer9* pStreamData,
    UINT OffsetInBytes,
    UINT Stride
) {
    LastStride = Stride; // Capture the stride value
    //std::cout << "[Hooked] SetStreamSource Called!" << std::endl;
    //std::cout << "  StreamNumber: " << StreamNumber
   //     << ", OffsetInBytes: " << OffsetInBytes
    //    << ", Stride: " << Stride << std::endl;

    return OriginalSetStreamSource(pDevice, StreamNumber, pStreamData, OffsetInBytes, Stride);
}

HRESULT APIENTRY HookedDrawIndexedPrimitive(
    IDirect3DDevice9* pDevice,
    D3DPRIMITIVETYPE PrimitiveType,
    INT BaseVertexIndex,
    UINT MinVertexIndex,
    UINT NumVertices,
    UINT StartIndex,
    UINT PrimitiveCount
) {
    // Debug: Log stride
    //std::cout << "[Debug] LastStride: " << LastStride << std::endl;

    // Check for stride range (e.g., for character rendering)
    if (LastStride >= 40 && LastStride <= 100) {
        //std::cout << "[Stride Match] Applying through-wall effects!" << std::endl;

        // Disable depth testing
        pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);

        // Apply visual effects (e.g., wireframe mode)
        pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

        // Call the original function to render the character
        HRESULT result = OriginalDrawIndexedPrimitive(
            pDevice, PrimitiveType, BaseVertexIndex, MinVertexIndex,
            NumVertices, StartIndex, PrimitiveCount);

        // Restore original render states
        pDevice->SetRenderState(D3DRS_ZENABLE, TRUE); // Re-enable depth testing
        pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID); // Solid fill mode

        return result; // Exit after rendering
    }

    // Call the original function for non-matching strides
    return OriginalDrawIndexedPrimitive(
        pDevice, PrimitiveType, BaseVertexIndex, MinVertexIndex,
        NumVertices, StartIndex, PrimitiveCount);
}



// Hook setup
void SetupHook() {
    if (MH_Initialize() != MH_OK) {
        std::cerr << "[Error] Failed to initialize MinHook!" << std::endl;
        return;
    }

    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) {
        std::cerr << "[Error] Failed to create Direct3D9 object!" << std::endl;
        return;
    }

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = GetForegroundWindow();

    IDirect3DDevice9* pDevice = nullptr;
    if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDevice))) {
        std::cerr << "[Error] Failed to create Direct3D9 device!" << std::endl;
        pD3D->Release();
        return;
    }

    void** vTable = *reinterpret_cast<void***>(pDevice);

    // Log vtable for debugging
    for (int i = 0; i < 100; ++i) {
        std::cout << "vTable[" << i << "]: " << vTable[i] << std::endl;
    }

    // Hook DrawIndexedPrimitive
    int dipOffset = 82; // Typical offset for IDirect3DDevice9::DrawIndexedPrimitive
    if (MH_CreateHook(vTable[dipOffset], HookedDrawIndexedPrimitive, reinterpret_cast<void**>(&OriginalDrawIndexedPrimitive)) != MH_OK) {
        std::cerr << "[Error] Failed to create hook for DrawIndexedPrimitive!" << std::endl;
    }

    // Hook SetStreamSource
    int sssOffset = 100; // Typical offset for IDirect3DDevice9::SetStreamSource
    if (MH_CreateHook(vTable[sssOffset], HookedSetStreamSource, reinterpret_cast<void**>(&OriginalSetStreamSource)) != MH_OK) {
        std::cerr << "[Error] Failed to create hook for SetStreamSource!" << std::endl;
    }

    // Enable hooks
    MH_EnableHook(MH_ALL_HOOKS);
    std::cout << "[Debug] Hooks installed successfully!" << std::endl;

    // Cleanup
    pDevice->Release();
    pD3D->Release();
}

DWORD WINAPI HackThread(HMODULE hModule) {
    SetupConsole(); // Initialize debugging console
    SetupHook();    // Install the hook

    // Wait for user to press END key to unload
    while (!GetAsyncKeyState(VK_END)) {
        Sleep(100);
    }

    // Unhook and clean up
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall, LPVOID lpReserved) {
    if (ulReasonForCall == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)HackThread, hModule, 0, nullptr);
    }
    return TRUE;
}
