//-------------------------------------------------------------------------------------------------
// <copyright file="core.cpp" company="Microsoft">
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
//    Setup chainer/bootstrapper core for WiX toolset.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"


// structs

struct BURN_CACHE_THREAD_CONTEXT
{
    BURN_ENGINE_STATE* pEngineState;
    DWORD* pcOverallProgressTicks;
    BOOL* pfRollback;
};


// internal function declarations

static HRESULT ParseCommandLine(
    __in_z_opt LPCWSTR wzCommandLine,
    __in BOOTSTRAPPER_COMMAND* pCommand,
    __in BURN_PIPE_CONNECTION* pCompanionConnection,
    __in BURN_PIPE_CONNECTION* pEmbeddedConnection,
    __out BURN_MODE* pMode,
    __out BURN_ELEVATION_STATE* pElevationState,
    __out BOOL* pfDisableUnelevate,
    __out DWORD *pdwLoggingAttributes,
    __out_z LPWSTR* psczLogFile,
    __out_z LPWSTR* psczIgnoreDependencies
    );
static HRESULT ParsePipeConnection(
    __in LPWSTR* rgArgs,
    __in BURN_PIPE_CONNECTION* pConnection
    );
static HRESULT DetectPackagePayloadsCached(
    __in BURN_PACKAGE* pPackage
    );
static DWORD WINAPI CacheThreadProc(
    __in LPVOID lpThreadParameter
    );
static HRESULT WaitForCacheThread(
    __in HANDLE hCacheThread
    );


// function definitions

extern "C" HRESULT CoreInitialize(
    __in_z_opt LPCWSTR wzCommandLine,
    __in BURN_ENGINE_STATE* pEngineState
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczStreamName = NULL;
    BYTE* pbBuffer = NULL;
    SIZE_T cbBuffer = 0;
    BURN_CONTAINER_CONTEXT containerContext = { };

    // parse command line
    hr = ParseCommandLine(wzCommandLine, &pEngineState->command, &pEngineState->companionConnection, &pEngineState->embeddedConnection, &pEngineState->mode, &pEngineState->elevationState, &pEngineState->fDisableUnelevate, &pEngineState->log.dwAttributes, &pEngineState->log.sczPath, &pEngineState->sczIgnoreDependencies);
    ExitOnFailure(hr, "Failed to parse command line.");

    // initialize variables
    hr = VariableInitialize(&pEngineState->variables);
    ExitOnFailure(hr, "Failed to initialize variables.");

    // retain whether burn was initially run elevated
    hr = VariableSetNumeric(&pEngineState->variables, BURN_BUNDLE_ELEVATED, BURN_ELEVATION_STATE_UNELEVATED != pEngineState->elevationState, TRUE);
    ExitOnFailure1(hr, "Failed to overwrite the %ls built-in variable.", BURN_BUNDLE_ELEVATED);

    // open attached UX container
    hr = SectionInitialize(&pEngineState->section);
    ExitOnFailure(hr, "Failed to load section information.");

    hr = ContainerOpenUX(&pEngineState->section, &containerContext);
    ExitOnFailure(hr, "Failed to open attached UX container.");

    // load manifest
    hr = ContainerNextStream(&containerContext, &sczStreamName);
    ExitOnFailure(hr, "Failed to open manifest stream.");

    hr = ContainerStreamToBuffer(&containerContext, &pbBuffer, &cbBuffer);
    ExitOnFailure(hr, "Failed to get manifest stream from container.");

    hr = ManifestLoadXmlFromBuffer(pbBuffer, cbBuffer, pEngineState);
    ExitOnFailure(hr, "Failed to load manifest.");

    // If we're not elevated then we'll be loading the bootstrapper application, so extract
    // the payloads from the BA container.
    if (pEngineState->elevationState == BURN_ELEVATION_STATE_UNELEVATED || pEngineState->elevationState == BURN_ELEVATION_STATE_UNELEVATED_EXPLICITLY)
    {
        // Extract all UX payloads to working folder.
        hr = UserExperienceEnsureWorkingFolder(pEngineState->registration.sczId, &pEngineState->userExperience.sczTempDirectory);
        ExitOnFailure(hr, "Failed to get unique temporary folder for bootstrapper application.");

        hr = PayloadExtractFromContainer(&pEngineState->userExperience.payloads, NULL, &containerContext, pEngineState->userExperience.sczTempDirectory);
        ExitOnFailure(hr, "Failed to extract bootstrapper application payloads.");

        // Load the catalog files as soon as they are extracted
        hr = CatalogLoadFromPayload(&pEngineState->catalogs, &pEngineState->userExperience.payloads);
        ExitOnFailure(hr, "Failed to load catalog files.");
    }

LExit:
    ContainerClose(&containerContext);
    ReleaseStr(sczStreamName);
    ReleaseMem(pbBuffer);

    return hr;
}

extern "C" HRESULT CoreSerializeEngineState(
    __in BURN_ENGINE_STATE* pEngineState,
    __inout BYTE** ppbBuffer,
    __inout SIZE_T* piBuffer
    )
{
    HRESULT hr = S_OK;

    hr = VariableSerialize(&pEngineState->variables, TRUE, ppbBuffer, piBuffer);
    ExitOnFailure(hr, "Failed to serialize variables.");

LExit:
    return hr;
}

