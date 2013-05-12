//-------------------------------------------------------------------------------------------------
// <copyright file="oldconflict.cpp" company="Outercurve Foundation">
//   Copyright (c) 2004, Outercurve Foundation.
//   This software is released under Microsoft Reciprocal License (MS-RL).
//   The license and further copyright text can be found in the file
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
//
// <summary>
// Internal utility functions for reporting and resolving conflicts
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

HRESULT ConflictGetList(
    __in const CFG_ENUMERATION *pcesInEnum1,
    __in DWORD dwCount1,
    __in const CFG_ENUMERATION *pcesInEnum2,
    __in DWORD dwCount2,
    __deref_out_bcount_opt(CFG_ENUMERATION_HANDLE_BYTES) CFG_ENUMERATION_HANDLE *pcehOutHandle1,
    __out DWORD *pdwOutCount1,
    __deref_out_bcount_opt(CFG_ENUMERATION_HANDLE_BYTES) CFG_ENUMERATION_HANDLE *pcehOutHandle2,
    __out DWORD *pdwOutCount2
    )
{
    HRESULT hr = S_OK;
    DWORD i;
    DWORD dwFoundIndex = 0;
    DWORD dwStartCopyIndex = 0;

    *pdwOutCount1 = 0;
    *pdwOutCount2 = 0;

    // we subtract 1 because it's a 0-based array, and 1 more because we already checked the last index of the array
    // when we checked for subsumation
    dwStartCopyIndex = 0;
    for (i = dwCount1 - 2; i != DWORD_MAX; --i)
    {
        hr = EnumFindValueInHistory(pcesInEnum2, dwCount2, pcesInEnum1, i, &dwFoundIndex);
        if (E_NOTFOUND == hr)
        {
            hr = S_OK;
            continue;
        }
        ExitOnFailure(hr, "Failed to search in enum 2");

        dwStartCopyIndex = i + 1;
        break;
    }

    // Now create pcehOutHandle1 with all the members of pcesInEnum1 after index dwFoundIndex
    hr = EnumCopy(pcesInEnum1, dwCount1, dwStartCopyIndex, reinterpret_cast<CFG_ENUMERATION **>(pcehOutHandle1), pdwOutCount1);
    ExitOnFailure(hr, "Failed to copy enumeration struct 1");

    // we subtract 1 because it's a 0-based array, and 1 more because we already checked the last index of the array
    // when we checked for subsumation
    dwStartCopyIndex = 0;
    for (i = dwCount2 - 2; i != DWORD_MAX; --i)
    {
        hr = EnumFindValueInHistory(pcesInEnum1, dwCount1, pcesInEnum2, i, &dwFoundIndex);
        if (E_NOTFOUND == hr)
        {
            hr = S_OK;
            continue;
        }
        ExitOnFailure(hr, "Failed to search in enum 1");

        dwStartCopyIndex = i + 1;
        break;
    }

    // Now create pcehOutHandle1 with all the members of pcesInEnum1 after index dwFoundIndex
    hr = EnumCopy(pcesInEnum2, dwCount2, dwStartCopyIndex, reinterpret_cast<CFG_ENUMERATION **>(pcehOutHandle2), pdwOutCount2);
    ExitOnFailure(hr, "Failed to copy enumeration struct 2");

LExit:
    if (FAILED(hr))
    {
        EnumFree(static_cast<CFG_ENUMERATION *>(*pcehOutHandle1));
        *pcehOutHandle1 = NULL;
        EnumFree(static_cast<CFG_ENUMERATION *>(*pcehOutHandle2));
        *pcehOutHandle2 = NULL;
    }

    return hr;
}

HRESULT ConflictResolve(
    __in CFGDB_STRUCT *pcdb,
    __in CONFLICT_PRODUCT *cpProduct,
    __in DWORD dwValueIndex
    )
{
    HRESULT hr = S_OK;
    LPCWSTR wzValueName = NULL;
    RESOLUTION_CHOICE rcChoice = cpProduct->rgrcValueChoices[dwValueIndex];
    DWORD i;

    hr = CfgEnumReadString(cpProduct->rgcesValueEnumLocal[dwValueIndex], 0, ENUM_DATA_VALUENAME, &wzValueName);
    ExitOnFailure(hr, "Failed to read value name from enum");

    // TODO: Make sure the values haven't changed since the time these conflicts were created

    if (RESOLUTION_LOCAL == rcChoice)
    {
        // Local Cfg database wins, transfer all history
        for (i = 0; i < cpProduct->rgdwValueCountLocal[dwValueIndex]; ++i)
        {
            // TODO: In some cases this is writing to the main value when it's about to be overwritten again. Optimize?
            hr = ValueWriteFromHistoryEnum(pcdb, wzValueName, static_cast<CFG_ENUMERATION *>(cpProduct->rgcesValueEnumLocal[dwValueIndex]), i);
            ExitOnFailure(hr, "Failed to set value from enumeration history while accepting local value");
        }
    }
    else if (RESOLUTION_REMOTE == rcChoice)
    {
        // Remote database wins
        for (i = 0; i < cpProduct->rgdwValueCountRemote[dwValueIndex]; ++i)
        {
            // TODO: In some cases this is writing to the main value when it's about to be overwritten again. Optimize?
            hr = ValueWriteFromHistoryEnum(pcdb->pcdbLocal, wzValueName, static_cast<CFG_ENUMERATION *>(cpProduct->rgcesValueEnumRemote[dwValueIndex]), i);
            ExitOnFailure(hr, "Failed to set value from enumeration history while accepting remote value");
        }
    }
    
LExit:
    return hr;
}
