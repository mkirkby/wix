//-------------------------------------------------------------------------------------------------
// <copyright file="host.cpp" company="Microsoft">
// Copyright (c) Microsoft Corporation. All rights reserved.
//    
//    The use and distribution terms for this software are covered by the
//    Common Public License 1.0 (http://opensource.org/licenses/cpl1.0.php)
//    which can be found in the file CPL.TXT at the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by
//    the terms of this license.
//    
//    You must not remove this notice, or any other, from this software.
// </copyright>
// 
// <summary>
// Source for the managed bootstrapper application host.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"
#include <BootstrapperCore.h> // includes the generated assembly name macros.

using namespace mscorlib;

extern "C" typedef HRESULT (WINAPI *PFN_CORBINDTOCURRENTRUNTIME)(
    __in LPCWSTR pwszFileName,
    __in REFCLSID rclsid,
    __in REFIID riid,
    __out LPVOID *ppv
    );

static HINSTANCE vhInstance = NULL;
static ICorRuntimeHost *vpCLRHost = NULL;
static HMODULE vhMbapreqModule = NULL;


// internal function declarations

static HRESULT GetAppDomain(
    __out _AppDomain** ppAppDomain
    );
static HRESULT GetAppBase(
    __out LPWSTR* psczAppBase
    );
static HRESULT CheckSupportedFrameworks(
    __in LPCWSTR wzConfigPath
    );
static HRESULT GetCLRHost(
    __in LPCWSTR wzConfigPath,
    __out ICorRuntimeHost** ppCLRHost
    );
static HRESULT CreateManagedBootstrapperApplication(
    __in _AppDomain* pAppDomain,
    __in IBootstrapperEngine* pEngine,
    __in const BOOTSTRAPPER_COMMAND* pCommand,
    __out IBootstrapperApplication** ppApplication
    );
static HRESULT CreateManagedBootstrapperApplicationFactory(
    __in _AppDomain* pAppDomain,
    __out IBootstrapperApplicationFactory** ppAppFactory
    );
static HRESULT CreatePrerequisiteBA(
    __in IBootstrapperEngine* pEngine,
    __in const BOOTSTRAPPER_COMMAND* pCommand,
    __out IBootstrapperApplication** ppApplication
    );


// function definitions

extern "C" BOOL WINAPI DllMain(
    IN HINSTANCE hInstance,
    IN DWORD dwReason,
    IN LPVOID /* pvReserved */
    )
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        ::DisableThreadLibraryCalls(hInstance);
        vhInstance = hInstance;
        break;

    case DLL_PROCESS_DETACH:
        vhInstance = NULL;
        break;
    }

    return TRUE;
}

// Note: This function assumes that COM was already initialized on the thread.
extern "C" HRESULT WINAPI BootstrapperApplicationCreate(
    __in IBootstrapperEngine* pEngine,
    __in const BOOTSTRAPPER_COMMAND* pCommand,
    __out IBootstrapperApplication** ppBA
    )
{
   HRESULT hr = S_OK; 
    _AppDomain* pAppDomain = NULL;

    BalInitialize(pEngine);

    hr = GetAppDomain(&pAppDomain);
    if (SUCCEEDED(hr))
    {
        BalLog(BOOTSTRAPPER_LOG_LEVEL_STANDARD, "Loading managed bootstrapper application.");

        hr = CreateManagedBootstrapperApplication(pAppDomain, pEngine, pCommand, ppBA);
        BalExitOnFailure(hr, "Failed to create the managed bootstrapper application.");
    }
    else // fallback to the pre-requisite BA.
    {
        BalLog(BOOTSTRAPPER_LOG_LEVEL_STANDARD, "Loading prerequisite bootstrapper application because managed host could not be loaded, error: 0x%08x.", hr);

        hr = CreatePrerequisiteBA(pEngine, pCommand, ppBA);
        BalExitOnFailure(hr, "Failed to create the pre-requisite bootstrapper application.");
    }

LExit:
    ReleaseObject(pAppDomain);

    return hr;
}

extern "C" void WINAPI BootstrapperApplicationDestroy()
{
    ReleaseNullObject(vpCLRHost);

    if (vhMbapreqModule)
    {
        PFN_BOOTSTRAPPER_APPLICATION_DESTROY pfnDestroy = reinterpret_cast<PFN_BOOTSTRAPPER_APPLICATION_DESTROY>(::GetProcAddress(vhMbapreqModule, "BootstrapperApplicationDestroy"));
        if (pfnDestroy)
        {
            (*pfnDestroy)();
        }

        ::FreeLibrary(vhMbapreqModule);
        vhMbapreqModule = NULL;
    }

    BalUninitialize();
}

