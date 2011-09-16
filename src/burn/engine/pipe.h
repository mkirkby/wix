//-------------------------------------------------------------------------------------------------
// <copyright file="pipe.h" company="Microsoft">
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
//    Burn Client Server pipe communication handler.
// </summary>
//-------------------------------------------------------------------------------------------------

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _BURN_PIPE_CONNECTION
{
    LPWSTR sczName;
    LPWSTR sczSecret;
    DWORD dwProcessId;

    HANDLE hProcess;
    HANDLE hPipe;
    HANDLE hCachePipe;
} BURN_PIPE_CONNECTION;

typedef enum _BURN_PIPE_MESSAGE_TYPE
{
    BURN_PIPE_MESSAGE_TYPE_LOG = 0xF0000001,
    BURN_PIPE_MESSAGE_TYPE_COMPLETE = 0xF0000002,
    BURN_PIPE_MESSAGE_TYPE_TERMINATE = 0xF0000003,
} BURN_PIPE_MESSAGE_TYPE;

typedef struct _BURN_PIPE_MESSAGE
{
    DWORD dwMessage;
    DWORD cbData;

    BOOL fAllocatedData;
    LPVOID pvData;
} BURN_PIPE_MESSAGE;

typedef HRESULT (*PFN_PIPE_MESSAGE_CALLBACK)(
    __in BURN_PIPE_MESSAGE* pMsg,
    __in_opt LPVOID pvContext,
    __out DWORD* pdwResult
    );


// Common functions.
void PipeConnectionInitialize(
    __in BURN_PIPE_CONNECTION* pConnection
    );
void PipeConnectionUninitialize(
    __in BURN_PIPE_CONNECTION* pConnection
    );
HRESULT PipeSendMessage(
    __in HANDLE hPipe,
    __in DWORD dwMessage,
    __in_bcount_opt(cbData) LPVOID pvData,
    __in DWORD cbData,
    __in_opt PFN_PIPE_MESSAGE_CALLBACK pfnCallback,
    __in_opt LPVOID pvContext,
    __out DWORD* pdwResult
    );
HRESULT PipePumpMessages(
    __in HANDLE hPipe,
    __in_opt PFN_PIPE_MESSAGE_CALLBACK pfnCallback,
    __in_opt LPVOID pvContext,
    __out DWORD* pdwResult
    );

// Parent functions.
HRESULT PipeCreateNameAndSecret(
    __out_z LPWSTR *psczConnectionName,
    __out_z LPWSTR *psczSecret
    );
HRESULT PipeCreatePipes(
    __in BURN_PIPE_CONNECTION* pConnection,
    __in BOOL fCreateCachePipe,
    __out HANDLE* phEvent
    );
HRESULT PipeLaunchParentProcess(
    __in LPCWSTR wzCommandLine,
    __in int nCmdShow,
    __in_z LPWSTR sczPipeName,
    __in_z LPWSTR sczClientToken
    );
HRESULT PipeLaunchChildProcess(
    __in_z LPCWSTR wzExecutablePath,
    __in BURN_PIPE_CONNECTION* pConnection,
    __in BOOL fElevate,
    __in_opt HWND hwndParent
    );
HRESULT PipeWaitForChildConnect(
    __in BURN_PIPE_CONNECTION* pConnection
    );
HRESULT PipeTerminateChildProcess(
    __in BURN_PIPE_CONNECTION* pConnection,
    __in DWORD dwParentExitCode
    );

// Child functions.
HRESULT PipeChildConnect(
    __in BURN_PIPE_CONNECTION* pConnection,
    __in BOOL fConnectCachePipe
    );

#ifdef __cplusplus
}
#endif
