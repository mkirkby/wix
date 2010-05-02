//-------------------------------------------------------------------------------------------------
// <copyright file="VSCompiler.cs" company="Microsoft">
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
// The compiler for the Windows Installer XML Toolset Visual Studio Extension.
// </summary>
//-------------------------------------------------------------------------------------------------

namespace Microsoft.Tools.WindowsInstallerXml.Extensions
{
    using System;
    using System.Diagnostics;
    using System.IO;
    using System.Reflection;
    using System.Xml;
    using System.Xml.Schema;
    using Microsoft.Tools.WindowsInstallerXml;

    /// <summary>
    /// The compiler for the Windows Installer XML Toolset Visual Studio Extension.
    /// </summary>
    public sealed class VSCompiler : CompilerExtension
    {
        private XmlSchema schema;

        /// <summary>
        /// Instantiate a new HelpCompiler.
        /// </summary>
        public VSCompiler()
        {
            this.schema = LoadXmlSchemaHelper(Assembly.GetExecutingAssembly(), "Microsoft.Tools.WindowsInstallerXml.Extensions.Xsd.vs.xsd");
        }

        /// <summary>
        /// Gets the schema for this extension.
        /// </summary>
        /// <value>Schema for this extension.</value>
        public override XmlSchema Schema
        {
            get { return this.schema; }
        }

        /// <summary>
        /// Processes an element for the Compiler.
        /// </summary>
        /// <param name="sourceLineNumbers">Source line number for the parent element.</param>
        /// <param name="parentElement">Parent element of element to process.</param>
        /// <param name="element">Element to process.</param>
        /// <param name="contextValues">Extra information about the context in which this element is being parsed.</param>
        public override void ParseElement(SourceLineNumberCollection sourceLineNumbers, XmlElement parentElement, XmlElement element, params string[] contextValues)
        {
            switch (parentElement.LocalName)
            {
                case "File":
                    switch (element.LocalName)
                    {
                        case "HelpCollection":
                            this.ParseHelpCollectionElement(element, contextValues[0]);
                            break;
                        case "HelpFile":
                            this.ParseHelpFileElement(element, contextValues[0]);
                            break;
                        default:
                            base.ParseElement(sourceLineNumbers, parentElement, element, contextValues);
                            break;
                    }
                    break;
                case "Fragment":
                case "Module":
                case "Product":
                    switch (element.LocalName)
                    {
                        case "HelpCollectionRef":
                            this.ParseHelpCollectionRefElement(element);
                            break;
                        case "HelpFilter":
                            this.ParseHelpFilterElement(element);
                            break;
                        default:
                            base.ParseElement(sourceLineNumbers, parentElement, element, contextValues);
                            break;
                    }
                    break;
                default:
                    base.ParseElement(sourceLineNumbers, parentElement, element, contextValues);
                    break;
            }
        }

        /// <summary>
        /// Parses a HelpCollectionRef element.
        /// </summary>
        /// <param name="node">Element to process.</param>
        private void ParseHelpCollectionRefElement(XmlNode node)
        {
            SourceLineNumberCollection sourceLineNumbers = Preprocessor.GetSourceLineNumbers(node);
            string id = null;

            foreach (XmlAttribute attrib in node.Attributes)
            {
                if (0 == attrib.NamespaceURI.Length || attrib.NamespaceURI == this.schema.TargetNamespace)
                {
                    switch (attrib.LocalName)
                    {
                        case "Id":
                            id = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "HelpNamespace", id);
                            break;
                        default:
                            this.Core.UnexpectedAttribute(sourceLineNumbers, attrib);
                            break;
                    }
                }
                else
                {
                    this.Core.ParseExtensionAttribute(sourceLineNumbers, (XmlElement)node, attrib);
                }
            }

            if (null == id)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Id"));
            }

