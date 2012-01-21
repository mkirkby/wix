﻿//-------------------------------------------------------------------------------------------------
// <copyright file="Model.cs" company="Microsoft">
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
// The model.
// </summary>
//-------------------------------------------------------------------------------------------------

namespace Microsoft.Tools.WindowsInstallerXml.UX
{
    using System;
    using System.Diagnostics;
    using System.Net;
    using System.Reflection;
    using Microsoft.Tools.WindowsInstallerXml.Bootstrapper;

    /// <summary>
    /// The model.
    /// </summary>
    public class Model
    {
        private Version version;

        /// <summary>
        /// Creates a new model for the UX.
        /// </summary>
        /// <param name="bootstrapper">Bootstrapper hosting the UX.</param>
        public Model(BootstrapperApplication bootstrapper)
        {
            this.Bootstrapper = bootstrapper;
        }

        /// <summary>
        /// Gets the bootstrapper.
        /// </summary>
        public BootstrapperApplication Bootstrapper { get; private set; }

        /// <summary>
        /// Gets the bootstrapper command-line.
        /// </summary>
        public Command Command { get { return this.Bootstrapper.Command; } }

        /// <summary>
        /// Gets the bootstrapper engine.
        /// </summary>
        public Engine Engine { get { return this.Bootstrapper.Engine; } }

        /// <summary>
        /// Get the version of the install.
        /// </summary>
        public Version Version
        {
            get
            {
                if (null == this.version)
                {
                    Assembly assembly = Assembly.GetExecutingAssembly();
                    FileVersionInfo fileVersion = FileVersionInfo.GetVersionInfo(assembly.Location);

                    this.version = new Version(fileVersion.FileVersion);
                }

                return this.version;
            }
        }

        /// <summary>
        /// Creates a correctly configured HTTP web request.
        /// </summary>
        /// <param name="uri">URI to connect to.</param>
        /// <returns>Correctly configured HTTP web request.</returns>
        public HttpWebRequest CreateWebRequest(string uri)
        {
            HttpWebRequest request = (HttpWebRequest)WebRequest.Create(uri);
            request.UserAgent = String.Concat("WixInstall", this.Version.ToString());

            return request;
        }
    }
}