extern "C" HRESULT CoreQueryRegistration(
    __in BURN_ENGINE_STATE* pEngineState
    )
{
    HRESULT hr = S_OK;
    BYTE* pbBuffer = NULL;
    SIZE_T cbBuffer = 0;
    SIZE_T iBuffer = 0;

    // Detect if bundle is already installed.
    RegistrationDetectInstalled(&pEngineState->registration, &pEngineState->registration.fInstalled);

    // detect resume type
    hr = RegistrationDetectResumeType(&pEngineState->registration, &pEngineState->command.resumeType);
    ExitOnFailure(hr, "Failed to detect resume type.");

    // If we have a resume mode that suggests the bundle might already be present, try to load any
    // previously stored state.
    if (BOOTSTRAPPER_RESUME_TYPE_INVALID < pEngineState->command.resumeType)
    {
        // load resume state
        hr = RegistrationLoadState(&pEngineState->registration, &pbBuffer, &cbBuffer);
        if (SUCCEEDED(hr))
        {
            hr = VariableDeserialize(&pEngineState->variables, pbBuffer, cbBuffer, &iBuffer);
        }

        // Log any failures and continue.
        if (FAILED(hr))
        {
            LogId(REPORT_STANDARD, MSG_CANNOT_LOAD_STATE_FILE, hr, pEngineState->registration.sczStateFile);
            hr = S_OK;
        }
    }

LExit:
    ReleaseBuffer(pbBuffer);

    return hr;
}

extern "C" HRESULT CoreDetect(
    __in BURN_ENGINE_STATE* pEngineState
    )
{
    HRESULT hr = S_OK;
    BOOL fActivated = FALSE;
    BURN_PACKAGE* pPackage = NULL;
    HRESULT hrFirstPackageFailure = S_OK;

    LogId(REPORT_STANDARD, MSG_DETECT_BEGIN, pEngineState->packages.cPackages);

    hr = UserExperienceActivateEngine(&pEngineState->userExperience, &fActivated);
    ExitOnFailure(hr, "Engine cannot start detect because it is busy with another action.");

    int nResult = pEngineState->userExperience.pUserExperience->OnDetectBegin(pEngineState->packages.cPackages);
    hr = HRESULT_FROM_VIEW(nResult);
    ExitOnRootFailure(hr, "UX aborted detect begin.");

    hr = SearchesExecute(&pEngineState->searches, &pEngineState->variables);
    ExitOnFailure(hr, "Failed to execute searches.");

    hr = RegistrationDetectRelatedBundles(BURN_MODE_ELEVATED == pEngineState->mode, &pEngineState->userExperience, &pEngineState->registration, &pEngineState->command);
    ExitOnFailure(hr, "Failed to detect bundles.");

    // Detecting MSPs requires special initialization before processing each package but
    // only do the detection if there are actually patch packages to detect because it
    // can be expensive.
    if (pEngineState->packages.cPatchInfo)
    {
        hr = MspEngineDetectInitialize(&pEngineState->packages);
        ExitOnFailure(hr, "Failed to initialize MSP engine detection.");
    }

    for (DWORD i = 0; i < pEngineState->packages.cPackages; ++i)
    {
        pPackage = pEngineState->packages.rgPackages + i;

        nResult = pEngineState->userExperience.pUserExperience->OnDetectPackageBegin(pPackage->sczId);
        hr = HRESULT_FROM_VIEW(nResult);
        ExitOnRootFailure(hr, "UX aborted detect package begin.");

        // Detect the cache state of the package.
        hr = DetectPackagePayloadsCached(pPackage);
        ExitOnFailure1(hr, "Failed to detect if payloads are all cached for package: %ls", pPackage->sczId);

        // Use the correct engine to detect the package.
        switch (pPackage->type)
        {
        case BURN_PACKAGE_TYPE_EXE:
            hr = ExeEngineDetectPackage(pPackage, &pEngineState->variables);
            break;

        case BURN_PACKAGE_TYPE_MSI:
            hr = MsiEngineDetectPackage(pPackage, &pEngineState->userExperience);
            break;

        case BURN_PACKAGE_TYPE_MSP:
            hr = MspEngineDetectPackage(pPackage, &pEngineState->userExperience);
            break;

        case BURN_PACKAGE_TYPE_MSU:
            hr = MsuEngineDetectPackage(pPackage, &pEngineState->variables);
            break;

        default:
            hr = E_NOTIMPL;
            ExitOnRootFailure(hr, "Package type not supported by detect yet.");
        }

        // If the package detection failed, ensure the package state is set to unknown.
        if (FAILED(hr))
        {
            if (SUCCEEDED(hrFirstPackageFailure))
            {
                hrFirstPackageFailure = hr;
            }

            pPackage->currentState = BOOTSTRAPPER_PACKAGE_STATE_UNKNOWN;
            LogErrorId(hr, MSG_FAILED_DETECT_PACKAGE, pPackage->sczId, NULL, NULL);
        }
        // TODO: consider how to notify the UX that a package is cached.
        //else if (BOOTSTRAPPER_PACKAGE_STATE_CACHED > pPackage->currentState && pPackage->fCached)
        //{
        //     pPackage->currentState = BOOTSTRAPPER_PACKAGE_STATE_CACHED;
        //}

        LogId(REPORT_STANDARD, MSG_DETECTED_PACKAGE, pPackage->sczId, LoggingPackageStateToString(pPackage->currentState), LoggingBoolToString(pPackage->fCached));
        pEngineState->userExperience.pUserExperience->OnDetectPackageComplete(pPackage->sczId, hr, pPackage->currentState);
    }

LExit:
    if (SUCCEEDED(hr))
    {
        hr = hrFirstPackageFailure;
    }

    if (fActivated)
    {
        UserExperienceDeactivateEngine(&pEngineState->userExperience);
    }

    pEngineState->userExperience.pUserExperience->OnDetectComplete(hr);

    LogId(REPORT_STANDARD, MSG_DETECT_COMPLETE, hr);

    return hr;
}

