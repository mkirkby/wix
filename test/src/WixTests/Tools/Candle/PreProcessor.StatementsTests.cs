//-----------------------------------------------------------------------
// <copyright file="PreProcessor.StatementsTests.cs" company="Outercurve Foundation">
//   Copyright (c) 2004, Outercurve Foundation.
//   This software is released under Microsoft Reciprocal License (MS-RL).
//   The license and further copyright text can be found in the file
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
// <summary>Test how Candle handles preprocessing for statements.</summary>
//-----------------------------------------------------------------------

namespace WixTest.Tests.Tools.Candle.PreProcessor
{
    using System;
    using System.IO;
    using System.Xml;
    using WixTest;
    using Microsoft.VisualStudio.TestTools.UnitTesting;

    /// <summary>
    /// Test how Candle handles preprocessing for statements.
    /// </summary>
    [TestClass]
    public class StatementTests : WixTests
    {
        private static readonly string TestDataDirectory = @"%WIX_ROOT%\test\data\Tools\Candle\PreProcessor\StatementTests";

        [TestMethod]
        [Description("Verify that Candle can preprocess an if statement.")]
        [Priority(1)]
        public void If()
        {
            string testFile = Path.Combine(StatementTests.TestDataDirectory, @"SharedData\Product.wxs");

            Candle candle = new Candle();
            candle.SourceFiles.Add(testFile);
            candle.PreProcessorParams.Add("MyVariable", "1");
            candle.Run();

            Verifier.VerifyWixObjProperty(candle.ExpectedOutputFiles[0], "MyProperty1", "foo");
        }

        [TestMethod]
        [Description("Verify that Candle can preprocess elseif statement.")]
        [Priority(2)]
        public void ElseIf()
        {
            string testFile = Path.Combine(StatementTests.TestDataDirectory, @"SharedData\Product.wxs");

            Candle candle = new Candle();
            candle.SourceFiles.Add(testFile);
            candle.PreProcessorParams.Add("MyVariable", "2");
            candle.Run();

            Verifier.VerifyWixObjProperty(candle.ExpectedOutputFiles[0], "MyProperty2", "bar");
        }

        [TestMethod]
        [Description("Verify that Candle can preprocess else statement.")]
        [Priority(2)]
        public void Else()
        {
            string testFile = Path.Combine(StatementTests.TestDataDirectory, @"SharedData\Product.wxs");

            Candle candle = new Candle();
            candle.SourceFiles.Add(testFile);
            candle.PreProcessorParams.Add("MyVariable", "3");
            candle.Run();

            Verifier.VerifyWixObjProperty(candle.ExpectedOutputFiles[0], "MyProperty3", "baz");
        }

        [TestMethod]
        [Description("Verify that Candle can preprocess ifdef statement.")]
        [Priority(2)]
        public void IfDef()
        {
            string testFile = Path.Combine(StatementTests.TestDataDirectory, @"IfDef\Product.wxs");

            Candle candle = new Candle();
            candle.SourceFiles.Add(testFile);
            candle.PreProcessorParams.Add("MyVariable", "10");
            candle.Run();

            Verifier.VerifyWixObjProperty(candle.ExpectedOutputFiles[0], "MyProperty", "foo");
        }

        [TestMethod]
        [Description("Verify that Candle can preprocess ifndef statement.")]
        [Priority(2)]
        public void IfNDef()
        {
            string testFile = Path.Combine(StatementTests.TestDataDirectory, @"IfNDef\Product.wxs");

            string outputFile = Candle.Compile(testFile);

            Verifier.VerifyWixObjProperty(outputFile, "MyProperty", "bar");
        }

        [TestMethod]
        [Description("Verify that Candle can preprocess foreach statement.")]
        [Priority(2)]
        public void ForEach()
        {
            string testFile = Path.Combine(StatementTests.TestDataDirectory, @"ForEach\Product.wxs");

            string outputFile = Candle.Compile(testFile);

            for (int i = 1; i < 4; i++)
            {
                string expectedPropertyID = String.Concat("MyProperty", Convert.ToString(i));
                Verifier.VerifyWixObjProperty(outputFile, expectedPropertyID, Convert.ToString(i));
            }
        }
    }
}