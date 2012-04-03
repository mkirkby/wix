//-------------------------------------------------------------------------------------------------
// <copyright file="variable.cpp" company="Microsoft">
//    Copyright (c) Microsoft Corporation.  All rights reserved.
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
//    Module: Core
//
//    Variable managing functions for Burn.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"


// structs

typedef const struct _BUILT_IN_VARIABLE_DECLARATION
{
    LPCWSTR wzVariable;
    PFN_INITIALIZEVARIABLE pfnInitialize;
    DWORD_PTR dwpInitializeData;
} BUILT_IN_VARIABLE_DECLARATION;


// constants

const DWORD GROW_VARIABLE_ARRAY = 3;

enum OS_INFO_VARIABLE
{
    OS_INFO_VARIABLE_NONE,
    OS_INFO_VARIABLE_VersionNT,
    OS_INFO_VARIABLE_VersionNT64,
    OS_INFO_VARIABLE_ServicePackLevel,
    OS_INFO_VARIABLE_NTProductType,
    OS_INFO_VARIABLE_NTSuiteBackOffice,
    OS_INFO_VARIABLE_NTSuiteDataCenter,
    OS_INFO_VARIABLE_NTSuiteEnterprise,
    OS_INFO_VARIABLE_NTSuitePersonal,
    OS_INFO_VARIABLE_NTSuiteSmallBusiness,
    OS_INFO_VARIABLE_NTSuiteSmallBusinessRestricted,
    OS_INFO_VARIABLE_NTSuiteWebServer,
    OS_INFO_VARIABLE_CompatibilityMode,
    OS_INFO_VARIABLE_TerminalServer,
};


// internal function declarations

static HRESULT FormatString(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzIn,
    __out_z_opt LPWSTR* psczOut,
    __out_opt DWORD* pcchOut,
    __in BOOL fObfuscateHiddenVariables
    );
static HRESULT AddBuiltInVariable(
    __in BURN_VARIABLES* pVariables,
    __in LPCWSTR wzVariable,
    __in PFN_INITIALIZEVARIABLE pfnInitialize,
    __in DWORD_PTR dwpInitializeData
    );
static HRESULT GetVariable(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out BURN_VARIABLE** ppVariable
    );
static HRESULT FindVariableIndexByName(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out DWORD* piVariable
    );
static HRESULT InsertVariable(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in DWORD iPosition
    );
static HRESULT SetVariableValue(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in BURN_VARIANT* pVariant,
    __in BOOL fOverwriteBuiltIn,
    __in BOOL fLog
    );