extern "C" HRESULT CorePlan(
    __in BURN_ENGINE_STATE* pEngineState,
    __in BOOTSTRAPPER_ACTION action
    )
{
    HRESULT hr = S_OK;
    BOOL fActivated = FALSE;
    LPWSTR sczLayoutDirectory = NULL;
    BURN_PACKAGE* pPackage = NULL;
    DWORD dwExecuteActionEarlyIndex = 0;
    HANDLE hSyncpointEvent = NULL;
    BOOTSTRAPPER_REQUEST_STATE defaultRequested = BOOTSTRAPPER_REQUEST_STATE_NONE;
    BOOL fPlannedCachePackage = FALSE;
    BOOL fPlannedCleanPackage = FALSE;
    BURN_ROLLBACK_BOUNDARY* pRollbackBoundary = NULL;
    HANDLE hRollbackBoundaryCompleteEvent = NULL;
    DWORD iAfterExecuteFirstNonPermanentPackage = BURN_PLAN_INVALID_ACTION_INDEX;
    DWORD iBeforeRollbackFirstNonPermanentPackage = BURN_PLAN_INVALID_ACTION_INDEX;
    DWORD iAfterExecuteLastNonPermanentPackage = BURN_PLAN_INVALID_ACTION_INDEX;
    DWORD iAfterRollbackLastNonPermanentPackage = BURN_PLAN_INVALID_ACTION_INDEX;

    LogId(REPORT_STANDARD, MSG_PLAN_BEGIN, pEngineState->packages.cPackages, LoggingBurnActionToString(action));

    hr = UserExperienceActivateEngine(&pEngineState->userExperience, &fActivated);
    ExitOnFailure(hr, "Engine cannot start plan because it is busy with another action.");

    int nResult = pEngineState->userExperience.pUserExperience->OnPlanBegin(pEngineState->packages.cPackages);
    hr = HRESULT_FROM_VIEW(nResult);
    ExitOnRootFailure(hr, "UX aborted plan begin.");

    // Always reset the plan.
    PlanUninitialize(&pEngineState->plan, &pEngineState->packages);

    // Remember the overall action state in the plan since it shapes the changes
    // we make everywhere.
    pEngineState->plan.action = action;
    pEngineState->plan.wzBundleId = pEngineState->registration.sczId;

    // By default we want to keep the registration if the bundle was already installed.
    pEngineState->plan.fKeepRegistrationDefault = pEngineState->registration.fInstalled;

    // Set resume commandline
    hr = PlanSetResumeCommand(&pEngineState->registration, action, &pEngineState->command, &pEngineState->log);
    ExitOnFailure(hr, "Failed to set resume command");

    hr = DependencyPlanInitialize(pEngineState, &pEngineState->plan);
    ExitOnFailure(hr, "Failed to initialize the dependencies for the plan.");

    if (BOOTSTRAPPER_ACTION_LAYOUT == action)
    {
        // Plan the bundle's layout.
        hr = PlanLayoutBundle(&pEngineState->plan, pEngineState->registration.sczExecutableName, &pEngineState->variables, &pEngineState->payloads, &sczLayoutDirectory);
        ExitOnFailure(hr, "Failed to plan the layout of the bundle.");
    }
    else if (pEngineState->registration.fPerMachine) // the registration of this bundle is per-machine then the plan needs to be per-machine as well.
    {
        pEngineState->plan.fPerMachine = TRUE;
    }

    // Remember the early index, because we want to be able to insert some related bundles
    // into the plan before other executed packages. This particularly occurs for uninstallation
    // of addons, which should be uninstalled before the main product.
    dwExecuteActionEarlyIndex = pEngineState->plan.cExecuteActions;

    // Plan the packages.
    for (DWORD i = 0; i < pEngineState->packages.cPackages; ++i)
    {
        DWORD iPackage = (BOOTSTRAPPER_ACTION_UNINSTALL == action) ? pEngineState->packages.cPackages - 1 - i : i;
        pPackage = pEngineState->packages.rgPackages + iPackage;
        BURN_ROLLBACK_BOUNDARY* pEffectiveRollbackBoundary = (BOOTSTRAPPER_ACTION_UNINSTALL == action) ? pPackage->pRollbackBoundaryBackward : pPackage->pRollbackBoundaryForward;

        fPlannedCachePackage = FALSE;
        fPlannedCleanPackage = FALSE;

        // If the package marks the start of a rollback boundary, start a new one.
        if (pEffectiveRollbackBoundary)
        {
            // Complete previous rollback boundary.
            if (pRollbackBoundary)
            {
                hr = PlanRollbackBoundaryComplete(&pEngineState->plan, hRollbackBoundaryCompleteEvent);
                ExitOnFailure(hr, "Failed to plan rollback boundary complete.");
            }

            // Start new rollback boundary.
            hr = PlanRollbackBoundaryBegin(&pEngineState->plan, pEffectiveRollbackBoundary, &hRollbackBoundaryCompleteEvent);
            ExitOnFailure(hr, "Failed to plan rollback boundary begin.");

            pRollbackBoundary = pEffectiveRollbackBoundary;
        }

        // Remember the default requested state so the engine doesn't get blamed for planning the wrong thing if the UX changes it.
        hr = PlanDefaultPackageRequestState(pPackage->type, pPackage->currentState, !pPackage->fUninstallable, action, &pEngineState->variables, pPackage->sczInstallCondition, pEngineState->command.relationType, &defaultRequested);
        ExitOnFailure(hr, "Failed to set default package state.");

        pPackage->requested = defaultRequested;

        nResult = pEngineState->userExperience.pUserExperience->OnPlanPackageBegin(pPackage->sczId, &pPackage->requested);
        hr = HRESULT_FROM_VIEW(nResult);
        ExitOnRootFailure(hr, "UX aborted plan package begin.");

        // If the the package is in a requested state, plan it.
        if (BOOTSTRAPPER_REQUEST_STATE_NONE != pPackage->requested)
        {
            if (BOOTSTRAPPER_ACTION_LAYOUT == action)
            {
                hr = PlanLayoutPackage(&pEngineState->plan, pPackage, sczLayoutDirectory);
                ExitOnFailure(hr, "Failed to plan layout package.");
            }
            else
            {
                if (pPackage->fUninstallable)
                {
                    if (BURN_PLAN_INVALID_ACTION_INDEX == iBeforeRollbackFirstNonPermanentPackage)
                    {
                        iBeforeRollbackFirstNonPermanentPackage = pEngineState->plan.cRollbackActions;
                    }
                }

                hr = PlanExecutePackage(pEngineState->command.display, &pEngineState->userExperience, &pEngineState->plan, pPackage, &pEngineState->log, &pEngineState->variables, pEngineState->registration.sczProviderKey, &hSyncpointEvent, &fPlannedCachePackage, &fPlannedCleanPackage);
                ExitOnFailure(hr, "Failed to plan execute package.");

                if (pPackage->fUninstallable)
                {
                    if (BURN_PLAN_INVALID_ACTION_INDEX == iAfterExecuteFirstNonPermanentPackage)
                    {
                        iAfterExecuteFirstNonPermanentPackage = pEngineState->plan.cExecuteActions - 1;
                    }

                    iAfterExecuteLastNonPermanentPackage = pEngineState->plan.cExecuteActions;
                    iAfterRollbackLastNonPermanentPackage = pEngineState->plan.cRollbackActions;
                }
            }
        }
        else if (BOOTSTRAPPER_ACTION_LAYOUT != action)
        {
            // Make sure the package is properly ref-counted even if no plan is requested.
            hr = DependencyPlanPackageBegin(pPackage, &pEngineState->plan, pEngineState->registration.sczProviderKey);
            ExitOnFailure1(hr, "Failed to plan dependency actions to unregister package: %ls", pPackage->sczId);

            hr = DependencyPlanPackageComplete(pPackage, &pEngineState->plan, pEngineState->registration.sczProviderKey);
            ExitOnFailure1(hr, "Failed to plan dependency actions to register package: %ls", pPackage->sczId);
        }

        // Add the checkpoint after each package and dependency registration action.
        if (BOOTSTRAPPER_ACTION_STATE_NONE != pPackage->execute || BOOTSTRAPPER_ACTION_STATE_NONE != pPackage->rollback || BURN_DEPENDENCY_ACTION_NONE != pPackage->dependency)
        {
            hr = PlanExecuteCheckpoint(&pEngineState->plan);
            ExitOnFailure(hr, "Failed to append execute checkpoint.");
        }

        LogId(REPORT_STANDARD, MSG_PLANNED_PACKAGE, pPackage->sczId, LoggingPackageStateToString(pPackage->currentState), LoggingRequestStateToString(defaultRequested), LoggingRequestStateToString(pPackage->requested), LoggingActionStateToString(pPackage->execute), LoggingActionStateToString(pPackage->rollback), LoggingBoolToString(fPlannedCachePackage), LoggingBoolToString(fPlannedCleanPackage), LoggingDependencyActionToString(pPackage->dependency));

        pEngineState->userExperience.pUserExperience->OnPlanPackageComplete(pPackage->sczId, hr, pPackage->currentState, pPackage->requested, pPackage->execute, pPackage->rollback);
    }

    // Insert the "keep registration" and "remove registration" actions in the plan when installing the first time and anytime we are uninstalling respectively.
    if (!pEngineState->registration.fInstalled && (BOOTSTRAPPER_ACTION_INSTALL == action || BOOTSTRAPPER_ACTION_MODIFY == action || BOOTSTRAPPER_ACTION_REPAIR == action))
    {
        hr = PlanKeepRegistration(&pEngineState->plan, iAfterExecuteFirstNonPermanentPackage, iBeforeRollbackFirstNonPermanentPackage);
        ExitOnFailure(hr, "Failed to plan install keep registration.");
    }
    else if (BOOTSTRAPPER_ACTION_UNINSTALL == action)
    {
        hr = PlanRemoveRegistration(&pEngineState->plan, iAfterExecuteLastNonPermanentPackage, iAfterRollbackLastNonPermanentPackage);
        ExitOnFailure(hr, "Failed to plan uninstall remove registration.");
    }

    // If we still have an open rollback boundary, complete it.
    if (pRollbackBoundary)
    {
        hr = PlanRollbackBoundaryComplete(&pEngineState->plan, hRollbackBoundaryCompleteEvent);
        ExitOnFailure(hr, "Failed to plan rollback boundary begin.");

        pRollbackBoundary = NULL;
        hRollbackBoundaryCompleteEvent = NULL;
    }

    // Plan the update of related bundles last as long as we are not doing layout only.
    if (BOOTSTRAPPER_ACTION_LAYOUT != action)
    {
        hr = PlanRelatedBundles(action, &pEngineState->userExperience, &pEngineState->registration.relatedBundles, pEngineState->registration.qwVersion, &pEngineState->plan, &pEngineState->log, &pEngineState->variables, &hSyncpointEvent, dwExecuteActionEarlyIndex);
        ExitOnFailure(hr, "Failed to plan related bundles.");
    }

#ifdef DEBUG
    PlanDump(&pEngineState->plan);
#endif

LExit:
    if (fActivated)
    {
        UserExperienceDeactivateEngine(&pEngineState->userExperience);
    }

    pEngineState->userExperience.pUserExperience->OnPlanComplete(hr);

    LogId(REPORT_STANDARD, MSG_PLAN_COMPLETE, hr);
    ReleaseStr(sczLayoutDirectory);

    return hr;
}

