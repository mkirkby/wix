﻿//-------------------------------------------------------------------------------------------------
// <copyright file="AssemblyInfo.cs" company="Microsoft">
// Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
// 
// <summary>
// Assembly information.
// </summary>
//-------------------------------------------------------------------------------------------------

using System;
using System.Reflection;
using System.Runtime.InteropServices;
using Microsoft.Tools.WindowsInstallerXml.Bootstrapper;
using Microsoft.Tools.WindowsInstallerXml.UX;


[assembly: AssemblyTitle("WixBA")]
[assembly: AssemblyDescription("WiX User Experience")]

// Setting ComVisible to false makes the types in this assembly not visible 
// to COM components.  If you need to access a type in this assembly from 
// COM, set the ComVisible attribute to true on that type.
[assembly: ComVisible(false)]
[assembly: Guid("0ffc4944-9295-40b7-adac-3a6864b5219b")]
[assembly: CLSCompliantAttribute(true)]

// Identifies the class that derives from UserExperience and is the UX class that gets
// instantiated by the interop layer
[assembly: BootstrapperApplication(typeof(WixBA))]