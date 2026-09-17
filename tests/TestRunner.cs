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

            // Test 5: Localization & Fixed Language Button (Revision 6 / v1.1)
            try
            {
                Assert(Localization.FixedLanguageButtonText == "언어 / Language / Langue / 言語 (F8)", "AC-10: Button text is fixed with (F8)");
                Localization.CurrentLanguage = AppLanguage.Korean;
                Assert(Localization.Open == "열기 (F2)" && Localization.Close == "닫기 (F4)" && Localization.None == "없음", "AC-10: Korean strings correct");
                Localization.CurrentLanguage = AppLanguage.English;
                Assert(Localization.Open == "Open (F2)" && Localization.Close == "Close (F4)" && Localization.None == "None", "AC-10: English strings correct");
                Localization.CurrentLanguage = AppLanguage.French;
                Assert(Localization.Open == "Ouvrir (F2)" && Localization.Close == "Fermer (F4)" && Localization.None == "Aucun", "AC-10: French strings correct");
                Localization.CurrentLanguage = AppLanguage.Japanese;
                Assert(Localization.Open == "開く (F2)" && Localization.Close == "閉じる (F4)" && Localization.None == "なし", "AC-10: Japanese strings correct");

                // Test language cycling
                Assert(Localization.GetNextLanguage(AppLanguage.Korean) == AppLanguage.English, "Cycle: Korean -> English");
                Assert(Localization.GetNextLanguage(AppLanguage.English) == AppLanguage.French, "Cycle: English -> French");
                Assert(Localization.GetNextLanguage(AppLanguage.French) == AppLanguage.Japanese, "Cycle: French -> Japanese");
                Assert(Localization.GetNextLanguage(AppLanguage.Japanese) == AppLanguage.Korean, "Cycle: Japanese -> Korean");

                Console.WriteLine("  [PASS] Test 5: Localization (4 languages + fixed (F8) button label + F8 cycling)");
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 5: " + ex.Message);
                failed++;
            }

            // Test 6: Empty To & Empty Subject Display Logic
            try
            {
                MsgMessage emptyMsg = new MsgMessage();
                emptyMsg.DisplayTo = "";
                emptyMsg.Subject = null;
                emptyMsg.SenderName = "sender";

                Localization.CurrentLanguage = AppLanguage.Korean;
                string toKorean = !string.IsNullOrEmpty(emptyMsg.DisplayTo) && emptyMsg.DisplayTo.Trim().Length > 0 ? emptyMsg.DisplayTo : Localization.None;
                string subjKorean = !string.IsNullOrEmpty(emptyMsg.Subject) && emptyMsg.Subject.Trim().Length > 0 ? emptyMsg.Subject : Localization.None;
                Assert(toKorean == "없음", "Empty To displayed as '없음'");
                Assert(subjKorean == "없음", "Empty Subject displayed as '없음'");

                Localization.CurrentLanguage = AppLanguage.English;
                string toEnglish = !string.IsNullOrEmpty(emptyMsg.DisplayTo) && emptyMsg.DisplayTo.Trim().Length > 0 ? emptyMsg.DisplayTo : Localization.None;
                string subjEnglish = !string.IsNullOrEmpty(emptyMsg.Subject) && emptyMsg.Subject.Trim().Length > 0 ? emptyMsg.Subject : Localization.None;
                Assert(toEnglish == "None", "Empty To displayed as 'None'");
                Assert(subjEnglish == "None", "Empty Subject displayed as 'None'");

                Localization.CurrentLanguage = AppLanguage.French;
                string toFrench = !string.IsNullOrEmpty(emptyMsg.DisplayTo) && emptyMsg.DisplayTo.Trim().Length > 0 ? emptyMsg.DisplayTo : Localization.None;
                string subjFrench = !string.IsNullOrEmpty(emptyMsg.Subject) && emptyMsg.Subject.Trim().Length > 0 ? emptyMsg.Subject : Localization.None;
                Assert(toFrench == "Aucun", "Empty To displayed as 'Aucun'");
                Assert(subjFrench == "Aucun", "Empty Subject displayed as 'Aucun'");

                Localization.CurrentLanguage = AppLanguage.Japanese;
                string toJapanese = !string.IsNullOrEmpty(emptyMsg.DisplayTo) && emptyMsg.DisplayTo.Trim().Length > 0 ? emptyMsg.DisplayTo : Localization.None;
                string subjJapanese = !string.IsNullOrEmpty(emptyMsg.Subject) && emptyMsg.Subject.Trim().Length > 0 ? emptyMsg.Subject : Localization.None;
                Assert(toJapanese == "なし", "Empty To displayed as 'なし'");
                Assert(subjJapanese == "なし", "Empty Subject displayed as 'なし'");

                Console.WriteLine("  [PASS] Test 6: Empty To & Empty Subject None handling across 4 languages");
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 6: " + ex.Message);
                failed++;
            }


            // Test 7: Binary size check
            try
            {
                string exePath = @"C:\Users\LG\Documents\ChatGPT\msgviewer\dist\MsgViewer.exe";
                FileInfo fi = new FileInfo(exePath);
                Assert(fi.Exists, "EXE exists");
                Assert(fi.Length < 8000000, "AC-08: EXE size < 8,000,000 bytes (7~8MB target)");
                Console.WriteLine("  [PASS] Test 7: EXE Size = {0} bytes ({1:F2} KB / {2:F4} MB) - PASS",
                    fi.Length, fi.Length / 1024.0, fi.Length / (1024.0 * 1024.0));
                passed++;
            }
            catch (Exception ex)
            {
                Console.WriteLine("  [FAIL] Test 7: " + ex.Message);
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