extern "C" HRESULT CoreElevate(
    __in BURN_ENGINE_STATE* pEngineState,
    __in_opt HWND hwndParent
    )
{
    HRESULT hr = S_OK;

    // If the elevated companion pipe isn't created yet, let's make that happen.
    if (INVALID_HANDLE_VALUE == pEngineState->companionConnection.hPipe)
    {
        hr = ElevationElevate(pEngineState, hwndParent);
        ExitOnFailure(hr, "Failed to actually elevate.");

        hr = VariableSetNumeric(&pEngineState->variables, BURN_BUNDLE_ELEVATED, TRUE, TRUE);
        ExitOnFailure1(hr, "Failed to overwrite the %ls built-in variable.", BURN_BUNDLE_ELEVATED);
    }

LExit:
    return hr;
}

extern "C" HRESULT CoreApply(
    __in BURN_ENGINE_STATE* pEngineState,
    __in_opt HWND hwndParent
    )
{
    HRESULT hr = S_OK;
    BOOL fLayoutOnly = (BOOTSTRAPPER_ACTION_LAYOUT == pEngineState->plan.action);
    BOOL fActivated = FALSE;
    DWORD cOverallProgressTicks = 0;
    HANDLE hCacheThread = NULL;
    BOOL fRegistered = FALSE;
    BOOL fKeepRegistration = pEngineState->plan.fKeepRegistrationDefault;
    BOOL fRollback = FALSE;
    BOOL fSuspend = FALSE;
    BOOTSTRAPPER_APPLY_RESTART restart = BOOTSTRAPPER_APPLY_RESTART_NONE;
    BURN_CACHE_THREAD_CONTEXT cacheThreadContext = { };

    LogId(REPORT_STANDARD, MSG_APPLY_BEGIN);

    hr = UserExperienceActivateEngine(&pEngineState->userExperience, &fActivated);
    ExitOnFailure(hr, "Engine cannot start apply because it is busy with another action.");

    int nResult = pEngineState->userExperience.pUserExperience->OnApplyBegin();
    hr = HRESULT_FROM_VIEW(nResult);
    ExitOnRootFailure(hr, "UX aborted apply begin.");

    // If the plan contains per-machine contents, let's make sure we are elevated.
    if (pEngineState->plan.fPerMachine)
    {
        AssertSz(!fLayoutOnly, "A Layout plan should never require elevation.");

        hr = CoreElevate(pEngineState, hwndParent);
        ExitOnFailure(hr, "Failed to elevate.");
    }

    // Register only if we are not doing a layout.
    if (!fLayoutOnly)
    {
        hr = ApplyRegister(pEngineState);
        ExitOnFailure(hr, "Failed to register bundle.");
        fRegistered = TRUE;
    }

    // Launch the cache thread.
    cacheThreadContext.pEngineState = pEngineState;
    cacheThreadContext.pcOverallProgressTicks = &cOverallProgressTicks;
    cacheThreadContext.pfRollback = &fRollback;

    hCacheThread = ::CreateThread(NULL, 0, CacheThreadProc, &cacheThreadContext, 0, NULL);
    ExitOnNullWithLastError(hCacheThread, hr, "Failed to create cache thread.");

    // If we're not caching in parallel, wait for the cache thread to terminate.
    if (!pEngineState->fParallelCacheAndExecute)
    {
        hr = WaitForCacheThread(hCacheThread);
        ExitOnFailure(hr, "Failed while waiting for cache thread to complete before executing.");

        ReleaseHandle(hCacheThread);
    }

    // Execute only if we are not doing a layout.
    if (!fLayoutOnly)
    {
        hr = ApplyExecute(pEngineState, hwndParent, hCacheThread, &cOverallProgressTicks, &fKeepRegistration, &fRollback, &fSuspend, &restart);
        ExitOnFailure(hr, "Failed to execute apply.");
    }

    // Wait for cache thread to terminate, this should return immediately (unless we're waiting for layout to complete).
    if (hCacheThread)
    {
        hr = WaitForCacheThread(hCacheThread);
        ExitOnFailure(hr, "Failed while waiting for cache thread to complete after execution.");
    }

    if (fRollback || fSuspend || BOOTSTRAPPER_APPLY_RESTART_INITIATED == restart)
    {
        ExitFunction();
    }

    ApplyClean(&pEngineState->userExperience, &pEngineState->plan, pEngineState->companionConnection.hPipe);

LExit:
    if (fRegistered)
    {
        ApplyUnregister(pEngineState, fKeepRegistration, fSuspend, restart);
    }

    if (fActivated)
    {
        UserExperienceDeactivateEngine(&pEngineState->userExperience);
    }

    ReleaseHandle(hCacheThread);

    nResult = pEngineState->userExperience.pUserExperience->OnApplyComplete(hr, restart);
    if (IDRESTART == nResult)
    {
        pEngineState->fRestart = TRUE;
    }

    LogId(REPORT_STANDARD, MSG_APPLY_COMPLETE, hr, LoggingBoolToString(pEngineState->fRestart));

    return hr;
}