// Gets the custom AppDomain for loading managed BA.
static HRESULT GetAppDomain(
    __out _AppDomain **ppAppDomain
    )
{
    HRESULT hr = S_OK;
    ICorRuntimeHost *pCLRHost = NULL;
    IUnknown *pUnk = NULL;
    LPWSTR sczAppBase = NULL;
    LPWSTR sczConfigPath = NULL;
    IAppDomainSetup *pAppDomainSetup;
    BSTR bstrAppBase = NULL;
    BSTR bstrConfigPath = NULL;

    hr = GetAppBase(&sczAppBase);
    ExitOnFailure(hr, "Failed to get the host base path.");

    hr = PathConcat(sczAppBase, L"BootstrapperCore.config", &sczConfigPath);
    ExitOnFailure(hr, "Failed to get the full path to the application configuration file.");

    // Check that the supported framework is installed.
    hr = CheckSupportedFrameworks(sczConfigPath);
    ExitOnFailure(hr, "Failed to find supported framework.");

    // Load the CLR.
    hr = GetCLRHost(sczConfigPath, &pCLRHost);
    ExitOnFailure(hr, "Failed to create the CLR host.");

    hr = pCLRHost->Start();
    ExitOnRootFailure(hr, "Failed to start the CLR host.");

    // Create the setup information for a new AppDomain to set the app base and config.
    hr = pCLRHost->CreateDomainSetup(&pUnk);
    ExitOnRootFailure(hr, "Failed to create the AppDomainSetup object.");

    hr = pUnk->QueryInterface(__uuidof(IAppDomainSetup), reinterpret_cast<LPVOID*>(&pAppDomainSetup));
    ExitOnRootFailure(hr, "Failed to query for the IAppDomainSetup interface.");
    ReleaseNullObject(pUnk);

    // Set properties on the AppDomainSetup object.
    bstrAppBase = ::SysAllocString(sczAppBase);
    ExitOnNull(bstrAppBase, hr, E_OUTOFMEMORY, "Failed to allocate the application base path for the AppDomainSetup.");

    hr = pAppDomainSetup->put_ApplicationBase(bstrAppBase);
    ExitOnRootFailure(hr, "Failed to set the application base path for the AppDomainSetup.");

    bstrConfigPath = ::SysAllocString(sczConfigPath);
    ExitOnNull(bstrConfigPath, hr, E_OUTOFMEMORY, "Failed to allocate the application configuration file for the AppDomainSetup.");

    hr = pAppDomainSetup->put_ConfigurationFile(bstrConfigPath);
    ExitOnRootFailure(hr, "Failed to set the configuration file path for the AppDomainSetup.");

    // Create the AppDomain to load the factory type.
    hr = pCLRHost->CreateDomainEx(L"MBA", pAppDomainSetup, NULL, &pUnk);
    ExitOnRootFailure(hr, "Failed to create the MBA AppDomain.");

    hr = pUnk->QueryInterface(__uuidof(_AppDomain), reinterpret_cast<LPVOID*>(ppAppDomain));
    ExitOnRootFailure(hr, "Failed to query for the _AppDomain interface.");

LExit:
    ReleaseBSTR(bstrConfigPath);
    ReleaseBSTR(bstrAppBase);
    ReleaseStr(sczConfigPath);
    ReleaseStr(sczAppBase);
    ReleaseNullObject(pUnk);
    ReleaseNullObject(pCLRHost);

    return hr;
}

static HRESULT GetAppBase(
    __out LPWSTR *psczAppBase
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczFullPath = NULL;

    hr = PathForCurrentProcess(&sczFullPath, vhInstance);
    ExitOnFailure(hr, "Failed to get the full host path.");

    hr = PathGetDirectory(sczFullPath, psczAppBase);
    ExitOnFailure(hr, "Failed to get the directory of the full process path.");

LExit:
    ReleaseStr(sczFullPath);

    return hr;
}

