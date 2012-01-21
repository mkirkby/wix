//-------------------------------------------------------------------------------------------------
// <copyright file="ManifestHelpers.cpp" company="Microsoft">
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
//    Manifest helper functions for unit tests for Burn.
// </summary>
//-------------------------------------------------------------------------------------------------

#include "precomp.h"


using namespace System;
using namespace Microsoft::VisualStudio::TestTools::UnitTesting;


namespace Microsoft
{
namespace Tools
{
namespace WindowsInstallerXml
{
namespace Test
{
namespace Bootstrapper
{
    void LoadBundleXmlHelper(LPCWSTR wzDocument, IXMLDOMElement** ppixeBundle)
    {
        HRESULT hr = S_OK;
        IXMLDOMDocument* pixdDocument = NULL;
        try
        {
            hr = XmlLoadDocument(wzDocument, &pixdDocument);
            TestThrowOnFailure(hr, L"Failed to load XML document.");

            hr = pixdDocument->get_documentElement(ppixeBundle);
            TestThrowOnFailure(hr, L"Failed to get bundle element.");
        }
        finally
        {
            ReleaseObject(pixdDocument);
        }
    }
}
}
}
}
}