extern "C" HRESULT CoreQuit(
    __in BURN_ENGINE_STATE* pEngineState,
    __in int nExitCode
    )
{
    HRESULT hr = S_OK;

    // Save engine state if resume mode is unequal to "none".
    if (BURN_RESUME_MODE_NONE != pEngineState->resumeMode)
    {
        hr = CoreSaveEngineState(pEngineState);
        if (FAILED(hr))
        {
            LogErrorId(hr, MSG_STATE_NOT_SAVED, NULL, NULL, NULL);
            hr = S_OK;
        }
   }

    LogId(REPORT_STANDARD, MSG_QUIT, nExitCode);

    ::PostQuitMessage(nExitCode); // go bye-bye.

    return hr;
}

extern "C" HRESULT CoreSaveEngineState(
    __in BURN_ENGINE_STATE* pEngineState
    )
{
    HRESULT hr = S_OK;
    BYTE* pbBuffer = NULL;
    SIZE_T cbBuffer = 0;

    // serialize engine state
    hr = CoreSerializeEngineState(pEngineState, &pbBuffer, &cbBuffer);
    ExitOnFailure(hr, "Failed to serialize engine state.");

    // write to registration store
    if (pEngineState->registration.fPerMachine)
    {
        hr = ElevationSaveState(pEngineState->companionConnection.hPipe, pbBuffer, cbBuffer);
        ExitOnFailure(hr, "Failed to save engine state in per-machine process.");
    }
    else
    {
        hr = RegistrationSaveState(&pEngineState->registration, pbBuffer, cbBuffer);
        ExitOnFailure(hr, "Failed to save engine state.");
    }

LExit:
    ReleaseBuffer(pbBuffer);

    return hr;
}


