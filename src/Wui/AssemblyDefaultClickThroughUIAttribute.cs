// <copyright file="AssemblyDefaultClickThroughUIAttribute.cs" company="Microsoft Corporation">
//   Copyright (c) 2004, Microsoft Corporation.
//   This software is released under Common Public License Version 1.0 (CPL).
//   The license and further copyright text can be found in the file LICENSE.TXT
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
// 
// <summary>
// Represents a custom attribute for declaring the type to use
// as the default ClickThrough console extension in an assembly.
// </summary>

namespace Microsoft.Tools.WindowsInstallerXml
{
    using System;

    /// <summary>
    /// Represents a custom attribute for declaring the type to use
    /// as the default builder extension in an assembly.
    /// </summary>
    public sealed class AssemblyDefaultClickThroughUIAttribute : Attribute
    {
        private readonly Type extensionType;

        /// <summary>
        /// Instantiate a new AssemblyDefaultBuilderExtensionAttribute.
        /// </summary>
        /// <param name="extensionType">The type of the default builder extension in an assembly.</param>
        public AssemblyDefaultClickThroughUIAttribute(Type extensionType)
        {
            this.extensionType = extensionType;
        }

        /// <summary>
        /// Gets the type of the default builder extension in an assembly.
        /// </summary>
        /// <value>The type of the default builder extension in an assembly.</value>
        public Type ExtensionType
        {
            get { return this.extensionType; }
        }
    }
}
