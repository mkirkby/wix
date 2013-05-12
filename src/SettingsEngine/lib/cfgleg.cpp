//-------------------------------------------------------------------------------------------------
// <copyright file="cfgleg.cpp" company="Outercurve Foundation">
//   Copyright (c) 2004, Outercurve Foundation.
//   This software is released under Microsoft Reciprocal License (MS-RL).
//   The license and further copyright text can be found in the file
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
//
// <summary>
//    Legacy settings engine API (these functions are NOT for legacy Apps -
//           they are for apps that want to help manage user data for legacy apps)
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"

volatile static DWORD s_dwRefCount = 0;
volatile static BOOL vfComInitialized = FALSE;
static BOOL s_fAdminAccess = FALSE;
static CFGDB_STRUCT s_cdb = { };

HRESULT CFGAPI CfgLegacyReadLatest(
    __in_bcount(CFGDB_HANDLE_BYTES) CFGDB_HANDLE cdHandle
    )
{
    HRESULT hr = S_OK;
    CFGDB_STRUCT *pcdb = static_cast<CFGDB_STRUCT *>(cdHandle);

    hr = LegacyPull(pcdb);
    ExitOnFailure(hr, "Failed to pull legacy settings");

LExit:
    return hr;
}

HRESULT CfgLegacyImportProductFromXMLFile(
    __in_bcount(CFGDB_HANDLE_BYTES) CFGDB_HANDLE cdHandle,
    __in_z LPCWSTR wzXmlFilePath
    )
{
    HRESULT hr = S_OK;
    CFGDB_STRUCT *pcdb = static_cast<CFGDB_STRUCT *>(cdHandle);
    LPWSTR sczManifestValueName = NULL;
    LPWSTR sczContent = NULL;
    LEGACY_PRODUCT product = { };
    BOOL fInSceTransaction = FALSE;
    CONFIG_VALUE cvValue = { };

    hr = FileToString(wzXmlFilePath, &sczContent, NULL);
    ExitOnFailure1(hr, "Failed to load string out of file contents from file at path: %ls", wzXmlFilePath);

    hr = ParseManifest(pcdb, sczContent, &product);
    ExitOnFailure1(hr, "Failed to parse XML manifest file from path: %ls", wzXmlFilePath);

    hr = SceBeginTransaction(pcdb->psceDb);
    ExitOnFailure(hr, "Failed to begin transaction");
    fInSceTransaction = TRUE;

    hr = ProductGetLegacyManifestValueName(product.sczProductId, &sczManifestValueName);
    ExitOnFailure(hr, "Failed to get legacy manifest value name");

    hr = ProductSet(pcdb, wzCfgProductId, wzCfgVersion, wzCfgPublicKey, TRUE, NULL);
    ExitOnFailure1(hr, "Failed to set legacy product to product ID: %ls", wzCfgProductId);

    hr = ValueSetString(sczContent, FALSE, NULL, pcdb->sczGuid, &cvValue);
    ExitOnFailure(hr, "Failed to set manifest contents as string value in memory");

    hr = ValueWrite(pcdb, pcdb->dwAppID, sczManifestValueName, &cvValue);
    ExitOnFailure(hr, "Failed to write manifest contents to database");

    hr = SceCommitTransaction(pcdb->psceDb);
    ExitOnFailure(hr, "Failed to commit transaction");
    fInSceTransaction = FALSE;

LExit:
    ReleaseCfgValue(cvValue);
    if (fInSceTransaction)
    {
        SceRollbackTransaction(pcdb->psceDb);
    }
    ManifestFreeProductStruct(&product);
    ReleaseStr(sczManifestValueName);
    ReleaseStr(sczContent);

    return hr;
}