static HRESULT InitializeVariableOsInfo(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableVersionMsi(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableCsidlFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableWindowsVolumeFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableTempFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableSystemFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariablePrivileged(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableRebootPending(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeSystemLanguageID(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeUserLanguageID(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableString(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableNumeric(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableRegistryFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableDate(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableInstallerName(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableInstallerVersion(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT InitializeVariableLogonUser(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    );
static HRESULT IsVariableHidden(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out BOOL* pfHidden
    );


// function definitions

extern "C" HRESULT VariableInitialize(
    __in BURN_VARIABLES* pVariables
    )
{
    HRESULT hr = S_OK;

    ::InitializeCriticalSection(&pVariables->csAccess);

    const BUILT_IN_VARIABLE_DECLARATION vrgBuiltInVariables[] = {
        {L"AdminToolsFolder", InitializeVariableCsidlFolder, CSIDL_ADMINTOOLS},
        {L"AppDataFolder", InitializeVariableCsidlFolder, CSIDL_APPDATA},
        {L"CommonAppDataFolder", InitializeVariableCsidlFolder, CSIDL_COMMON_APPDATA},
#if defined(_WIN64)
        {L"CommonFiles64Folder", InitializeVariableCsidlFolder, CSIDL_PROGRAM_FILES_COMMON},
#else
        {L"CommonFiles64Folder", InitializeVariableRegistryFolder, CSIDL_PROGRAM_FILES_COMMON},
#endif
        {L"CommonFilesFolder", InitializeVariableCsidlFolder, CSIDL_PROGRAM_FILES_COMMONX86},
        {L"CompatibilityMode", InitializeVariableOsInfo, OS_INFO_VARIABLE_CompatibilityMode},
        {VARIABLE_DATE, InitializeVariableDate, 0},
        {L"DesktopFolder", InitializeVariableCsidlFolder, CSIDL_DESKTOP},
        {L"FavoritesFolder", InitializeVariableCsidlFolder, CSIDL_FAVORITES},
        {L"FontsFolder", InitializeVariableCsidlFolder, CSIDL_FONTS},
        {VARIABLE_INSTALLERNAME, InitializeVariableInstallerName, 0},
        {VARIABLE_INSTALLERVERSION, InitializeVariableInstallerVersion, 0},
        {L"LocalAppDataFolder", InitializeVariableCsidlFolder, CSIDL_LOCAL_APPDATA},
        {VARIABLE_LOGONUSER, InitializeVariableLogonUser, 0},
        {L"MyPicturesFolder", InitializeVariableCsidlFolder, CSIDL_MYPICTURES},
        {L"NTProductType", InitializeVariableOsInfo, OS_INFO_VARIABLE_NTProductType},
        {L"NTSuiteBackOffice", InitializeVariableOsInfo, OS_INFO_VARIABLE_NTSuiteBackOffice},
        {L"NTSuiteDataCenter", InitializeVariableOsInfo, OS_INFO_VARIABLE_NTSuiteDataCenter},
        {L"NTSuiteEnterprise", InitializeVariableOsInfo, OS_INFO_VARIABLE_NTSuiteEnterprise},
        {L"NTSuitePersonal", InitializeVariableOsInfo, OS_INFO_VARIABLE_NTSuitePersonal},
        {L"NTSuiteSmallBusiness", InitializeVariableOsInfo, OS_INFO_VARIABLE_NTSuiteSmallBusiness},
        {L"NTSuiteSmallBusinessRestricted", InitializeVariableOsInfo, OS_INFO_VARIABLE_NTSuiteSmallBusinessRestricted},
        {L"NTSuiteWebServer", InitializeVariableOsInfo, OS_INFO_VARIABLE_NTSuiteWebServer},
        {L"PersonalFolder", InitializeVariableCsidlFolder, CSIDL_PERSONAL},
        {L"Privileged", InitializeVariablePrivileged, 0},
#if defined(_WIN64)
        {L"ProgramFiles64Folder", InitializeVariableCsidlFolder, CSIDL_PROGRAM_FILES},
        {L"ProgramFilesFolder", InitializeVariableCsidlFolder, CSIDL_PROGRAM_FILESX86},
#else
        {L"ProgramFiles64Folder", InitializeVariableRegistryFolder, CSIDL_PROGRAM_FILES},
        {L"ProgramFilesFolder", InitializeVariableCsidlFolder, CSIDL_PROGRAM_FILES},
#endif
        {L"ProgramMenuFolder", InitializeVariableCsidlFolder, CSIDL_PROGRAMS},
        {L"RebootPending", InitializeVariableRebootPending, 0},
        {L"SendToFolder", InitializeVariableCsidlFolder, CSIDL_SENDTO},
        {L"ServicePackLevel", InitializeVariableOsInfo, OS_INFO_VARIABLE_ServicePackLevel},
        {L"StartMenuFolder", InitializeVariableCsidlFolder, CSIDL_STARTMENU},
        {L"StartupFolder", InitializeVariableCsidlFolder, CSIDL_STARTUP},
        {L"SystemFolder", InitializeVariableSystemFolder, FALSE},
        {L"System64Folder", InitializeVariableSystemFolder, TRUE},
        {L"SystemLanguageID", InitializeSystemLanguageID, 0},
        {L"TempFolder", InitializeVariableTempFolder, 0},
        {L"TemplateFolder", InitializeVariableCsidlFolder, CSIDL_TEMPLATES},
        {L"TerminalServer", InitializeVariableOsInfo, OS_INFO_VARIABLE_TerminalServer},
        {L"UserLanguageID", InitializeUserLanguageID, 0},
        {L"VersionMsi", InitializeVariableVersionMsi, 0},
        {L"VersionNT", InitializeVariableOsInfo, OS_INFO_VARIABLE_VersionNT},
        {L"VersionNT64", InitializeVariableOsInfo, OS_INFO_VARIABLE_VersionNT64},
        {L"WindowsFolder", InitializeVariableCsidlFolder, CSIDL_WINDOWS},
        {L"WindowsVolume", InitializeVariableWindowsVolumeFolder, 0},
        {BURN_BUNDLE_ACTION, InitializeVariableNumeric, 0},
        {BURN_BUNDLE_INSTALLED, InitializeVariableNumeric, 0},
        {BURN_BUNDLE_ELEVATED, InitializeVariableNumeric, 0},
        {BURN_BUNDLE_PROVIDER_KEY, InitializeVariableString, (DWORD_PTR)L""},
        {BURN_BUNDLE_TAG, InitializeVariableString, (DWORD_PTR)L""},
    };

    for (DWORD i = 0; i < countof(vrgBuiltInVariables); ++i)
    {
        BUILT_IN_VARIABLE_DECLARATION* pBuiltInVariable = &vrgBuiltInVariables[i];

        hr = AddBuiltInVariable(pVariables, pBuiltInVariable->wzVariable, pBuiltInVariable->pfnInitialize, pBuiltInVariable->dwpInitializeData);
        ExitOnFailure1(hr, "Failed to add built-in variable: %ls.", pBuiltInVariable->wzVariable);
    }

LExit:
    return hr;
}

extern "C" HRESULT VariablesParseFromXml(
    __in BURN_VARIABLES* pVariables,
    __in IXMLDOMNode* pixnBundle
    )
{
    HRESULT hr = S_OK;
    IXMLDOMNodeList* pixnNodes = NULL;
    IXMLDOMNode* pixnNode = NULL;
    DWORD cNodes = 0;
    LPWSTR sczId = NULL;
    LPWSTR scz = NULL;
    BURN_VARIANT value = { };
    BURN_VARIANT_TYPE valueType = BURN_VARIANT_TYPE_NONE;
    BOOL fHidden = FALSE;
    BOOL fPersisted = FALSE;
    DWORD iVariable = 0;

    ::EnterCriticalSection(&pVariables->csAccess);

    // select variable nodes
    hr = XmlSelectNodes(pixnBundle, L"Variable", &pixnNodes);
    ExitOnFailure(hr, "Failed to select variable nodes.");

    // get variable node count
    hr = pixnNodes->get_length((long*)&cNodes);
    ExitOnFailure(hr, "Failed to get variable node count.");

    // parse package elements
    for (DWORD i = 0; i < cNodes; ++i)
    {
        hr = XmlNextElement(pixnNodes, &pixnNode, NULL);
        ExitOnFailure(hr, "Failed to get next node.");

        // @Id
        hr = XmlGetAttributeEx(pixnNode, L"Id", &sczId);
        ExitOnFailure(hr, "Failed to get @Id.");

        // @Hidden
        hr = XmlGetYesNoAttribute(pixnNode, L"Hidden", &fHidden);
        ExitOnFailure(hr, "Failed to get @Hidden.");

        // @Persisted
        hr = XmlGetYesNoAttribute(pixnNode, L"Persisted", &fPersisted);
        ExitOnFailure(hr, "Failed to get @Persisted.");

        // @Value
        hr = XmlGetAttributeEx(pixnNode, L"Value", &scz);
        if (E_NOTFOUND != hr)
        {
            ExitOnFailure(hr, "Failed to get @Value.");

            hr = BVariantSetString(&value, scz, 0);
            ExitOnFailure(hr, "Failed to set variant value.");

            // @Type
            hr = XmlGetAttributeEx(pixnNode, L"Type", &scz);
            ExitOnFailure(hr, "Failed to get @Type.");

            if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, 0, scz, -1, L"numeric", -1))
            {
                if (!fHidden)
                {
                    LogStringLine(REPORT_STANDARD, "Initializing numeric variable '%ls' to value '%ls'", sczId, value.sczValue);
                }
                valueType = BURN_VARIANT_TYPE_NUMERIC;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, 0, scz, -1, L"string", -1))
            {
                if (!fHidden)
                {
                    LogStringLine(REPORT_STANDARD, "Initializing string variable '%ls' to value '%ls'", sczId, value.sczValue);
                }
                valueType = BURN_VARIANT_TYPE_STRING;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, 0, scz, -1, L"version", -1))
            {
                if (!fHidden)
                {
                    LogStringLine(REPORT_STANDARD, "Initializing version variable '%ls' to value '%ls'", sczId, value.sczValue);
                }
                valueType = BURN_VARIANT_TYPE_VERSION;
            }
            else
            {
                hr = E_INVALIDARG;
                ExitOnFailure1(hr, "Invalid value for @Type: %ls", scz);
            }
        }
        else
        {
            valueType = BURN_VARIANT_TYPE_NONE;
        }

        if (fHidden)
        {
            LogStringLine(REPORT_STANDARD, "Initializing hidden variable '%ls'", sczId);
        }

        // change value variant to correct type
        hr = BVariantChangeType(&value, valueType);
        ExitOnFailure(hr, "Failed to change variant type.");

        // find existing variable
        hr = FindVariableIndexByName(pVariables, sczId, &iVariable);
        ExitOnFailure1(hr, "Failed to find variable value '%ls'.", sczId);

        // insert element if not found
        if (S_FALSE == hr)
        {
            hr = InsertVariable(pVariables, sczId, iVariable);
            ExitOnFailure1(hr, "Failed to insert variable '%ls'.", sczId);
        }
        else if (pVariables->rgVariables[iVariable].fBuiltIn)
        {
            hr = E_INVALIDARG;
            ExitOnRootFailure1(hr, "Attempt to set built-in variable value: %ls", sczId);
        }
        pVariables->rgVariables[iVariable].fHidden = fHidden;
        pVariables->rgVariables[iVariable].fPersisted = fPersisted;

        // update variable value
        hr = BVariantCopy(&value, &pVariables->rgVariables[iVariable].Value);
        ExitOnFailure1(hr, "Failed to set value of variable: %ls", sczId);

        // prepare next iteration
        ReleaseNullObject(pixnNode);
    }

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    ReleaseObject(pixnNodes);
    ReleaseObject(pixnNode);
    ReleaseStr(scz);
    ReleaseStr(sczId);
    BVariantUninitialize(&value);

    return hr;
}

extern "C" void VariablesUninitialize(
    __in BURN_VARIABLES* pVariables
    )
{
    ::DeleteCriticalSection(&pVariables->csAccess);

    if (pVariables->rgVariables)
    {
        for (DWORD i = 0; i < pVariables->cVariables; ++i)
        {
            BURN_VARIABLE* pVariable = &pVariables->rgVariables[i];
            if (pVariable)
            {
                ReleaseStr(pVariable->sczName);
                BVariantUninitialize(&pVariable->Value);
            }
        }
        MemFree(pVariables->rgVariables);
    }
}

extern "C" void VariablesDump(
    __in BURN_VARIABLES* pVariables
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczValue = NULL;

    for (DWORD i = 0; i < pVariables->cVariables; ++i)
    {
        BURN_VARIABLE* pVariable = &pVariables->rgVariables[i];
        if (pVariable && BURN_VARIANT_TYPE_NONE != pVariable->Value.Type)
        {
            hr = StrAllocFormatted(&sczValue, L"%ls = [%ls]", pVariable->sczName, pVariable->sczName);
            if (SUCCEEDED(hr))
            {
                if (pVariable->fHidden)
                {
                    hr = VariableFormatStringObfuscated(pVariables, sczValue, &sczValue, NULL);
                }
                else
                {
                    hr = VariableFormatString(pVariables, sczValue, &sczValue, NULL);
                }
            }

            if (FAILED(hr))
            {
                // already logged; best-effort to dump the rest on our way out the door
                continue;
            }

            LogId(REPORT_VERBOSE, MSG_VARIABLE_DUMP, sczValue);

            ReleaseNullStr(sczValue);
        }
    }

    ReleaseStr(sczValue);
}

extern "C" HRESULT VariableGetNumeric(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out LONGLONG* pllValue
    )
{
    HRESULT hr = S_OK;
    BURN_VARIABLE* pVariable = NULL;

    ::EnterCriticalSection(&pVariables->csAccess);

    hr = GetVariable(pVariables, wzVariable, &pVariable);
    if (SUCCEEDED(hr) && BURN_VARIANT_TYPE_NONE == pVariable->Value.Type)
    {
        ExitFunction1(hr = E_NOTFOUND);
    }
    else if (E_NOTFOUND == hr)
    {
        ExitFunction();
    }
    ExitOnFailure1(hr, "Failed to get value of variable: %ls", wzVariable);

    hr = BVariantGetNumeric(&pVariable->Value, pllValue);
    ExitOnFailure1(hr, "Failed to get value as numeric for variable: %ls", wzVariable);

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    return hr;
}

extern "C" HRESULT VariableGetString(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out_z LPWSTR* psczValue
    )
{
    HRESULT hr = S_OK;
    BURN_VARIABLE* pVariable = NULL;

    ::EnterCriticalSection(&pVariables->csAccess);

    hr = GetVariable(pVariables, wzVariable, &pVariable);
    if (SUCCEEDED(hr) && BURN_VARIANT_TYPE_NONE == pVariable->Value.Type)
    {
        ExitFunction1(hr = E_NOTFOUND);
    }
    else if (E_NOTFOUND == hr)
    {
        ExitFunction();
    }
    ExitOnFailure1(hr, "Failed to get value of variable: %ls", wzVariable);

    hr = BVariantGetString(&pVariable->Value, psczValue);
    ExitOnFailure1(hr, "Failed to get value as string for variable: %ls", wzVariable);

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    return hr;
}

extern "C" HRESULT VariableGetVersion(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in DWORD64* pqwValue
    )
{
    HRESULT hr = S_OK;
    BURN_VARIABLE* pVariable = NULL;

    ::EnterCriticalSection(&pVariables->csAccess);

    hr = GetVariable(pVariables, wzVariable, &pVariable);
    if (SUCCEEDED(hr) && BURN_VARIANT_TYPE_NONE == pVariable->Value.Type)
    {
        ExitFunction1(hr = E_NOTFOUND);
    }
    else if (E_NOTFOUND == hr)
    {
        ExitFunction();
    }
    ExitOnFailure1(hr, "Failed to get value of variable: %ls", wzVariable);

    hr = BVariantGetVersion(&pVariable->Value, pqwValue);
    ExitOnFailure1(hr, "Failed to get value as version for variable: %ls", wzVariable);

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    return hr;
}

extern "C" HRESULT VariableGetVariant(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    BURN_VARIABLE* pVariable = NULL;

    ::EnterCriticalSection(&pVariables->csAccess);

    hr = GetVariable(pVariables, wzVariable, &pVariable);
    if (E_NOTFOUND == hr)
    {
        ExitFunction();
    }
    ExitOnFailure1(hr, "Failed to get value of variable: %ls", wzVariable);

    hr = BVariantCopy(&pVariable->Value, pValue);
    ExitOnFailure1(hr, "Failed to copy value of variable: %ls", wzVariable);

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    return hr;
}

extern "C" HRESULT VariableGetFormatted(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out_z LPWSTR* psczValue
    )
{
    HRESULT hr = S_OK;
    BURN_VARIABLE* pVariable = NULL;

    ::EnterCriticalSection(&pVariables->csAccess);

    hr = GetVariable(pVariables, wzVariable, &pVariable);
    if (SUCCEEDED(hr) && BURN_VARIANT_TYPE_NONE == pVariable->Value.Type)
    {
        ExitFunction1(hr = E_NOTFOUND);
    }
    else if (E_NOTFOUND == hr)
    {
        ExitFunction();
    }
    ExitOnFailure1(hr, "Failed to get variable: %ls", wzVariable);

    // Non-builtin strings may need to get expanded... non-strings and builtin
    // variables are never expanded.
    if (BURN_VARIANT_TYPE_STRING == pVariable->Value.Type && !pVariable->fBuiltIn)
    {
        hr = VariableFormatString(pVariables, pVariable->Value.sczValue, psczValue, NULL);
        ExitOnFailure2(hr, "Failed to format value '%ls' of variable: %ls", pVariable->Value.sczValue, wzVariable);
    }
    else
    {
        hr = BVariantGetString(&pVariable->Value, psczValue);
        ExitOnFailure1(hr, "Failed to get value as string for variable: %ls", wzVariable);
    }

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    return hr;
}

extern "C" HRESULT VariableSetNumeric(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in LONGLONG llValue,
    __in BOOL fOverwriteBuiltIn
    )
{
    BURN_VARIANT variant = { };

    variant.llValue = llValue;
    variant.Type = BURN_VARIANT_TYPE_NUMERIC;

    return SetVariableValue(pVariables, wzVariable, &variant, fOverwriteBuiltIn, TRUE);
}

extern "C" HRESULT VariableSetString(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in_z_opt LPCWSTR wzValue,
    __in BOOL fOverwriteBuiltIn
    )
{
    BURN_VARIANT variant = { };

    variant.sczValue = (LPWSTR)wzValue;
    variant.Type = BURN_VARIANT_TYPE_STRING;

    return SetVariableValue(pVariables, wzVariable, &variant, fOverwriteBuiltIn, TRUE);
}

extern "C" HRESULT VariableSetVersion(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in DWORD64 qwValue,
    __in BOOL fOverwriteBuiltIn
    )
{
    BURN_VARIANT variant = { };

    variant.qwValue = qwValue;
    variant.Type = BURN_VARIANT_TYPE_VERSION;

    return SetVariableValue(pVariables, wzVariable, &variant, fOverwriteBuiltIn, TRUE);
}

extern "C" HRESULT VariableSetVariant(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in BURN_VARIANT* pVariant,
    __in BOOL fOverwriteBuiltIn
    )
{
    return SetVariableValue(pVariables, wzVariable, pVariant, fOverwriteBuiltIn, TRUE);
}

extern "C" HRESULT VariableFormatString(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzIn,
    __out_z_opt LPWSTR* psczOut,
    __out_opt DWORD* pcchOut
    )
{
    return FormatString(pVariables, wzIn, psczOut, pcchOut, FALSE);
}

extern "C" HRESULT VariableFormatStringObfuscated(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzIn,
    __out_z_opt LPWSTR* psczOut,
    __out_opt DWORD* pcchOut
    )
{
    return FormatString(pVariables, wzIn, psczOut, pcchOut, TRUE);
}

extern "C" HRESULT VariableEscapeString(
    __in_z LPCWSTR wzIn,
    __out_z LPWSTR* psczOut
    )
{
    HRESULT hr = S_OK;
    LPCWSTR wzRead = NULL;
    LPWSTR pwzEscaped = NULL;
    LPWSTR pwz = NULL;
    SIZE_T i = 0;

    // allocate buffer for escaped string
    hr = StrAlloc(&pwzEscaped, lstrlenW(wzIn) + 1);
    ExitOnFailure(hr, "Failed to allocate buffer for escaped string.");

    // read trough string and move characters, inserting escapes as needed
    wzRead = wzIn;
    for (;;)
    {
        // find next character needing escaping
        i = wcscspn(wzRead, L"[]{}");

        // copy skipped characters
        if (0 < i)
        {
            hr = StrAllocConcat(&pwzEscaped, wzRead, i);
            ExitOnFailure(hr, "Failed to append characters.");
        }

        if (L'\0' == wzRead[i])
        {
            break; // end reached
        }

        // escape character
        hr = StrAllocFormatted(&pwz, L"[\\%c]", wzRead[i]);
        ExitOnFailure(hr, "Failed to format escape sequence.");

        hr = StrAllocConcat(&pwzEscaped, pwz, 0);
        ExitOnFailure(hr, "Failed to append escape sequence.");

        // update read pointer
        wzRead += i + 1;
    }

    // return value
    hr = StrAllocString(psczOut, pwzEscaped, 0);
    ExitOnFailure(hr, "Failed to copy string.");

LExit:
    ReleaseStr(pwzEscaped);
    ReleaseStr(pwz);
    return hr;
}

extern "C" HRESULT VariableSerialize(
    __in BURN_VARIABLES* pVariables,
    __in BOOL fPersisting,
    __inout BYTE** ppbBuffer,
    __inout SIZE_T* piBuffer
    )
{
    HRESULT hr = S_OK;
    BOOL fIncluded = FALSE;

    ::EnterCriticalSection(&pVariables->csAccess);

    // write variable count
    hr = BuffWriteNumber(ppbBuffer, piBuffer, pVariables->cVariables);
    ExitOnFailure(hr, "Failed to write variable count.");

    // write variables
    for (DWORD i = 0; i < pVariables->cVariables; ++i)
    {
        BURN_VARIABLE* pVariable = &pVariables->rgVariables[i];
        fIncluded = TRUE;

        // if variable is built-in, don't serialized
        if (pVariable->fBuiltIn)
        {
            fIncluded = FALSE;
        }

        // if we are persisting, exclude variables that should not be persisted
        if (fPersisting && !pVariable->fPersisted)
        {
            fIncluded = FALSE;
        }

        // write included flag
        hr = BuffWriteNumber(ppbBuffer, piBuffer, (DWORD)fIncluded);
        ExitOnFailure(hr, "Failed to write included flag.");

        if (!fIncluded)
        {
            continue;
        }

        // write variable name
        hr = BuffWriteString(ppbBuffer, piBuffer, pVariable->sczName);
        ExitOnFailure(hr, "Failed to write variable name.");

        // write variable value
        hr = BuffWriteNumber(ppbBuffer, piBuffer, (DWORD)pVariable->Value.Type);
        ExitOnFailure(hr, "Failed to write variable value type.");

        switch (pVariable->Value.Type)
        {
        case BURN_VARIANT_TYPE_NONE:
            break;
        case BURN_VARIANT_TYPE_NUMERIC: __fallthrough;
        case BURN_VARIANT_TYPE_VERSION:
            hr = BuffWriteNumber64(ppbBuffer, piBuffer, pVariable->Value.qwValue);
            ExitOnFailure(hr, "Failed to write variable value as number.");
            break;
        case BURN_VARIANT_TYPE_STRING:
            hr = BuffWriteString(ppbBuffer, piBuffer, pVariable->Value.sczValue);
            ExitOnFailure(hr, "Failed to write variable value as string.");
            break;
        default:
            hr = E_INVALIDARG;
            ExitOnFailure(hr, "Unsupported variable type.");
        }
    }

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    return hr;
}

extern "C" HRESULT VariableDeserialize(
    __in BURN_VARIABLES* pVariables,
    __in_bcount(cbBuffer) BYTE* pbBuffer,
    __in SIZE_T cbBuffer,
    __inout SIZE_T* piBuffer
    )
{
    HRESULT hr = S_OK;
    DWORD cVariables = 0;
    LPWSTR sczName = NULL;
    BOOL fIncluded = FALSE;
    BURN_VARIANT value = { };

    ::EnterCriticalSection(&pVariables->csAccess);

    // read variable count
    hr = BuffReadNumber(pbBuffer, cbBuffer, piBuffer, &cVariables);
    ExitOnFailure(hr, "Failed to read variable count.");

    // read variables
    for (DWORD i = 0; i < cVariables; ++i)
    {
        // read variable included flag
        hr = BuffReadNumber(pbBuffer, cbBuffer, piBuffer, (DWORD*)&fIncluded);
        ExitOnFailure(hr, "Failed to read variable included flag.");

        if (!fIncluded)
        {
            continue; // if variable is not included, skip
        }

        // read variable name
        hr = BuffReadString(pbBuffer, cbBuffer, piBuffer, &sczName);
        ExitOnFailure(hr, "Failed to read variable name.");

        // read variable value type
        hr = BuffReadNumber(pbBuffer, cbBuffer, piBuffer, (DWORD*)&value.Type);
        ExitOnFailure(hr, "Failed to read variable value type.");

        // read variable value
        switch (value.Type)
        {
        case BURN_VARIANT_TYPE_NONE:
            break;
        case BURN_VARIANT_TYPE_NUMERIC: __fallthrough;
        case BURN_VARIANT_TYPE_VERSION:
            hr = BuffReadNumber64(pbBuffer, cbBuffer, piBuffer, &value.qwValue);
            ExitOnFailure(hr, "Failed to read variable value as number.");
            break;
        case BURN_VARIANT_TYPE_STRING:
            hr = BuffReadString(pbBuffer, cbBuffer, piBuffer, &value.sczValue);
            ExitOnFailure(hr, "Failed to read variable value as string.");
            break;
        default:
            hr = E_INVALIDARG;
            ExitOnFailure(hr, "Unsupported variable type.");
        }

        // set variable
        hr = SetVariableValue(pVariables, sczName, &value, FALSE, FALSE);
        ExitOnFailure(hr, "Failed to set variable.");

        // clean up
        BVariantUninitialize(&value);
    }

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    ReleaseStr(sczName);
    BVariantUninitialize(&value);

    return hr;
}


// internal function definitions

static HRESULT FormatString(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzIn,
    __out_z_opt LPWSTR* psczOut,
    __out_opt DWORD* pcchOut,
    __in BOOL fObfuscateHiddenVariables
    )
{
    HRESULT hr = S_OK;
    DWORD er = ERROR_SUCCESS;
    LPWSTR sczUnformatted = NULL;
    LPWSTR sczFormat = NULL;
    LPCWSTR wzRead = NULL;
    LPCWSTR wzOpen = NULL;
    LPCWSTR wzClose = NULL;
    LPWSTR scz = NULL;
    LPWSTR* rgVariables = NULL;
    DWORD cVariables = 0;
    DWORD cch = 0;
    BOOL fHidden = FALSE;
    MSIHANDLE hRecord = NULL;

    ::EnterCriticalSection(&pVariables->csAccess);

    // allocate buffer for format string
    hr = StrAlloc(&sczFormat, lstrlenW(wzIn) + 1);
    ExitOnFailure(hr, "Failed to allocate buffer for format string.");

    // read out variables from the unformatted string and build a format string
    wzRead = wzIn;
    for (;;)
    {
        // scan for opening '['
        wzOpen = wcschr(wzRead, L'[');
        if (!wzOpen)
        {
            // end reached, append the remainder of the string and end loop
            hr = StrAllocConcat(&sczFormat, wzRead, 0);
            ExitOnFailure(hr, "Failed to append string.");
            break;
        }

        // scan for closing ']'
        wzClose = wcschr(wzOpen + 1, L']');
        if (!wzClose)
        {
            // end reached, treat unterminated expander as literal
            hr = StrAllocConcat(&sczFormat, wzRead, 0);
            ExitOnFailure(hr, "Failed to append string.");
            break;
        }
        cch = wzClose - wzOpen - 1;

        if (0 == cch)
        {
            // blank, copy all text including the terminator
            hr = StrAllocConcat(&sczFormat, wzRead, (DWORD_PTR)(wzClose - wzRead) + 1);
            ExitOnFailure(hr, "Failed to append string.");
        }
        else
        {
            // append text preceding expander
            if (wzOpen > wzRead)
            {
                hr = StrAllocConcat(&sczFormat, wzRead, (DWORD_PTR)(wzOpen - wzRead));
                ExitOnFailure(hr, "Failed to append string.");
            }

            // get variable name
            hr = StrAllocString(&scz, wzOpen + 1, cch);
            ExitOnFailure(hr, "Failed to get variable name.");

            // allocate space in variable array
            if (rgVariables)
            {
                LPVOID pv = MemReAlloc(rgVariables, sizeof(LPWSTR) * (cVariables + 1), TRUE);
                ExitOnNull(pv, hr, E_OUTOFMEMORY, "Failed to reallocate variable array.");
                rgVariables = (LPWSTR*)pv;
            }
            else
            {
                rgVariables = (LPWSTR*)MemAlloc(sizeof(LPWSTR) * (cVariables + 1), TRUE);
                ExitOnNull(rgVariables, hr, E_OUTOFMEMORY, "Failed to allocate variable array.");
            }

            // set variable value
            if (2 <= cch && L'\\' == wzOpen[1])
            {
                // escape sequence, copy character
                hr = StrAllocString(&rgVariables[cVariables], &wzOpen[2], 1);
            }
            else
            {
                if (fObfuscateHiddenVariables)
                {
                    hr = IsVariableHidden(pVariables, scz, &fHidden);
                    ExitOnFailure1(hr, "Failed to determine variable visibility: '%ls'.", scz);
                }

                if (fHidden)
                {
                    hr = StrAllocString(&rgVariables[cVariables], L"*****", 0);
                }
                else
                {
                    // get formatted variable value
                    hr = VariableGetFormatted(pVariables, scz, &rgVariables[cVariables]);
                    if (E_NOTFOUND == hr) // variable not found
                    {
                        hr = StrAllocString(&rgVariables[cVariables], L"", 0);
                    }
                }
            }
            ExitOnFailure(hr, "Failed to set variable value.");
            ++cVariables;

            // append placeholder to format string
            hr = StrAllocFormatted(&scz, L"[%d]", cVariables);
            ExitOnFailure(hr, "Failed to format placeholder string.");

            hr = StrAllocConcat(&sczFormat, scz, 0);
            ExitOnFailure(hr, "Failed to append placeholder.");
        }

        // update read pointer
        wzRead = wzClose + 1;
    }

    // create record
    hRecord = ::MsiCreateRecord(cVariables);
    ExitOnNull(hRecord, hr, E_OUTOFMEMORY, "Failed to allocate record.");

    // set format string
    er = ::MsiRecordSetStringW(hRecord, 0, sczFormat);
    ExitOnWin32Error(er, hr, "Failed to set record format string.");

    // copy record fields
    for (DWORD i = 0; i < cVariables; ++i)
    {
        if (*rgVariables[i]) // not setting if blank
        {
            er = ::MsiRecordSetStringW(hRecord, i + 1, rgVariables[i]);
            ExitOnWin32Error(er, hr, "Failed to set record string.");
        }
    }

    // get formatted character count
    cch = 0;
#pragma prefast(push)
#pragma prefast(disable:6298)
    er = ::MsiFormatRecordW(NULL, hRecord, L"", &cch);
#pragma prefast(pop)
    if (ERROR_MORE_DATA != er)
    {
        ExitOnWin32Error(er, hr, "Failed to get formatted length.");
    }

    // return formatted string
    if (psczOut)
    {
        hr = StrAlloc(&scz, ++cch);
        ExitOnFailure(hr, "Failed to allocate string.");

        er = ::MsiFormatRecordW(NULL, hRecord, scz, &cch);
        ExitOnWin32Error(er, hr, "Failed to format record.");

        hr = StrAllocString(psczOut, scz, 0);
        ExitOnFailure(hr, "Failed to copy string.");
    }

    // return character count
    if (pcchOut)
    {
        *pcchOut = cch;
    }

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    if (rgVariables)
    {
        for (DWORD i = 0; i < cVariables; ++i)
        {
            ReleaseStr(rgVariables[i]);
        }
        MemFree(rgVariables);
    }

    if (hRecord)
    {
        ::MsiCloseHandle(hRecord);
    }

    ReleaseStr(sczUnformatted);
    ReleaseStr(sczFormat);
    ReleaseStr(scz);

    return hr;
}

static HRESULT AddBuiltInVariable(
    __in BURN_VARIABLES* pVariables,
    __in LPCWSTR wzVariable,
    __in PFN_INITIALIZEVARIABLE pfnInitialize,
    __in DWORD_PTR dwpInitializeData
    )
{
    HRESULT hr = S_OK;
    DWORD iVariable = 0;
    BURN_VARIABLE* pVariable = NULL;

    hr = FindVariableIndexByName(pVariables, wzVariable, &iVariable);
    ExitOnFailure(hr, "Failed to find variable value.");

    // insert element if not found
    if (S_FALSE == hr)
    {
        hr = InsertVariable(pVariables, wzVariable, iVariable);
        ExitOnFailure(hr, "Failed to insert variable.");
    }

    // set variable values
    pVariable = &pVariables->rgVariables[iVariable];
    pVariable->fBuiltIn = TRUE;
    pVariable->pfnInitialize = pfnInitialize;
    pVariable->dwpInitializeData = dwpInitializeData;

LExit:
    return hr;
}

static HRESULT GetVariable(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out BURN_VARIABLE** ppVariable
    )
{
    HRESULT hr = S_OK;
    DWORD iVariable = 0;
    BURN_VARIABLE* pVariable = NULL;

    hr = FindVariableIndexByName(pVariables, wzVariable, &iVariable);
    ExitOnFailure1(hr, "Failed to find variable value '%ls'.", wzVariable);

    if (S_FALSE == hr)
    {
        ExitFunction1(hr = E_NOTFOUND);
    }

    pVariable = &pVariables->rgVariables[iVariable];

    // initialize built-in variable
    if (BURN_VARIANT_TYPE_NONE == pVariable->Value.Type && pVariable->fBuiltIn)
    {
        hr = pVariable->pfnInitialize(pVariable->dwpInitializeData, &pVariable->Value);
        ExitOnFailure1(hr, "Failed to initialize built-in variable value '%ls'.", wzVariable);
    }

    *ppVariable = pVariable;

LExit:
    return hr;
}

static HRESULT FindVariableIndexByName(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out DWORD* piVariable
    )
{
    HRESULT hr = S_OK;
    DWORD iRangeFirst = 0;
    DWORD cRangeLength = pVariables->cVariables;

    while (cRangeLength)
    {
        // get variable in middle of range
        DWORD iPosition = cRangeLength / 2;
        BURN_VARIABLE* pVariable = &pVariables->rgVariables[iRangeFirst + iPosition];

        switch (::CompareStringW(LOCALE_INVARIANT, SORT_STRINGSORT, wzVariable, -1, pVariable->sczName, -1))
        {
        case CSTR_LESS_THAN:
            // restrict range to elements before the current
            cRangeLength = iPosition;
            break;
        case CSTR_EQUAL:
            // variable found
            *piVariable = iRangeFirst + iPosition;
            ExitFunction1(hr = S_OK);
        case CSTR_GREATER_THAN:
            // restrict range to elements after the current
            iRangeFirst += iPosition + 1;
            cRangeLength -= iPosition + 1;
            break;
        default:
            ExitWithLastError(hr, "Failed to compare strings.");
        }
    }

    *piVariable = iRangeFirst;
    hr = S_FALSE; // variable not found

LExit:
    return hr;
}

static HRESULT InsertVariable(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in DWORD iPosition
    )
{
    HRESULT hr = S_OK;
    size_t cbAllocSize = 0;

    // ensure there is room in the variable array
    if (pVariables->cVariables == pVariables->dwMaxVariables)
    {
        hr = ::DWordAdd(pVariables->dwMaxVariables, GROW_VARIABLE_ARRAY, &(pVariables->dwMaxVariables));
        ExitOnRootFailure(hr, "Overflow while growing variable array size");

        if (pVariables->rgVariables)
        {
            hr = ::SizeTMult(sizeof(BURN_VARIABLE), pVariables->dwMaxVariables, &cbAllocSize);
            ExitOnRootFailure(hr, "Overflow while calculating size of variable array buffer");

            LPVOID pv = MemReAlloc(pVariables->rgVariables, cbAllocSize, FALSE);
            ExitOnNull(pv, hr, E_OUTOFMEMORY, "Failed to allocate room for more variables.");

            // Prefast claims it's possible to hit this. Putting the check in just in case.
            if (pVariables->dwMaxVariables < pVariables->cVariables)
            {
                hr = INTSAFE_E_ARITHMETIC_OVERFLOW;
                ExitOnRootFailure(hr, "Overflow while dealing with variable array buffer allocation");
            }

            pVariables->rgVariables = (BURN_VARIABLE*)pv;
            memset(&pVariables->rgVariables[pVariables->cVariables], 0, sizeof(BURN_VARIABLE) * (pVariables->dwMaxVariables - pVariables->cVariables));
        }
        else
        {
            pVariables->rgVariables = (BURN_VARIABLE*)MemAlloc(sizeof(BURN_VARIABLE) * pVariables->dwMaxVariables, TRUE);
            ExitOnNull(pVariables->rgVariables, hr, E_OUTOFMEMORY, "Failed to allocate room for variables.");
        }
    }

    // move variables
    if (0 < pVariables->cVariables - iPosition)
    {
        memmove(&pVariables->rgVariables[iPosition + 1], &pVariables->rgVariables[iPosition], sizeof(BURN_VARIABLE) * (pVariables->cVariables - iPosition));
        memset(&pVariables->rgVariables[iPosition], 0, sizeof(BURN_VARIABLE));
    }

    ++pVariables->cVariables;

    // allocate name
    hr = StrAllocString(&pVariables->rgVariables[iPosition].sczName, wzVariable, 0);
    ExitOnFailure(hr, "Failed to copy variable name.");

LExit:
    return hr;
}

static HRESULT SetVariableValue(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __in BURN_VARIANT* pVariant,
    __in BOOL fOverwriteBuiltIn,
    __in BOOL fLog
    )
{
    HRESULT hr = S_OK;
    DWORD iVariable = 0;

    ::EnterCriticalSection(&pVariables->csAccess);

    hr = FindVariableIndexByName(pVariables, wzVariable, &iVariable);
    ExitOnFailure1(hr, "Failed to find variable value '%ls'.", wzVariable);

    // insert element if not found
    if (S_FALSE == hr)
    {
        hr = InsertVariable(pVariables, wzVariable, iVariable);
        ExitOnFailure1(hr, "Failed to insert variable '%ls'.", wzVariable);
    }
    else if (!fOverwriteBuiltIn && pVariables->rgVariables[iVariable].fBuiltIn)
    {
        hr = E_INVALIDARG;
        ExitOnRootFailure1(hr, "Attempt to set built-in variable value: %ls", wzVariable);
    }
    else if (fOverwriteBuiltIn && !pVariables->rgVariables[iVariable].fBuiltIn)
    {
        // invalid intent for the opposite condition; not possible from external callers so just assert
        AssertSz(FALSE, "Intent to overwrite non-built-in variable.");
    }

    // log value when not overwriting a built-in variable
    if (fLog && !fOverwriteBuiltIn)
    {
        if (pVariables->rgVariables[iVariable].fHidden)
        {
            LogStringLine(REPORT_STANDARD, "Setting hidden variable '%ls'", wzVariable);
        }
        else
        {
            switch (pVariant->Type)
            {
            case BURN_VARIANT_TYPE_NONE:
                break;

            case BURN_VARIANT_TYPE_NUMERIC:
                LogStringLine(REPORT_STANDARD, "Setting numeric variable '%ls' to value %lld", wzVariable, pVariant->llValue);
                break;

            case BURN_VARIANT_TYPE_STRING:
                LogStringLine(REPORT_STANDARD, "Setting string variable '%ls' to value '%ls'", wzVariable, pVariant->sczValue);
                break;

            case BURN_VARIANT_TYPE_VERSION:
                LogStringLine(REPORT_STANDARD, "Setting version variable '%ls' to value '%hu.%hu.%hu.%hu'", wzVariable, (WORD)(pVariant->qwValue >> 48), (WORD)(pVariant->qwValue >> 32), (WORD)(pVariant->qwValue >> 16), (WORD)(pVariant->qwValue));
                break;

            default:
                AssertSz(FALSE, "Unknown variant type.");
                break;
            }
        }
    }

    // update variable value
    hr = BVariantCopy(pVariant, &pVariables->rgVariables[iVariable].Value);
    ExitOnFailure1(hr, "Failed to set value of variable: %ls", wzVariable);

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    if (FAILED(hr) && fLog)
    {
        LogStringLine(REPORT_STANDARD, "Setting variable failed: ID '%ls', HRESULT 0x%x", wzVariable, hr);
    }

    return hr;
}

static HRESULT InitializeVariableOsInfo(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    OSVERSIONINFOEXW ovix = { };
    BURN_VARIANT value = { };

    ovix.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
    if (!::GetVersionExW((LPOSVERSIONINFOW)&ovix))
    {
        ExitWithLastError(hr, "Failed to get OS info.");
    }

    switch ((OS_INFO_VARIABLE)dwpData)
    {
    case OS_INFO_VARIABLE_ServicePackLevel:
        if (0 != ovix.wServicePackMajor)
        {
            value.qwValue = static_cast<DWORD64>(ovix.wServicePackMajor);
            value.Type = BURN_VARIANT_TYPE_NUMERIC;
        }
        break;
    case OS_INFO_VARIABLE_VersionNT:
        value.qwValue = MAKEQWORDVERSION(ovix.dwMajorVersion, ovix.dwMinorVersion, 0, 0);
        value.Type = BURN_VARIANT_TYPE_VERSION;
        break;
    case OS_INFO_VARIABLE_VersionNT64:
        {
#if !defined(_WIN64)
            BOOL fIsWow64 = FALSE;

            ProcWow64(::GetCurrentProcess(), &fIsWow64);
            if (fIsWow64)
#endif
            {
                value.qwValue = MAKEQWORDVERSION(ovix.dwMajorVersion, ovix.dwMinorVersion, 0, 0);
                value.Type = BURN_VARIANT_TYPE_VERSION;
            }
        }
        break;
    case OS_INFO_VARIABLE_NTProductType:
        value.llValue = ovix.wProductType;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    case OS_INFO_VARIABLE_NTSuiteBackOffice:
        value.llValue = VER_SUITE_BACKOFFICE == ovix.wSuiteMask ? 1 : 0;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    case OS_INFO_VARIABLE_NTSuiteDataCenter:
        value.llValue = VER_SUITE_DATACENTER == ovix.wSuiteMask ? 1 : 0;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    case OS_INFO_VARIABLE_NTSuiteEnterprise:
        value.llValue = VER_SUITE_ENTERPRISE == ovix.wSuiteMask ? 1 : 0;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    case OS_INFO_VARIABLE_NTSuitePersonal:
        value.llValue = VER_SUITE_PERSONAL == ovix.wSuiteMask ? 1 : 0;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    case OS_INFO_VARIABLE_NTSuiteSmallBusiness:
        value.llValue = VER_SUITE_SMALLBUSINESS == ovix.wSuiteMask ? 1 : 0;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    case OS_INFO_VARIABLE_NTSuiteSmallBusinessRestricted:
        value.llValue = VER_SUITE_SMALLBUSINESS_RESTRICTED == ovix.wSuiteMask ? 1 : 0;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    case OS_INFO_VARIABLE_NTSuiteWebServer:
        value.llValue = VER_SUITE_BLADE == ovix.wSuiteMask ? 1 : 0;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    case OS_INFO_VARIABLE_CompatibilityMode:
        {
            DWORDLONG dwlConditionMask = 0;
            VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_EQUAL);
            VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_EQUAL);
            VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMAJOR, VER_EQUAL);
            VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMINOR, VER_EQUAL);

            value.llValue = ::VerifyVersionInfoW(&ovix, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR, dwlConditionMask);
            value.Type = BURN_VARIANT_TYPE_NUMERIC;
        }
        break;
    case OS_INFO_VARIABLE_TerminalServer:
        value.llValue = (VER_SUITE_TERMINAL == (ovix.wSuiteMask & VER_SUITE_TERMINAL)) && (VER_SUITE_SINGLEUSERTS != (ovix.wSuiteMask & VER_SUITE_SINGLEUSERTS)) ? 1 : 0;
        value.Type = BURN_VARIANT_TYPE_NUMERIC;
        break;
    default:
        AssertSz(FALSE, "Unknown OS info type.");
        break;
    }

    hr = BVariantCopy(&value, pValue);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableVersionMsi(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    UNREFERENCED_PARAMETER(dwpData);

    HRESULT hr = S_OK;
    DLLGETVERSIONPROC pfnMsiDllGetVersion = NULL;
    DLLVERSIONINFO msiVersionInfo = { };

    // get DllGetVersion proc address
    pfnMsiDllGetVersion = (DLLGETVERSIONPROC)::GetProcAddress(::GetModuleHandleW(L"msi"), "DllGetVersion");
    ExitOnNullWithLastError(pfnMsiDllGetVersion, hr, "Failed to find DllGetVersion entry point in msi.dll.");

    // get msi.dll version info
    msiVersionInfo.cbSize = sizeof(DLLVERSIONINFO);
    hr = pfnMsiDllGetVersion(&msiVersionInfo);
    ExitOnFailure(hr, "Failed to get msi.dll version info.");

    hr = BVariantSetVersion(pValue, MAKEQWORDVERSION(msiVersionInfo.dwMajorVersion, msiVersionInfo.dwMinorVersion, 0, 0));
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableCsidlFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczPath = NULL;
    int nFolder = (int)dwpData;

    // get folder path
    hr = ShelGetFolder(&sczPath, nFolder);
    ExitOnRootFailure(hr, "Failed to get shell folder.");

    // set value
    hr = BVariantSetString(pValue, sczPath, 0);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    ReleaseStr(sczPath);

    return hr;
}

static HRESULT InitializeVariableTempFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    UNREFERENCED_PARAMETER(dwpData);

    HRESULT hr = S_OK;
    WCHAR wzPath[MAX_PATH] = { };

    // get volume path name
    if (!::GetTempPathW(MAX_PATH, wzPath))
    {
        ExitWithLastError(hr, "Failed to get temp path.");
    }

    // set value
    hr = BVariantSetString(pValue, wzPath, 0);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableSystemFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    BOOL f64 = (BOOL)dwpData;
    WCHAR wzSystemFolder[MAX_PATH] = { };

#ifndef _WIN64
    if (f64)
    {
        // Try to get the WOW system folder. If this function is not implemented (aka: 32-bit Windows)
        // then we'll leave the folder blank.
        if (!::GetSystemWow64DirectoryW(wzSystemFolder, countof(wzSystemFolder)))
        {
            DWORD er = ::GetLastError();
            if (ERROR_CALL_NOT_IMPLEMENTED != er)
            {
                er = ERROR_SUCCESS;
            }

            hr = HRESULT_FROM_WIN32(er);
            ExitOnRootFailure(hr, "Failed to get 32-bit system folder.");
        }
    }
    else
    {
        if (!::GetSystemDirectoryW(wzSystemFolder, countof(wzSystemFolder)))
        {
            ExitWithLastError(hr, "Failed to get 64-bit system folder.");
        }
    }
#else
    if (f64)
    {
        if (!::GetSystemDirectoryW(wzSystemFolder, countof(wzSystemFolder)))
        {
            ExitWithLastError(hr, "Failed to get 64-bit system folder.");
        }
    }
    else
    {
        if (!::GetSystemWow64DirectoryW(wzSystemFolder, countof(wzSystemFolder)))
        {
            ExitWithLastError(hr, "Failed to get 32-bit system folder.");
        }
    }
#endif

    if (*wzSystemFolder)
    {
        hr = PathFixedBackslashTerminate(wzSystemFolder, countof(wzSystemFolder));
        ExitOnFailure(hr, "Failed to backslash terminate system folder.");
    }

    // set value
    hr = BVariantSetString(pValue, wzSystemFolder, 0);
    ExitOnFailure(hr, "Failed to set system folder variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableWindowsVolumeFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    UNREFERENCED_PARAMETER(dwpData);

    HRESULT hr = S_OK;
    WCHAR wzWindowsPath[MAX_PATH] = { };
    WCHAR wzVolumePath[MAX_PATH] = { };

    // get windows directory
    if (!::GetWindowsDirectoryW(wzWindowsPath, countof(wzWindowsPath)))
    {
        ExitWithLastError(hr, "Failed to get windows directory.");
    }

    // get volume path name
    if (!::GetVolumePathNameW(wzWindowsPath, wzVolumePath, MAX_PATH))
    {
        ExitWithLastError(hr, "Failed to get volume path name.");
    }

    // set value
    hr = BVariantSetString(pValue, wzVolumePath, 0);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariablePrivileged(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    UNREFERENCED_PARAMETER(dwpData);

    HRESULT hr = S_OK;
    BOOL fPrivileged = FALSE;

    // check if process is running privileged
    hr = OsIsRunningPrivileged(&fPrivileged);
    ExitOnFailure(hr, "Failed to check if process is running privileged.");

    // set value
    hr = BVariantSetNumeric(pValue, fPrivileged);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableRebootPending(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    UNREFERENCED_PARAMETER(dwpData);

    HRESULT hr = S_OK;
    BOOL fRebootPending = FALSE;
    BOOL fComInitialized = FALSE;

    // Do a best effort to ask WU if a reboot is required. If anything goes
    // wrong then let's pretend a reboot is not required.
    hr = ::CoInitialize(NULL);
    if (SUCCEEDED(hr) || RPC_E_CHANGED_MODE == hr)
    {
        fComInitialized = TRUE;

        hr = WuaRestartRequired(&fRebootPending);
        if (FAILED(hr))
        {
            fRebootPending = FALSE;
            hr = S_OK;
        }
    }

    hr = BVariantSetNumeric(pValue, fRebootPending);
    ExitOnFailure(hr, "Failed to set reboot pending variant value.");

LExit:
    if (fComInitialized)
    {
        ::CoUninitialize();
    }

    return hr;
}

static HRESULT InitializeSystemLanguageID(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    UNREFERENCED_PARAMETER(dwpData);

    HRESULT hr = S_OK;
    LANGID langid = ::GetSystemDefaultLangID();

    hr = BVariantSetNumeric(pValue, langid);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeUserLanguageID(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    UNREFERENCED_PARAMETER(dwpData);

    HRESULT hr = S_OK;
    LANGID langid = ::GetUserDefaultLangID();

    hr = BVariantSetNumeric(pValue, langid);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableString(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    LPCWSTR wzValue = (LPCWSTR)dwpData;

    // set value
    hr = BVariantSetString(pValue, wzValue, 0);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableNumeric(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    LONGLONG llValue = (LONGLONG)dwpData;

    // set value
    hr = BVariantSetNumeric(pValue, llValue);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableRegistryFolder(
    __in DWORD_PTR dwpData,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    HKEY hkFolders = NULL;
    LPWSTR sczPath = NULL;

    int nFolder = (int)dwpData;
    AssertSz(CSIDL_PROGRAM_FILES == nFolder || CSIDL_PROGRAM_FILES_COMMON == nFolder, "Unknown folder CSIDL.");
    LPCWSTR wzFolderValue = CSIDL_PROGRAM_FILES_COMMON == nFolder ? L"CommonW6432Dir" : L"ProgramW6432Dir";

    hr = RegOpen(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion", KEY_READ, &hkFolders);
    if (E_FILENOTFOUND == hr) // on 32-bit machines
    {
        ExitFunction();
    }
    ExitOnFailure(hr, "Failed to open Windows folder key.");

    hr = RegReadString(hkFolders, wzFolderValue, &sczPath);
    ExitOnFailure1(hr, "Failed to read folder path for '%ls'.", wzFolderValue);

    // set value
    hr = BVariantSetString(pValue, sczPath, 0);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    ReleaseStr(sczPath);
    ReleaseRegKey(hkFolders);

    return hr;
}

// Get the date in the same format as Windows Installer.
static HRESULT InitializeVariableDate(
    __in DWORD_PTR /*dwpData*/,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    SYSTEMTIME systime = { };
    LPWSTR sczDate = NULL;
    int cchDate = 0;

    ::GetSystemTime(&systime);

    cchDate = ::GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &systime, NULL, NULL, cchDate);
    if (!cchDate)
    {
        ExitOnLastError(hr, "Failed to get the required buffer length for the Date.");
    }

    hr = StrAlloc(&sczDate, cchDate);
    ExitOnFailure(hr, "Failed to allocate the buffer for the Date.");

    if (!::GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &systime, NULL, sczDate, cchDate))
    {
        ExitOnLastError(hr, "Failed to get the Date.");
    }

    // set value
    hr = BVariantSetString(pValue, sczDate, cchDate);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    ReleaseStr(sczDate);

    return hr;
}

static HRESULT InitializeVariableInstallerName(
    __in DWORD_PTR /*dwpData*/,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;

    // set value
    hr = BVariantSetString(pValue, L"WiX Burn", 0);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT InitializeVariableInstallerVersion(
    __in DWORD_PTR /*dwpData*/,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczVersion = NULL;

    hr = StrAllocStringAnsi(&sczVersion, szVerMajorMinorBuild, 0, CP_ACP);
    ExitOnFailure(hr, "Failed to copy the engine version.");

    // set value
    hr = BVariantSetString(pValue, sczVersion, 0);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    ReleaseStr(sczVersion);

    return hr;
}

// Get the current user the same as Windows Installer.
static HRESULT InitializeVariableLogonUser(
    __in DWORD_PTR /*dwpData*/,
    __inout BURN_VARIANT* pValue
    )
{
    HRESULT hr = S_OK;
    WCHAR wzUserName[UNLEN + 1];
    DWORD cchUserName = countof(wzUserName);

    if (!::GetUserNameW(wzUserName, &cchUserName))
    {
        ExitOnLastError(hr, "Failed to get the user name.");
    }

    // set value
    hr = BVariantSetString(pValue, wzUserName, 0);
    ExitOnFailure(hr, "Failed to set variant value.");

LExit:
    return hr;
}

static HRESULT IsVariableHidden(
    __in BURN_VARIABLES* pVariables,
    __in_z LPCWSTR wzVariable,
    __out BOOL* pfHidden
    )
{
    HRESULT hr = S_OK;
    BURN_VARIABLE* pVariable = NULL;

    ::EnterCriticalSection(&pVariables->csAccess);

    hr = GetVariable(pVariables, wzVariable, &pVariable);
    if (E_NOTFOUND == hr)
    {
        // A missing variable does not need its data hidden.
        *pfHidden = FALSE;

        hr = S_OK;
        ExitFunction();
    }
    ExitOnFailure1(hr, "Failed to get visibility of variable: %ls", wzVariable);

    *pfHidden = pVariable->fHidden;

LExit:
    ::LeaveCriticalSection(&pVariables->csAccess);

    return hr;
}