// internal helper functions

static HRESULT ParseCommandLine(
    __in_z_opt LPCWSTR wzCommandLine,
    __in BOOTSTRAPPER_COMMAND* pCommand,
    __in BURN_PIPE_CONNECTION* pCompanionConnection,
    __in BURN_PIPE_CONNECTION* pEmbeddedConnection,
    __out BURN_MODE* pMode,
    __out BURN_ELEVATION_STATE* pElevationState,
    __out BOOL* pfDisableUnelevate,
    __out DWORD *pdwLoggingAttributes,
    __out_z LPWSTR* psczLogFile,
    __out_z LPWSTR* psczIgnoreDependencies
    )
{
    HRESULT hr = S_OK;
    int argc = 0;
    LPWSTR* argv = NULL;
    BOOL fUnknownArg = FALSE;

    if (wzCommandLine && *wzCommandLine)
    {
        argv = ::CommandLineToArgvW(wzCommandLine, &argc);
        ExitOnNullWithLastError(argv, hr, "Failed to get command line.");
    }

    for (int i = 0; i < argc; ++i)
    {
        fUnknownArg = FALSE;

        if (argv[i][0] == L'-' || argv[i][0] == L'/')
        {
            if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"l", -1) ||
                CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"log", -1))
            {
                *pdwLoggingAttributes &= ~BURN_LOGGING_ATTRIBUTE_APPEND;

                if (i + 1 >= argc)
                {
                    ExitOnRootFailure(hr = E_INVALIDARG, "Must specify a path for log.");
                }

                ++i;

                hr = StrAllocString(psczLogFile, argv[i], 0);
                ExitOnFailure(hr, "Failed to copy log file path.");
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"?", -1) ||
                     CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"h", -1) ||
                     CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"help", -1))
            {
                pCommand->action = BOOTSTRAPPER_ACTION_HELP;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"q", -1) ||
                     CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"quiet", -1) ||
                     CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"s", -1) ||
                     CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"silent", -1))
            {
                pCommand->display = BOOTSTRAPPER_DISPLAY_NONE;

                if (BOOTSTRAPPER_RESTART_UNKNOWN == pCommand->restart)
                {
                    pCommand->restart = BOOTSTRAPPER_RESTART_AUTOMATIC;
                }
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"passive", -1))
            {
                pCommand->display = BOOTSTRAPPER_DISPLAY_PASSIVE;

                if (BOOTSTRAPPER_RESTART_UNKNOWN == pCommand->restart)
                {
                    pCommand->restart = BOOTSTRAPPER_RESTART_AUTOMATIC;
                }
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"norestart", -1))
            {
                pCommand->restart = BOOTSTRAPPER_RESTART_NEVER;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"forcerestart", -1))
            {
                pCommand->restart = BOOTSTRAPPER_RESTART_ALWAYS;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"promptrestart", -1))
            {
                pCommand->restart = BOOTSTRAPPER_RESTART_PROMPT;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"layout", -1))
            {
                if (BOOTSTRAPPER_ACTION_HELP != pCommand->action)
                {
                    pCommand->action = BOOTSTRAPPER_ACTION_LAYOUT;
                }

                // If there is another command line argument and it is not a switch, use that as the layout directory.
                if (i + 1 < argc && argv[i + 1][0] != L'-' && argv[i + 1][0] != L'/')
                {
                    ++i;

                    hr = PathExpand(&pCommand->wzLayoutDirectory, argv[i], PATH_EXPAND_ENVIRONMENT | PATH_EXPAND_FULLPATH);
                    ExitOnFailure(hr, "Failed to copy path for layout directory.");
                }
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"uninstall", -1))
            {
                if (BOOTSTRAPPER_ACTION_HELP != pCommand->action)
                {
                    pCommand->action = BOOTSTRAPPER_ACTION_UNINSTALL;
                }
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"repair", -1))
            {
                if (BOOTSTRAPPER_ACTION_HELP != pCommand->action)
                {
                    pCommand->action = BOOTSTRAPPER_ACTION_REPAIR;
                }
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"modify", -1))
            {
                if (BOOTSTRAPPER_ACTION_HELP != pCommand->action)
                {
                    pCommand->action = BOOTSTRAPPER_ACTION_MODIFY;
                }
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"package", -1) ||
                     CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, L"update", -1))
            {
                if (BOOTSTRAPPER_ACTION_UNKNOWN == pCommand->action)
                {
                    pCommand->action = BOOTSTRAPPER_ACTION_INSTALL;
                }
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_LOG_APPEND, -1))
            {
                if (i + 1 >= argc)
                {
                    ExitOnRootFailure(hr = E_INVALIDARG, "Must specify a path for append log.");
                }

                ++i;

                hr = StrAllocString(psczLogFile, argv[i], 0);
                ExitOnFailure(hr, "Failed to copy append log file path.");

                *pdwLoggingAttributes |= BURN_LOGGING_ATTRIBUTE_APPEND;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_ELEVATED, -1))
            {
                if (i + 3 >= argc)
                {
                    ExitOnRootFailure(hr = E_INVALIDARG, "Must specify the elevated name, token and parent process id.");
                }

                *pElevationState = BURN_ELEVATION_STATE_ELEVATED_EXPLICITLY;

                ++i;

                hr = ParsePipeConnection(argv + i, pCompanionConnection);
                ExitOnFailure(hr, "Failed to parse elevated connection.");

                i += 2;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_UNELEVATED, -1))
            {
                if (i + 3 >= argc)
                {
                    ExitOnRootFailure(hr = E_INVALIDARG, "Must specify the unelevated name, token and parent process id.");
                }

                *pElevationState = BURN_ELEVATION_STATE_UNELEVATED_EXPLICITLY;

                ++i;

                hr = ParsePipeConnection(argv + i, pCompanionConnection);
                ExitOnFailure(hr, "Failed to parse unelevated connection.");

                i += 2;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_EMBEDDED, -1))
            {
                if (i + 3 >= argc)
                {
                    ExitOnRootFailure(hr = E_INVALIDARG, "Must specify the embedded name, token and parent process id.");
                }

                *pMode = BURN_MODE_EMBEDDED;

                ++i;

                hr = ParsePipeConnection(argv + i, pEmbeddedConnection);
                ExitOnFailure(hr, "Failed to parse embedded connection.");

                i += 2;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_RELATED_DETECT, -1))
            {
                pCommand->relationType = BOOTSTRAPPER_RELATION_DETECT;

                LogId(REPORT_STANDARD, MSG_BURN_RUN_BY_RELATED_BUNDLE, "Detect");
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_RELATED_UPGRADE, -1))
            {
                pCommand->relationType = BOOTSTRAPPER_RELATION_UPGRADE;

                LogId(REPORT_STANDARD, MSG_BURN_RUN_BY_RELATED_BUNDLE, "Upgrade");
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_RELATED_ADDON, -1))
            {
                pCommand->relationType = BOOTSTRAPPER_RELATION_ADDON;

                LogId(REPORT_STANDARD, MSG_BURN_RUN_BY_RELATED_BUNDLE, "Addon");
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_RELATED_PATCH, -1))
            {
                pCommand->relationType = BOOTSTRAPPER_RELATION_PATCH;

                LogId(REPORT_STANDARD, MSG_BURN_RUN_BY_RELATED_BUNDLE, "Patch");
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_DISABLE_UNELEVATE, -1))
            {
                *pfDisableUnelevate = TRUE;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], -1, BURN_COMMANDLINE_SWITCH_RUNONCE, -1))
            {
                *pMode = BURN_MODE_RUNONCE;
            }
            else if (CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], lstrlenW(BURN_COMMANDLINE_SWITCH_IGNOREDEPENDENCIES), BURN_COMMANDLINE_SWITCH_IGNOREDEPENDENCIES, lstrlenW(BURN_COMMANDLINE_SWITCH_IGNOREDEPENDENCIES)))
            {
                // Get a pointer to the next character after the switch.
                LPCWSTR wzParam = &argv[i][1 + lstrlenW(BURN_COMMANDLINE_SWITCH_IGNOREDEPENDENCIES)];
                if (L'=' != wzParam[0] || L'\0' == wzParam[1])
                {
                    ExitOnRootFailure1(hr = E_INVALIDARG, "Missing required parameter for switch: %ls", BURN_COMMANDLINE_SWITCH_IGNOREDEPENDENCIES);
                }

                hr = StrAllocString(psczIgnoreDependencies, &wzParam[1], 0);
                ExitOnFailure(hr, "Failed to allocate the list of dependencies to ignore.");
            }
            else if (lstrlenW(&argv[i][1]) >= lstrlenW(BURN_COMMANDLINE_SWITCH_PREFIX) &&
                CSTR_EQUAL == ::CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, &argv[i][1], lstrlenW(BURN_COMMANDLINE_SWITCH_PREFIX), BURN_COMMANDLINE_SWITCH_PREFIX, lstrlenW(BURN_COMMANDLINE_SWITCH_PREFIX)))
            {
                // Skip (but log) any other private burn switches we don't recognize, so that
                // adding future private variables doesn't break old bundles 
                LogId(REPORT_STANDARD, MSG_BURN_UNKNOWN_PRIVATE_SWITCH, &argv[i][1]);
            }
            else
            {
                fUnknownArg = TRUE;
            }
        }
        else
        {
            fUnknownArg = TRUE;
        }

        // Remember command-line switch to pass off to UX.
        if (fUnknownArg)
        {
            PathCommandLineAppend(&pCommand->wzCommandLine, argv[i]);
        }
    }

    // Elevation trumps other modes, except RunOnce which is also elevated.
    if (BURN_MODE_RUNONCE != *pMode && (BURN_ELEVATION_STATE_ELEVATED == *pElevationState || BURN_ELEVATION_STATE_ELEVATED_EXPLICITLY == *pElevationState))
    {
        *pMode = BURN_MODE_ELEVATED;
    }
    else if (BURN_MODE_EMBEDDED == *pMode) // if embedded, ensure the display goes embedded as well.
    {
        pCommand->display = BOOTSTRAPPER_DISPLAY_EMBEDDED;
    }

    // Set the defaults if nothing was set above.
    if (BOOTSTRAPPER_ACTION_UNKNOWN == pCommand->action)
    {
        pCommand->action = BOOTSTRAPPER_ACTION_INSTALL;
    }

    if (BOOTSTRAPPER_DISPLAY_UNKNOWN == pCommand->display)
    {
        pCommand->display = BOOTSTRAPPER_DISPLAY_FULL;
    }

    if (BOOTSTRAPPER_RESTART_UNKNOWN == pCommand->restart)
    {
        pCommand->restart = BOOTSTRAPPER_RESTART_PROMPT;
    }

