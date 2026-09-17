using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

namespace MsgViewer.Tests
{
    class TestRunner
    {
        static int Main(string[] args)
        {
            Console.WriteLine("========================================");
            Console.WriteLine("    MsgViewer Automated Test Suite      ");
            Console.WriteLine("========================================");

            int passed = 0;
            int failed = 0;

            string samplesDir = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"..\..\tests\samples");
            if (!Directory.Exists(samplesDir))
            {
                samplesDir = @"C:\Users\LG\Documents\ChatGPT\msgviewer\tests\samples";
            }

            // Test 1: Standard MSG with PDF attachment and HTML table
            try
            {
                string path = Path.Combine(samplesDir, "sample_standard.msg");
                Stopwatch sw = Stopwatch.StartNew();
                MsgMessage msg = MsgReader.Read(path);
                sw.Stop();

                Assert(msg.Subject == "오프라인 MSG 뷰어 구현 진행 보고", "AC-11: Subject matches");
                Assert(msg.SenderFormatted.Contains("김철수") && msg.SenderFormatted.Contains("chulsoo@example.com"), "AC-11: Sender matches");
                Assert(msg.DisplayTo.Contains("이영희") && msg.DisplayTo.Contains("박민수"), "AC-11: To matches");
                Assert(msg.DisplayCc.Contains("홍길동"), "AC-11: Cc matches");
                Assert(msg.SentDate.HasValue, "AC-11: SentDate parsed");
                Assert(msg.RegularAttachments.Count == 1, "AC-05: Exactly 1 regular attachment");
                Assert(msg.RegularAttachments[0].FileName == "architecture_report.pdf", "AC-05: Attachment filename matches");
                Assert(msg.RegularAttachments[0].Data != null && msg.RegularAttachments[0].Data.Length > 0, "AC-05: Attachment data preserved");
                Assert(msg.HtmlBody.Contains("<table") && msg.HtmlBody.Contains("단일 x86 EXE 빌드"), "AC-03: Body HTML table preserved");

                Console.WriteLine("  [PASS] Test 1: sample_standard.msg parsed in {0}ms", sw.ElapsedMilliseconds);
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 1: " + ex.Message);
                failed++;
            }

            // Test 2: Inline Image (cid:) handling
            try
            {
                string path = Path.Combine(samplesDir, "sample_inline_image.msg");
                Stopwatch sw = Stopwatch.StartNew();
                MsgMessage msg = MsgReader.Read(path);
                sw.Stop();

                Assert(msg.Subject == "사내 뉴스레터 (로고 포함)", "Subject matches");
                Assert(msg.Attachments.Count == 2, "Total 2 attachments in MSG");
                Assert(msg.RegularAttachments.Count == 1, "AC-05: Inline image excluded from regular attachments");
                Assert(msg.RegularAttachments[0].FileName == "announcement.txt", "Regular attachment is announcement.txt");
                Assert(msg.HtmlBody.Contains("data:image/png;base64,"), "AC-03: cid: replaced with offline data URI");

                Console.WriteLine("  [PASS] Test 2: sample_inline_image.msg (Inline CID handling) parsed in {0}ms", sw.ElapsedMilliseconds);
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 2: " + ex.Message);
                failed++;
            }

            // Test 3: Zero attachments
            try
            {
                string path = Path.Combine(samplesDir, "sample_no_attachments.msg");
                Stopwatch sw = Stopwatch.StartNew();
                MsgMessage msg = MsgReader.Read(path);
                sw.Stop();

                Assert(msg.RegularAttachments.Count == 0, "AC-05: 0 attachments reported");
                Console.WriteLine("  [PASS] Test 3: sample_no_attachments.msg parsed in {0}ms", sw.ElapsedMilliseconds);
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 3: " + ex.Message);
                failed++;
            }

            // Test 4: Long multi-line subject
            try
            {
                string path = Path.Combine(samplesDir, "sample_long_subject.msg");
                Stopwatch sw = Stopwatch.StartNew();
                MsgMessage msg = MsgReader.Read(path);
                sw.Stop();

                Assert(msg.Subject.Length > 100, "AC-11: Long subject intact (>100 chars)");
                Console.WriteLine("  [PASS] Test 4: sample_long_subject.msg parsed in {0}ms", sw.ElapsedMilliseconds);
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 4: " + ex.Message);
                failed++;
            }

            // Test 5: Localization & Fixed Language Button
            try
            {
                Assert(Localization.FixedLanguageButtonText == "언어 / Language / Langue / 言語", "AC-10: Button text is fixed");
                Localization.CurrentLanguage = AppLanguage.Korean;
                Assert(Localization.Open == "열기" && Localization.None == "없음", "AC-10: Korean strings correct");
                Localization.CurrentLanguage = AppLanguage.English;
                Assert(Localization.Open == "Open" && Localization.None == "None", "AC-10: English strings correct");
                Localization.CurrentLanguage = AppLanguage.French;
                Assert(Localization.Open == "Ouvrir" && Localization.None == "Aucun", "AC-10: French strings correct");
                Localization.CurrentLanguage = AppLanguage.Japanese;
                Assert(Localization.Open == "開く" && Localization.None == "なし", "AC-10: Japanese strings correct");

                Console.WriteLine("  [PASS] Test 5: Localization (4 languages + fixed button label)");
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 5: " + ex.Message);
                failed++;
            }

            // Test 6: Binary size check
            try
            {
                string exePath = @"C:\Users\LG\Documents\ChatGPT\msgviewer\dist\MsgViewer.exe";
                FileInfo fi = new FileInfo(exePath);
                Assert(fi.Exists, "EXE exists");
                Assert(fi.Length < 8000000, "AC-08: EXE size < 8,000,000 bytes (7~8MB target)");
                Console.WriteLine("  [PASS] Test 6: EXE Size = {0} bytes ({1:F2} KB / {2:F4} MB) - PASS",
                    fi.Length, fi.Length / 1024.0, fi.Length / (1024.0 * 1024.0));
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 6: " + ex.Message);
                failed++;
            }

            Console.WriteLine("========================================");
            Console.WriteLine("Results: {0} passed, {1} failed", passed, failed);
            Console.WriteLine("========================================");

            return failed == 0 ? 0 : 1;
        }

        static void Assert(bool condition, string message)
        {
            if (!condition)
            {
                throw new Exception("Assertion Failed: " + message);
            }
        }
    }
}