            foreach (XmlNode child in node.ChildNodes)
            {
                if (XmlNodeType.Element == child.NodeType)
                {
                    if (child.NamespaceURI == this.schema.TargetNamespace)
                    {
                        switch (child.LocalName)
                        {
                            case "HelpFileRef":
                                this.ParseHelpFileRefElement(child, id);
                                break;
                            default:
                                this.Core.UnexpectedElement(node, child);
                                break;
                        }
                    }
                    else
                    {
                        this.Core.UnsupportedExtensionElement(node, child);
                    }
                }
            }
        }

        /// <summary>
        /// Parses a HelpCollection element.
        /// </summary>
        /// <param name="node">Element to process.</param>
        /// <param name="fileId">Identifier of the parent File element.</param>
        private void ParseHelpCollectionElement(XmlNode node, string fileId)
        {
            SourceLineNumberCollection sourceLineNumbers = Preprocessor.GetSourceLineNumbers(node);
            string id = null;
            string description = null;
            string name = null;
            YesNoType suppressCAs = YesNoType.No;

            foreach (XmlAttribute attrib in node.Attributes)
            {
                if (0 == attrib.NamespaceURI.Length || attrib.NamespaceURI == this.schema.TargetNamespace)
                {
                    switch (attrib.LocalName)
                    {
                        case "Id":
                            id = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            break;
                        case "Description":
                            description = this.Core.GetAttributeValue(sourceLineNumbers, attrib);
                            break;
                        case "Name":
                            name = this.Core.GetAttributeValue(sourceLineNumbers, attrib);
                            break;
                        case "SuppressCustomActions":
                            suppressCAs = this.Core.GetAttributeYesNoValue(sourceLineNumbers, attrib);
                            break;
                        default:
                            this.Core.UnexpectedAttribute(sourceLineNumbers, attrib);
                            break;
                    }
                }
                else
                {
                    this.Core.UnsupportedExtensionAttribute(sourceLineNumbers, attrib);
                }
            }

            if (null == id)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Id"));
            }

            if (null == description)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Description"));
            }

            if (null == name)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Name"));
            }

            foreach (XmlNode child in node.ChildNodes)
            {
                if (XmlNodeType.Element == child.NodeType)
                {
                    if (child.NamespaceURI == this.schema.TargetNamespace)
                    {
                        switch (child.LocalName)
                        {
                            case "HelpFileRef":
                                this.ParseHelpFileRefElement(child, id);
                                break;
                            case "HelpFilterRef":
                                this.ParseHelpFilterRefElement(child, id);
                                break;
                            case "PlugCollectionInto":
                                this.ParsePlugCollectionIntoElement(child, id);
                                break;
                            default:
                                this.Core.UnexpectedElement(node, child);
                                break;
                        }
                    }
                    else
                    {
                        this.Core.UnsupportedExtensionElement(node, child);
                    }
                }
            }

            if (!this.Core.EncounteredError)
            {
                Row row = this.Core.CreateRow(sourceLineNumbers, "HelpNamespace");
                row[0] = id;
                row[1] = name;
                row[2] = fileId;
                row[3] = description;

                if (YesNoType.No == suppressCAs)
                {
                    this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "CustomAction", "CA_RegisterMicrosoftHelp.3643236F_FC70_11D3_A536_0090278A1BB8");
                }
            }
        }

        /// <summary>
        /// Parses a HelpFile element.
        /// </summary>
        /// <param name="node">Element to process.</param>
        /// <param name="fileId">Identifier of the parent file element.</param>
        private void ParseHelpFileElement(XmlNode node, string fileId)
        {
            SourceLineNumberCollection sourceLineNumbers = Preprocessor.GetSourceLineNumbers(node);
            string id = null;
            string name = null;
            int language = CompilerCore.IntegerNotSet;
            string hxi = null;
            string hxq = null;
            string hxr = null;
            string samples = null;
            YesNoType suppressCAs = YesNoType.No;

            foreach (XmlAttribute attrib in node.Attributes)
            {
                if (0 == attrib.NamespaceURI.Length || attrib.NamespaceURI == this.schema.TargetNamespace)
                {
                    switch (attrib.LocalName)
                    {
                        case "Id":
                            id = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            break;
                        case "AttributeIndex":
                            hxr = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "File", hxr);
                            break;
                        case "Index":
                            hxi = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "File", hxi);
                            break;
                        case "Language":
                            language = this.Core.GetAttributeIntegerValue(sourceLineNumbers, attrib, 0, short.MaxValue);
                            break;
                        case "Name":
                            name = this.Core.GetAttributeValue(sourceLineNumbers, attrib);
                            break;
                        case "SampleLocation":
                            samples = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "File", samples);
                            break;
                        case "Search":
                            hxq = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "File", hxq);
                            break;
                        case "SuppressCustomActions":
                            suppressCAs = this.Core.GetAttributeYesNoValue(sourceLineNumbers, attrib);
                            break;
                        default:
                            this.Core.UnexpectedAttribute(sourceLineNumbers, attrib);
                            break;
                    }
                }
                else
                {
                    this.Core.UnsupportedExtensionAttribute(sourceLineNumbers, attrib);
                }
            }

            if (null == id)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Id"));
            }

            if (null == name)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Name"));
            }

            //uninstall will always fail silently, leaving file registered, if Language is not set
            if (CompilerCore.IntegerNotSet == language)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Language"));
            }

            // find unexpected child elements
            foreach (XmlNode child in node.ChildNodes)
            {
                if (XmlNodeType.Element == child.NodeType)
                {
                    if (child.NamespaceURI == this.schema.TargetNamespace)
                    {
                        this.Core.UnexpectedElement(node, child);
                    }
                    else
                    {
                        this.Core.UnsupportedExtensionElement(node, child);
                    }
                }
            }

            if (!this.Core.EncounteredError)
            {
                Row row = this.Core.CreateRow(sourceLineNumbers, "HelpFile");
                row[0] = id;
                row[1] = name;
                row[2] = language;
                row[3] = fileId;
                row[4] = hxi;
                row[5] = hxq;
                row[6] = hxr;
                row[7] = samples;

                if (YesNoType.No == suppressCAs)
                {
                    this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "CustomAction", "CA_RegisterMicrosoftHelp.3643236F_FC70_11D3_A536_0090278A1BB8");
                }
            }
        }

        /// <summary>
        /// Parses a HelpFileRef element.
        /// </summary>
        /// <param name="node">Element to process.</param>
        /// <param name="collectionId">Identifier of the parent help collection.</param>
        private void ParseHelpFileRefElement(XmlNode node, string collectionId)
        {
            SourceLineNumberCollection sourceLineNumbers = Preprocessor.GetSourceLineNumbers(node);
            string id = null;

            foreach (XmlAttribute attrib in node.Attributes)
            {
                if (0 == attrib.NamespaceURI.Length || attrib.NamespaceURI == this.schema.TargetNamespace)
                {
                    switch (attrib.LocalName)
                    {
                        case "Id":
                            id = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "HelpFile", id);
                            break;
                        default:
                            this.Core.UnexpectedAttribute(sourceLineNumbers, attrib);
                            break;
                    }
                }
                else
                {
                    this.Core.ParseExtensionAttribute(sourceLineNumbers, (XmlElement)node, attrib);
                }
            }

            if (null == id)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Id"));
            }

            // find unexpected child elements
            foreach (XmlNode child in node.ChildNodes)
            {
                if (XmlNodeType.Element == child.NodeType)
                {
                    if (child.NamespaceURI == this.schema.TargetNamespace)
                    {
                        this.Core.UnexpectedElement(node, child);
                    }
                    else
                    {
                        this.Core.UnsupportedExtensionElement(node, child);
                    }
                }
            }

            if (!this.Core.EncounteredError)
            {
                Row row = this.Core.CreateRow(sourceLineNumbers, "HelpFileToNamespace");
                row[0] = id;
                row[1] = collectionId;
            }
        }

        /// <summary>
        /// Parses a HelpFilter element.
        /// </summary>
        /// <param name="node">Element to process.</param>
        private void ParseHelpFilterElement(XmlNode node)
        {
            SourceLineNumberCollection sourceLineNumbers = Preprocessor.GetSourceLineNumbers(node);
            string id = null;
            string filterDefinition = null;
            string name = null;
            YesNoType suppressCAs = YesNoType.No;

            foreach (XmlAttribute attrib in node.Attributes)
            {
                if (0 == attrib.NamespaceURI.Length || attrib.NamespaceURI == this.schema.TargetNamespace)
                {
                    switch (attrib.LocalName)
                    {
                        case "Id":
                            id = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            break;
                        case "FilterDefinition":
                            filterDefinition = this.Core.GetAttributeValue(sourceLineNumbers, attrib);
                            break;
                        case "Name":
                            name = this.Core.GetAttributeValue(sourceLineNumbers, attrib);
                            break;
                        case "SuppressCustomActions":
                            suppressCAs = this.Core.GetAttributeYesNoValue(sourceLineNumbers, attrib);
                            break;
                        default:
                            this.Core.UnexpectedAttribute(sourceLineNumbers, attrib);
                            break;
                    }
                }
                else
                {
                    this.Core.UnsupportedExtensionAttribute(sourceLineNumbers, attrib);
                }
            }

            if (null == id)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Id"));
            }

            if (null == name)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Name"));
            }

            // find unexpected child elements
            foreach (XmlNode child in node.ChildNodes)
            {
                if (XmlNodeType.Element == child.NodeType)
                {
                    if (child.NamespaceURI == this.schema.TargetNamespace)
                    {
                        this.Core.UnexpectedElement(node, child);
                    }
                    else
                    {
                        this.Core.UnsupportedExtensionElement(node, child);
                    }
                }
            }

            if (!this.Core.EncounteredError)
            {
                Row row = this.Core.CreateRow(sourceLineNumbers, "HelpFilter");
                row[0] = id;
                row[1] = name;
                row[2] = filterDefinition;

                if (YesNoType.No == suppressCAs)
                {
                    this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "CustomAction", "CA_RegisterMicrosoftHelp.3643236F_FC70_11D3_A536_0090278A1BB8");
                }
            }
        }

        /// <summary>
        /// Parses a HelpFilterRef element.
        /// </summary>
        /// <param name="node">Element to process.</param>
        /// <param name="collectionId">Identifier of the parent help collection.</param>
        private void ParseHelpFilterRefElement(XmlNode node, string collectionId)
        {
            SourceLineNumberCollection sourceLineNumbers = Preprocessor.GetSourceLineNumbers(node);
            string id = null;

            foreach (XmlAttribute attrib in node.Attributes)
            {
                if (0 == attrib.NamespaceURI.Length || attrib.NamespaceURI == this.schema.TargetNamespace)
                {
                    switch (attrib.LocalName)
                    {
                        case "Id":
                            id = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "HelpFilter", id);
                            break;
                        default:
                            this.Core.UnexpectedAttribute(sourceLineNumbers, attrib);
                            break;
                    }
                }
                else
                {
                    this.Core.ParseExtensionAttribute(sourceLineNumbers, (XmlElement)node, attrib);
                }
            }

            if (null == id)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "Id"));
            }

            // find unexpected child elements
            foreach (XmlNode child in node.ChildNodes)
            {
                if (XmlNodeType.Element == child.NodeType)
                {
                    if (child.NamespaceURI == this.schema.TargetNamespace)
                    {
                        this.Core.UnexpectedElement(node, child);
                    }
                    else
                    {
                        this.Core.UnsupportedExtensionElement(node, child);
                    }
                }
            }

            if (!this.Core.EncounteredError)
            {
                Row row = this.Core.CreateRow(sourceLineNumbers, "HelpFilterToNamespace");
                row[0] = id;
                row[1] = collectionId;
            }
        }

        /// <summary>
        /// Parses a PlugCollectionInto element.
        /// </summary>
        /// <param name="node">Element to process.</param>
        /// <param name="parentId">Identifier of the parent help collection.</param>
        private void ParsePlugCollectionIntoElement(XmlNode node, string parentId)
        {
            SourceLineNumberCollection sourceLineNumbers = Preprocessor.GetSourceLineNumbers(node);
            string hxa = null;
            string hxt = null;
            string hxtParent = null;
            string namespaceParent = null;
            string feature = null;
            YesNoType suppressExternalNamespaces = YesNoType.No;
            bool pluginVS05 = false;
            bool pluginVS08 = false;

            foreach (XmlAttribute attrib in node.Attributes)
            {
                if (0 == attrib.NamespaceURI.Length || attrib.NamespaceURI == this.schema.TargetNamespace)
                {
                    switch (attrib.LocalName)
                    {
                        case "Attributes":
                            hxa = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            break;
                        case "TableOfContents":
                            hxt = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            break;
                        case "TargetCollection":
                            namespaceParent = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            break;
                        case "TargetTableOfContents":
                            hxtParent = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            break;
                        case "TargetFeature":
                            feature = this.Core.GetAttributeIdentifierValue(sourceLineNumbers, attrib);
                            break;
                        case "SuppressExternalNamespaces":
                            suppressExternalNamespaces = this.Core.GetAttributeYesNoValue(sourceLineNumbers, attrib);
                            break;
                        default:
                            this.Core.UnexpectedAttribute(sourceLineNumbers, attrib);
                            break;
                    }
                }
                else
                {
                    this.Core.UnsupportedExtensionAttribute(sourceLineNumbers, attrib);
                }
            }

            pluginVS05 = namespaceParent.Equals("MS_VSIPCC_v80", StringComparison.Ordinal);
            pluginVS08 = namespaceParent.Equals("MS.VSIPCC.v90", StringComparison.Ordinal);

            if (null == namespaceParent)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "TargetCollection"));
            }

            if (null == feature && (pluginVS05 || pluginVS08) && YesNoType.No == suppressExternalNamespaces)
            {
                this.Core.OnMessage(WixErrors.ExpectedAttribute(sourceLineNumbers, node.Name, "TargetFeature"));
            }

            // find unexpected child elements
            foreach (XmlNode child in node.ChildNodes)
            {
                if (XmlNodeType.Element == child.NodeType)
                {
                    if (child.NamespaceURI == this.schema.TargetNamespace)
                    {
                        this.Core.UnexpectedElement(node, child);
                    }
                    else
                    {
                        this.Core.UnsupportedExtensionElement(node, child);
                    }
                }
            }

            if (!this.Core.EncounteredError)
            {
                Row row = this.Core.CreateRow(sourceLineNumbers, "HelpPlugin");
                row[0] = parentId;
                row[1] = namespaceParent;
                row[2] = hxt;
                row[3] = hxa;
                row[4] = hxtParent;

                if (pluginVS05)
                {
                    if (YesNoType.No == suppressExternalNamespaces)
                    {
                        // Bring in the help 2 base namespace components for VS 2005
                        this.Core.CreateComplexReference(sourceLineNumbers, ComplexReferenceParentType.Feature, feature, String.Empty,
                            ComplexReferenceChildType.ComponentGroup, "Help2_VS2005_Namespace_Components", false);
                        // Reference CustomAction since nothing will happen without it
                        this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "CustomAction",
                            "CA_HxMerge_VSIPCC_VSCC");
                    }
                }
                else if (pluginVS08)
                {
                    if (YesNoType.No == suppressExternalNamespaces)
                    {
                        // Bring in the help 2 base namespace components for VS 2008
                        this.Core.CreateComplexReference(sourceLineNumbers, ComplexReferenceParentType.Feature, feature, String.Empty,
                            ComplexReferenceChildType.ComponentGroup, "Help2_VS2008_Namespace_Components", false);
                        // Reference CustomAction since nothing will happen without it
                        this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "CustomAction",
                            "CA_ScheduleExtHelpPlugin_VSCC_VSIPCC");
                    }
                }
                else
                {
                    // Reference the parent namespace to enforce the foreign key relationship
                    this.Core.CreateWixSimpleReferenceRow(sourceLineNumbers, "HelpNamespace",
                        namespaceParent);
                }
            }
        }
    }
}