// Checks whether at least one of required supported frameworks is installed via the NETFX registry keys.
static HRESULT CheckSupportedFrameworks(
    __in LPCWSTR wzConfigPath
    )
{
    HRESULT hr = S_OK;
    IXMLDOMDocument* pixdManifest = NULL;
    IXMLDOMNodeList* pNodeList = NULL;
    IXMLDOMNode* pNode = NULL;
    DWORD cSupportedFrameworks = 0;
    LPWSTR sczSupportedFrameworkVersion = NULL;
    LPWSTR sczFrameworkRegistryKey = NULL;
    HKEY hkFramework = NULL;
    DWORD dwFrameworkInstalled = 0;

    hr = XmlInitialize();
    ExitOnFailure(hr, "Failed to initialize XML.");

    hr = XmlLoadDocumentFromFile(wzConfigPath, &pixdManifest);
    ExitOnFailure1(hr, "Failed to load bootstrapper config file from path: %ls", wzConfigPath);

    hr = XmlSelectNodes(pixdManifest, L"/configuration/wix.bootstrapper/host/supportedFramework", &pNodeList);
    ExitOnFailure(hr, "Failed to select all supportedFramework elements.");

    hr = pNodeList->get_length(reinterpret_cast<long*>(&cSupportedFrameworks));
    ExitOnFailure(hr, "Failed to get the supported framework count.");

    if (cSupportedFrameworks)
    {
        while (S_OK == (hr = XmlNextElement(pNodeList, &pNode, NULL)))
        {
            hr = XmlGetAttributeEx(pNode, L"version", &sczSupportedFrameworkVersion);
            ExitOnFailure(hr, "Failed to get supportedFramework/@version.");

            hr = StrAllocFormatted(&sczFrameworkRegistryKey, L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\%ls", sczSupportedFrameworkVersion);
            ExitOnFailure(hr, "Failed to allocate path to supported framework Install registry key.");

            hr = RegOpen(HKEY_LOCAL_MACHINE, sczFrameworkRegistryKey, KEY_READ, &hkFramework);
            if (SUCCEEDED(hr))
            {
                hr = RegReadNumber(hkFramework, L"Install", &dwFrameworkInstalled);
                if (dwFrameworkInstalled)
                {
                    hr = S_OK;
                    break;
                }
            }

            ReleaseNullObject(pNode);
        }

        // If we looped through all the supported frameworks but didn't find anything, ensure we return a failure.
        if (S_FALSE == hr)
        {
            hr = E_NOTFOUND;
        }
    }
    // else no supported frameworks specified, so the startup/supportedRuntime must be enough.

LExit:
    ReleaseRegKey(hkFramework);
    ReleaseStr(sczFrameworkRegistryKey);
    ReleaseStr(sczSupportedFrameworkVersion);
    ReleaseObject(pNode);
    ReleaseObject(pNodeList);
    ReleaseObject(pixdManifest);

    XmlUninitialize();

    return hr;
}

// Gets the CLR host and caches it.
static HRESULT GetCLRHost(
    __in LPCWSTR wzConfigPath,
    __out ICorRuntimeHost **ppCLRHost
    )
{
    HRESULT hr = S_OK;
    UINT uiMode = 0;
    HMODULE hModule = NULL;
    PFN_CORBINDTOCURRENTRUNTIME pfnCorBindToCurrentRuntime = NULL;

    // Always set the error mode because we will always restore it below.
    uiMode = ::SetErrorMode(0);

    // Cache the CLR host to be shutdown later. This can occur on a different thread.
    if (!vpCLRHost)
    {
        // Disable message boxes from being displayed on error and blocking execution.
        ::SetErrorMode(uiMode | SEM_FAILCRITICALERRORS);

        hr = LoadSystemLibrary(L"mscoree.dll", &hModule);
        ExitOnFailure(hr, "Failed to load mscoree.dll");

        pfnCorBindToCurrentRuntime = reinterpret_cast<PFN_CORBINDTOCURRENTRUNTIME>(::GetProcAddress(hModule, "CorBindToCurrentRuntime"));
        ExitOnNullWithLastError(pfnCorBindToCurrentRuntime, hr, "Failed to get procedure address for CorBindToCurrentRuntime.");

        hr = pfnCorBindToCurrentRuntime(wzConfigPath, CLSID_CorRuntimeHost, IID_ICorRuntimeHost, reinterpret_cast<LPVOID*>(&vpCLRHost));
        ExitOnRootFailure(hr, "Failed to create the CLR host using the application configuration file path.");
    }

    vpCLRHost->AddRef();
    *ppCLRHost = vpCLRHost;

LExit:
    // Unload the module so it's not in use when we install .NET.
    if (FAILED(hr))
    {
        ::FreeLibrary(hModule);
    }

    ::SetErrorMode(uiMode); // restore the previous error mode.

    return hr;
}

// Creates the bootstrapper app and returns it for the engine.
static HRESULT CreateManagedBootstrapperApplication(
    __in _AppDomain* pAppDomain,
    __in IBootstrapperEngine* pEngine,
    __in const BOOTSTRAPPER_COMMAND* pCommand,
    __out IBootstrapperApplication** ppApplication
    )
{
    HRESULT hr = S_OK;
    IBootstrapperApplicationFactory* pAppFactory = NULL;

    hr = CreateManagedBootstrapperApplicationFactory(pAppDomain, &pAppFactory);
    ExitOnFailure(hr, "Failed to create the factory to create the bootstrapper application.");

    hr = pAppFactory->Create(pEngine, pCommand, ppApplication);
    ExitOnFailure(hr, "Failed to create the bootstrapper application.");

LExit:
    ReleaseNullObject(pAppFactory);

    return hr;
}

// Creates the app factory to create the managed app in the default AppDomain.
static HRESULT CreateManagedBootstrapperApplicationFactory(
    __in _AppDomain* pAppDomain,
    __out IBootstrapperApplicationFactory** ppAppFactory
    )
{
    HRESULT hr = S_OK;
    BSTR bstrAssemblyName = NULL;
    BSTR bstrTypeName = NULL;
    _ObjectHandle* pObj = NULL;
    VARIANT vtBAFactory;

    ::VariantInit(&vtBAFactory);

    bstrAssemblyName = ::SysAllocString(MUX_ASSEMBLY_FULL_NAME);
    ExitOnNull(bstrAssemblyName, hr, E_OUTOFMEMORY, "Failed to allocate the full assembly name for the bootstrapper application factory.");

    bstrTypeName = ::SysAllocString(L"Microsoft.Tools.WindowsInstallerXml.Bootstrapper.BootstrapperApplicationFactory");
    ExitOnNull(bstrTypeName, hr, E_OUTOFMEMORY, "Failed to allocate the full type name for the BA factory.");

    hr = pAppDomain->CreateInstance(bstrAssemblyName, bstrTypeName, &pObj);
    ExitOnRootFailure(hr, "Failed to create the BA factory object.");

    hr = pObj->Unwrap(&vtBAFactory);
    ExitOnRootFailure(hr, "Failed to unwrap the BA factory object into the host domain.");
    ExitOnNull(vtBAFactory.punkVal, hr, E_UNEXPECTED, "The variant did not contain the expected IUnknown pointer.");

    hr = vtBAFactory.punkVal->QueryInterface(__uuidof(IBootstrapperApplicationFactory), reinterpret_cast<LPVOID*>(ppAppFactory));
    ExitOnRootFailure(hr, "Failed to query for the bootstrapper app factory interface.");

LExit:
    ReleaseVariant(vtBAFactory);
    ReleaseNullObject(pObj);
    ReleaseBSTR(bstrTypeName);
    ReleaseBSTR(bstrAssemblyName);

    return hr;
}

static HRESULT CreatePrerequisiteBA(
    __in IBootstrapperEngine* pEngine,
    __in const BOOTSTRAPPER_COMMAND* pCommand,
    __out IBootstrapperApplication** ppApplication
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczMbapreqPath = NULL;
    HMODULE hModule = NULL;
    IBootstrapperApplication* pApp = NULL;

    hr = PathRelativeToModule(&sczMbapreqPath, L"mbapreq.dll", vhInstance);
    ExitOnFailure(hr, "Failed to get path to pre-requisite BA.");

    hModule = ::LoadLibraryW(sczMbapreqPath);
    ExitOnNullWithLastError(hModule, hr, "Failed to load pre-requisite BA DLL.");

    PFN_BOOTSTRAPPER_APPLICATION_CREATE pfnCreate = reinterpret_cast<PFN_BOOTSTRAPPER_APPLICATION_CREATE>(::GetProcAddress(hModule, "BootstrapperApplicationCreate"));
    ExitOnNullWithLastError1(pfnCreate, hr, "Failed to get BootstrapperApplicationCreate entry-point from: %ls", sczMbapreqPath);

    hr = pfnCreate(pEngine, pCommand, &pApp);
    ExitOnFailure(hr, "Failed to create prequisite bootstrapper app.");

    vhMbapreqModule = hModule;
    hModule = NULL;

    *ppApplication = pApp;
    pApp = NULL;

LExit:
    ReleaseObject(pApp);
    if (hModule)
    {
        ::FreeLibrary(hModule);
    }
    ReleaseStr(sczMbapreqPath);

    return hr;
}
