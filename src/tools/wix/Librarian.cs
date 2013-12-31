//-------------------------------------------------------------------------------------------------
// <copyright file="Librarian.cs" company="Outercurve Foundation">
//   Copyright (c) 2004, Outercurve Foundation.
//   This software is released under Microsoft Reciprocal License (MS-RL).
//   The license and further copyright text can be found in the file
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
// 
// <summary>
// Core librarian tool.
// </summary>
//-------------------------------------------------------------------------------------------------
namespace WixToolset
{
    using System;
    using System.Collections;
    using System.Collections.Specialized;
    using WixToolset.Data;
    using WixToolset.Extensibility;
    using WixToolset.Msi;

    /// <summary>
    /// Core librarian tool.
    /// </summary>
    public sealed class Librarian : IMessageHandler
    {
        private TableDefinitionCollection tableDefinitions;

        /// <summary>
        /// Instantiate a new Librarian class.
        /// </summary>
        public Librarian()
        {
            this.tableDefinitions = WindowsInstallerStandard.GetTableDefinitions();
        }

        /// <summary>
        /// Gets table definitions used by this librarian.
        /// </summary>
        /// <value>Table definitions.</value>
        public TableDefinitionCollection TableDefinitions
        {
            get { return this.tableDefinitions; }
        }

        /// <summary>
        /// Adds an extension.
        /// </summary>
        /// <param name="extension">The extension to add.</param>
        public void AddExtensionData(IExtensionData extension)
        {
            if (null != extension.TableDefinitions)
            {
                foreach (TableDefinition tableDefinition in extension.TableDefinitions)
                {
                    if (!this.tableDefinitions.Contains(tableDefinition.Name))
                    {
                        this.tableDefinitions.Add(tableDefinition);
                    }
                    else
                    {
                        Messaging.Instance.OnMessage(WixErrors.DuplicateExtensionTable(extension.GetType().ToString(), tableDefinition.Name));
                    }
                }
            }
        }

        /// <summary>
        /// Create a library by combining several intermediates (objects).
        /// </summary>
        /// <param name="sections">The sections to combine into a library.</param>
        /// <returns>Returns the new library.</returns>
        public Library Combine(SectionCollection sections)
        {
            Library library = new Library();

            library.Sections.AddRange(sections);

            // check for multiple entry sections and duplicate symbols
            this.Validate(library);

            return (Messaging.Instance.EncounteredError ? null : library);
        }

        /// <summary>
        /// Sends a message to the message delegate if there is one.
        /// </summary>
        /// <param name="mea">Message event arguments.</param>
        public void OnMessage(MessageEventArgs e)
        {
            Messaging.Instance.OnMessage(e);
        }

        /// <summary>
        /// Validate that a library contains one entry section and no duplicate symbols.
        /// </summary>
        /// <param name="library">Library to validate.</param>
        private void Validate(Library library)
        {
            Section entrySection;
            SymbolCollection allSymbols;

            library.Sections.FindEntrySectionAndLoadSymbols(false, this, OutputType.Unknown, out entrySection, out allSymbols);

            foreach (Section section in library.Sections)
            {
                section.ResolveReferences(OutputType.Unknown, allSymbols, null, null, this);
            }
        }
    }
}