LExit:
    if (argv)
    {
        ::LocalFree(argv);
    }

    return hr;
}

static HRESULT ParsePipeConnection(
    __in_ecount(3) LPWSTR* rgArgs,
    __in BURN_PIPE_CONNECTION* pConnection
    )
{
    HRESULT hr = S_OK;

    hr = StrAllocString(&pConnection->sczName, rgArgs[0], 0);
    ExitOnFailure(hr, "Failed to copy connection name from command line.");

    hr = StrAllocString(&pConnection->sczSecret, rgArgs[1], 0);
    ExitOnFailure(hr, "Failed to copy connection secret from command line.");

    hr = StrStringToUInt32(rgArgs[2], 0, reinterpret_cast<UINT*>(&pConnection->dwProcessId));
    ExitOnFailure(hr, "Failed to copy parent process id from command line.");

LExit:
    return hr;
}

static HRESULT DetectPackagePayloadsCached(
    __in BURN_PACKAGE* pPackage
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczCachePath = NULL;
    BOOL fAllPayloadsCached = FALSE;
    LPWSTR sczPayloadCachePath = NULL;
    LONGLONG llSize = 0;

    if (pPackage->sczCacheId && *pPackage->sczCacheId)
    {
        hr = CacheGetCompletedPath(pPackage->fPerMachine, pPackage->sczCacheId, &sczCachePath);
        ExitOnFailure(hr, "Failed to get completed cache path.");

        fAllPayloadsCached = TRUE; // assume all payloads will be cached.

        for (DWORD i = 0; i < pPackage->cPayloads; ++i)
        {
            BURN_PACKAGE_PAYLOAD* pPackagePayload = pPackage->rgPayloads + i;

            hr = PathConcat(sczCachePath, pPackagePayload->pPayload->sczFilePath, &sczPayloadCachePath);
            ExitOnFailure(hr, "Failed to concat payload cache path.");

            // TODO: should we do a full on hash verification on the file to ensure the exact right
            //       file is cached?
            hr = FileSize(sczPayloadCachePath, &llSize);
            if (SUCCEEDED(hr) && static_cast<DWORD64>(llSize) == pPackagePayload->pPayload->qwFileSize)
            {
                pPackagePayload->fCached = TRUE;
            }
            else
            {
                fAllPayloadsCached = FALSE; // found a payload that was not cached so our assumption above was wrong.
                hr = S_OK;
            }
        }
    }

    pPackage->fCached = fAllPayloadsCached;

LExit:
    ReleaseStr(sczPayloadCachePath);
    ReleaseStr(sczCachePath);
    return hr;
}

