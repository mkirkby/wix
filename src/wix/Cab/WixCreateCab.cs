//-------------------------------------------------------------------------------------------------
// <copyright file="WixCreateCab.cs" company="Microsoft">
//    Copyright (c) Microsoft Corporation.  All rights reserved.
//    
//    The use and distribution terms for this software are covered by the
//    Common Public License 1.0 (http://opensource.org/licenses/cpl.php)
//    which can be found in the file CPL.TXT at the root of this distribution.
//    By using this software in any fashion, you are agreeing to be bound by
//    the terms of this license.
//    
//    You must not remove this notice, or any other, from this software.
// </copyright>
// 
// <summary>
// Wrapper class around interop with wixcab.dll to compress files into a cabinet.
// </summary>
//-------------------------------------------------------------------------------------------------

namespace Microsoft.Tools.WindowsInstallerXml.Cab
{
    using System;
    using System.IO;
    using System.Runtime.InteropServices;
    using Microsoft.Tools.WindowsInstallerXml.Cab.Interop;

    /// <summary>
    /// Compression level to use when creating cabinet.
    /// </summary>
    public enum CompressionLevel
    {
        /// <summary>Use no compression.</summary>
        None,

        /// <summary>Use low compression.</summary>
        Low,

        /// <summary>Use medium compression.</summary>
        Medium,

        /// <summary>Use high compression.</summary>
        High,

        /// <summary>Use ms-zip compression.</summary>
        Mszip
    }

    /// <summary>
    /// Wrapper class around interop with wixcab.dll to compress files into a cabinet.
    /// </summary>
    public sealed class WixCreateCab : IDisposable
    {
        private IntPtr handle = IntPtr.Zero;
        private bool disposed;

        /// <summary>
        /// Creates a cabinet.
        /// </summary>
        /// <param name="cabName">Name of cabinet to create.</param>
        /// <param name="cabDir">Directory to create cabinet in.</param>
        /// <param name="maxSize">Maximum size of cabinet.</param>
        /// <param name="maxThresh">Maximum threshold for each cabinet.</param>
        /// <param name="compressionLevel">Level of compression to apply.</param>
        public WixCreateCab(string cabName, string cabDir, int maxSize, int maxThresh, CompressionLevel compressionLevel)
        {
            if (String.IsNullOrEmpty(cabDir))
            {
                cabDir = Directory.GetCurrentDirectory();
            }

            try
            {
                NativeMethods.CreateCabBegin(cabName, cabDir, (uint)maxSize, (uint)maxThresh, (uint)compressionLevel, out this.handle);
            }
            catch (COMException ce)
            {
                // If we get a "the file exists" error, we must have a full temp directory - so report the issue
                if (0x80070050 == unchecked((uint)ce.ErrorCode))
                {
                    throw new WixException(WixErrors.FullTempDirectory("WSC", Path.GetTempPath()));
                }

                throw;
            }
        }

        /// <summary>
        /// Destructor for cabinet creation.
        /// </summary>
        ~WixCreateCab()
        {
            this.Dispose();
        }

        /// <summary>
        /// Adds a file to the cabinet.
        /// </summary>
        /// <param name="file">The file to add.</param>
        /// <param name="token">The token for the file.</param>
        public void AddFile(string file, string token)
        {
            try
            {
                NativeMethods.CreateCabAddFile(file, token, this.handle);
            }
            catch (COMException ce)
            {
                if (0x80004005 == unchecked((uint)ce.ErrorCode)) // E_FAIL
                {
                    throw new WixException(WixErrors.CreateCabAddFileFailed());
                }
                else if (0x80070070 == unchecked((uint)ce.ErrorCode)) // ERROR_DISK_FULL
                {
                    throw new WixException(WixErrors.CreateCabInsufficientDiskSpace());
                }
                else
                {
                    throw;
                }
            }
            catch (DirectoryNotFoundException)
            {
                throw new WixFileNotFoundException(file);
            }
            catch (FileNotFoundException)
            {
                throw new WixFileNotFoundException(file);
            }
        }

        /// <summary>
        /// Complete/commit the cabinet - this must be called before Dispose so that errors will be 
        /// reported on the same thread.
        /// </summary>
        public void Complete()
        {
            if (IntPtr.Zero != this.handle)
            {
                try
                {
                    NativeMethods.CreateCabFinish(this.handle);
                    GC.SuppressFinalize(this);
                    this.disposed = true;
                }
                catch (COMException ce)
                {
                    if (0x80004005 == unchecked((uint)ce.ErrorCode)) // E_FAIL
                    {
                        // This error seems to happen, among other situations, when cabbing more than 0xFFFF files
                        throw new WixException(WixErrors.FinishCabFailed());
                    }
                    else if (0x80070070 == unchecked((uint)ce.ErrorCode)) // ERROR_DISK_FULL
                    {
                        throw new WixException(WixErrors.CreateCabInsufficientDiskSpace());
                    }
                    else
                    {
                        throw;
                    }
                }
                finally
                {
                    this.handle = IntPtr.Zero;
                }
            }
        }

        /// <summary>
        /// Cancels ("rolls back") the creation of the cabinet.
        /// Don't throw WiX errors from here, because we're in a different thread, and they won't be reported correctly.
        /// </summary>
        public void Dispose()
        {
            if (!this.disposed)
            {
                if (IntPtr.Zero != this.handle)
                {
                    NativeMethods.CreateCabCancel(this.handle);
                    this.handle = IntPtr.Zero;
                }

                GC.SuppressFinalize(this);
                this.disposed = true;
            }
        }
    }
}