static DWORD WINAPI CacheThreadProc(
    __in LPVOID lpThreadParameter
    )
{
    HRESULT hr = S_OK;
    BURN_CACHE_THREAD_CONTEXT* pContext = reinterpret_cast<BURN_CACHE_THREAD_CONTEXT*>(lpThreadParameter);
    BURN_ENGINE_STATE* pEngineState = pContext->pEngineState;
    DWORD* pcOverallProgressTicks = pContext->pcOverallProgressTicks;
    BOOL* pfRollback = pContext->pfRollback;
    BOOL fComInitialized = FALSE;

    // initialize COM
    hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
    ExitOnFailure(hr, "Failed to initialize COM.");
    fComInitialized = TRUE;

    // cache packages
    hr = ApplyCache(&pEngineState->userExperience, &pEngineState->variables, &pEngineState->plan, pEngineState->companionConnection.hCachePipe, pcOverallProgressTicks, pfRollback);
    ExitOnFailure(hr, "Failed to cache packages.");

LExit:
    if (fComInitialized)
    {
        ::CoUninitialize();
    }

    return (DWORD)hr;
}

static HRESULT WaitForCacheThread(
    __in HANDLE hCacheThread
    )
{
    HRESULT hr = S_OK;

    if (WAIT_OBJECT_0 != ::WaitForSingleObject(hCacheThread, INFINITE))
    {
        ExitWithLastError(hr, "Failed to wait for cache thread to terminate.");
    }

    if (!::GetExitCodeThread(hCacheThread, (DWORD*)&hr))
    {
        ExitWithLastError(hr, "Failed to get cache thread exit code.");
    }

LExit:
    return hr;
}

