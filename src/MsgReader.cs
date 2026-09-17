using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

namespace MsgViewer
{
    public class MsgAttachment
    {
        public string FileName { get; set; }
        public byte[] Data { get; set; }
        public string MimeType { get; set; }
        public string ContentId { get; set; }
        public bool IsInline { get; set; }
        public bool IsEmbeddedMsg { get; set; }
        public DirectoryEntry StorageEntry { get; set; }

        public override string ToString()
        {
            return FileName ?? "Unnamed";
        }
    }

    public class MsgRecipient
    {
        public string DisplayName { get; set; }
        public string EmailAddress { get; set; }
        public int RecipientType { get; set; } // 1=To, 2=Cc, 3=Bcc

        public string Formatted
        {
            get
            {
                if (!string.IsNullOrEmpty(DisplayName) && !string.IsNullOrEmpty(EmailAddress) &&
                    !string.Equals(DisplayName, EmailAddress, StringComparison.OrdinalIgnoreCase))
                {
                    return string.Format("{0} <{1}>", DisplayName, EmailAddress);
                }
                return DisplayName ?? EmailAddress ?? "";
            }
        }
    }

    public class MsgMessage
    {
        public string Subject { get; set; }
        public string SenderName { get; set; }
        public string SenderEmail { get; set; }
        public string DisplayTo { get; set; }
        public string DisplayCc { get; set; }
        public string DisplayBcc { get; set; }
        public DateTime? SentDate { get; set; }
        public string HtmlBody { get; set; }
        public string PlainTextBody { get; set; }
        public string RtfBody { get; set; }
        public List<MsgRecipient> Recipients { get; set; }
        public List<MsgAttachment> Attachments { get; set; }

        public MsgMessage()
        {
            Recipients = new List<MsgRecipient>();
            Attachments = new List<MsgAttachment>();
        }

        public string SenderFormatted
        {
            get
            {
                if (!string.IsNullOrEmpty(SenderName) && !string.IsNullOrEmpty(SenderEmail) &&
                    !string.Equals(SenderName, SenderEmail, StringComparison.OrdinalIgnoreCase))
                {
                    return string.Format("{0} <{1}>", SenderName, SenderEmail);
                }
                return SenderName ?? SenderEmail ?? "";
            }
        }

        public List<MsgAttachment> RegularAttachments
        {
            get
            {
                List<MsgAttachment> list = new List<MsgAttachment>();
                foreach (MsgAttachment att in Attachments)
                {
                    if (!att.IsInline)
                    {
                        list.Add(att);
                    }
                }
                return list;
            }
        }
    }

    public static class MsgReader
    {
        public static MsgMessage Read(string filePath)
        {
            using (CompoundFile cf = new CompoundFile(filePath))
            {
                return Read(cf);
            }
        }

        public static MsgMessage Read(CompoundFile cf)
        {
            MsgMessage msg = new MsgMessage();
            DirectoryEntry root = cf.RootEntry;
            if (root == null) return msg;

            // Parse root properties stream
            DirectoryEntry propEntry = cf.FindChild(root, "__properties_version1.0");
            if (propEntry != null)
            {
                byte[] propBytes = cf.ReadStream(propEntry);
                ParsePropertiesStream(propBytes, 32, msg);
            }

            // Parse top-level streams (__substg1.0_PPPPVVVV)
            foreach (DirectoryEntry child in root.Children)
            {
                if (child.ObjectType == CfObjectTypes.Stream && child.Name.StartsWith("__substg1.0_"))
                {
                    string tagStr = child.Name.Substring(12);
                    if (tagStr.Length == 8)
                    {
                        string propId = tagStr.Substring(0, 4).ToUpperInvariant();
                        string propType = tagStr.Substring(4, 4).ToUpperInvariant();
                        byte[] data = cf.ReadStream(child);

                        ApplyMessageProperty(msg, propId, propType, data);
                    }
                }
            }

            // Parse Recipients (__recip_version1.0_#...)
            List<DirectoryEntry> recipEntries = cf.FindChildrenStartingWith(root, "__recip_version1.0_#");
            foreach (DirectoryEntry recipStorage in recipEntries)
            {
                MsgRecipient recip = ParseRecipient(cf, recipStorage);
                if (recip != null)
                {
                    msg.Recipients.Add(recip);
                }
            }

            // Fill display To/Cc/Bcc from recipients if missing
            FillRecipientsDisplay(msg);

            // Parse Attachments (__attach_version1.0_#...)
            List<DirectoryEntry> attachEntries = cf.FindChildrenStartingWith(root, "__attach_version1.0_#");
            foreach (DirectoryEntry attachStorage in attachEntries)
            {
                MsgAttachment att = ParseAttachment(cf, attachStorage);
                if (att != null)
                {
                    msg.Attachments.Add(att);
                }
            }

            // Resolve final body and inline images
            ResolveBody(msg);

            return msg;
        }

        private static void ApplyMessageProperty(MsgMessage msg, string propId, string propType, byte[] data)
        {
            if (data == null || data.Length == 0) return;

            string text = null;
            if (propType == "001F") // Unicode
            {
                text = Encoding.Unicode.GetString(data).TrimEnd('\0');
            }
            else if (propType == "001E") // Ansi
            {
                text = Encoding.Default.GetString(data).TrimEnd('\0');
            }

            switch (propId)
            {
                case "0037": // Subject
                    if (string.IsNullOrEmpty(msg.Subject)) msg.Subject = text;
                    break;
                case "0C1A": // Sender Name
                    if (string.IsNullOrEmpty(msg.SenderName)) msg.SenderName = text;
                    break;
                case "0C1F": // Sender Email
                case "5D01": // Sender Smtp
                case "39FE": // SmtpAddress
                    if (string.IsNullOrEmpty(msg.SenderEmail)) msg.SenderEmail = text;
                    break;
                case "0E04": // Display To
                    if (string.IsNullOrEmpty(msg.DisplayTo)) msg.DisplayTo = text;
                    break;
                case "0E03": // Display CC
                    if (string.IsNullOrEmpty(msg.DisplayCc)) msg.DisplayCc = text;
                    break;
                case "0E02": // Display BCC
                    if (string.IsNullOrEmpty(msg.DisplayBcc)) msg.DisplayBcc = text;
                    break;
                case "1000": // Plain Text Body
                    if (string.IsNullOrEmpty(msg.PlainTextBody)) msg.PlainTextBody = text;
                    break;
                case "1013": // HTML Body
                    if (string.IsNullOrEmpty(msg.HtmlBody))
                    {
                        if (propType == "0102") // Binary
                        {
                            msg.HtmlBody = DecodeHtmlBytes(data);
                        }
                        else if (text != null)
                        {
                            msg.HtmlBody = text;
                        }
                    }
                    break;
                case "1009": // Compressed RTF
                    if (string.IsNullOrEmpty(msg.RtfBody) && propType == "0102")
                    {
                        byte[] decomp = RtfDecompressor.Decompress(data);
                        if (decomp != null && decomp.Length > 0)
                        {
                            msg.RtfBody = Encoding.Default.GetString(decomp);
                        }
                    }
                    break;
            }
        }

        private static string DecodeHtmlBytes(byte[] data)
        {
            if (data == null || data.Length == 0) return "";

            // Check BOM
            if (data.Length >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
                return Encoding.UTF8.GetString(data, 3, data.Length - 3).TrimEnd('\0');
            if (data.Length >= 2 && data[0] == 0xFF && data[1] == 0xFE)
                return Encoding.Unicode.GetString(data, 2, data.Length - 2).TrimEnd('\0');

            string utf8 = Encoding.UTF8.GetString(data).TrimEnd('\0');
            if (utf8.Contains("charset=utf-8") || utf8.Contains("charset=\"utf-8\"") ||
                utf8.Contains("charset=UTF-8") || utf8.Contains("charset=\"UTF-8\""))
            {
                return utf8;
            }

            // Check if contains euc-kr or cp949
            if (utf8.Contains("euc-kr") || utf8.Contains("cp949"))
            {
                try
                {
                    return Encoding.GetEncoding(949).GetString(data).TrimEnd('\0');
                }
                catch { }
            }

            // Default to UTF-8
            return utf8;
        }

        private static void ParsePropertiesStream(byte[] propBytes, int headerSize, MsgMessage msg)
        {
            if (propBytes == null || propBytes.Length < headerSize + 16) return;

            int count = (propBytes.Length - headerSize) / 16;
            for (int i = 0; i < count; i++)
            {
                int offset = headerSize + (i * 16);
                ushort propType = BitConverter.ToUInt16(propBytes, offset);
                ushort propId = BitConverter.ToUInt16(propBytes, offset + 2);

                if (propType == 0x0040) // PT_SYSTIME (FILETIME)
                {
                    long fileTime = BitConverter.ToInt64(propBytes, offset + 8);
                    if (fileTime > 0)
                    {
                        try
                        {
                            DateTime dt = DateTime.FromFileTime(fileTime);
                            if (propId == 0x0039) // ClientSubmitTime
                            {
                                msg.SentDate = dt;
                            }
                            else if (propId == 0x0E06 && !msg.SentDate.HasValue) // MessageDeliveryTime
                            {
                                msg.SentDate = dt;
                            }
                        }
                        catch { }
                    }
                }
            }
        }

        private static MsgRecipient ParseRecipient(CompoundFile cf, DirectoryEntry storage)
        {
            MsgRecipient recip = new MsgRecipient();
            DirectoryEntry propEntry = cf.FindChild(storage, "__properties_version1.0");
            if (propEntry != null)
            {
                byte[] propBytes = cf.ReadStream(propEntry);
                if (propBytes != null && propBytes.Length >= 24)
                {
                    // Recipient properties header is 8 bytes
                    int count = (propBytes.Length - 8) / 16;
                    for (int i = 0; i < count; i++)
                    {
                        int offset = 8 + (i * 16);
                        ushort pType = BitConverter.ToUInt16(propBytes, offset);
                        ushort pId = BitConverter.ToUInt16(propBytes, offset + 2);
                        if (pId == 0x0C15) // PR_RECIPIENT_TYPE
                        {
                            recip.RecipientType = BitConverter.ToInt32(propBytes, offset + 8);
                        }
                    }
                }
            }

            foreach (DirectoryEntry child in storage.Children)
            {
                if (child.ObjectType == CfObjectTypes.Stream && child.Name.StartsWith("__substg1.0_"))
                {
                    string tag = child.Name.Substring(12).ToUpperInvariant();
                    if (tag.Length == 8)
                    {
                        string pId = tag.Substring(0, 4);
                        string pType = tag.Substring(4, 4);
                        byte[] data = cf.ReadStream(child);
                        string text = pType == "001F" ? Encoding.Unicode.GetString(data).TrimEnd('\0')
                                    : pType == "001E" ? Encoding.Default.GetString(data).TrimEnd('\0')
                                    : null;

                        if (text != null)
                        {
                            if (pId == "3001") recip.DisplayName = text;
                            else if (pId == "39FE" || pId == "3003") recip.EmailAddress = text;
                        }
                    }
                }
            }

            return recip;
        }

        private static void FillRecipientsDisplay(MsgMessage msg)
        {
            if (string.IsNullOrEmpty(msg.DisplayTo))
            {
                List<string> toList = new List<string>();
                foreach (MsgRecipient r in msg.Recipients)
                {
                    if (r.RecipientType == 1 || r.RecipientType == 0) toList.Add(r.Formatted);
                }
                if (toList.Count > 0) msg.DisplayTo = string.Join("; ", toList.ToArray());
            }

            if (string.IsNullOrEmpty(msg.DisplayCc))
            {
                List<string> ccList = new List<string>();
                foreach (MsgRecipient r in msg.Recipients)
                {
                    if (r.RecipientType == 2) ccList.Add(r.Formatted);
                }
                if (ccList.Count > 0) msg.DisplayCc = string.Join("; ", ccList.ToArray());
            }

            if (string.IsNullOrEmpty(msg.DisplayBcc))
            {
                List<string> bccList = new List<string>();
                foreach (MsgRecipient r in msg.Recipients)
                {
                    if (r.RecipientType == 3) bccList.Add(r.Formatted);
                }
                if (bccList.Count > 0) msg.DisplayBcc = string.Join("; ", bccList.ToArray());
            }
        }

        private static MsgAttachment ParseAttachment(CompoundFile cf, DirectoryEntry storage)
        {
            MsgAttachment att = new MsgAttachment();
            att.StorageEntry = storage;

            int attachMethod = 1;
            int attachFlags = 0;

            DirectoryEntry propEntry = cf.FindChild(storage, "__properties_version1.0");
            if (propEntry != null)
            {
                byte[] propBytes = cf.ReadStream(propEntry);
                if (propBytes != null && propBytes.Length >= 24)
                {
                    int count = (propBytes.Length - 8) / 16;
                    for (int i = 0; i < count; i++)
                    {
                        int offset = 8 + (i * 16);
                        ushort pId = BitConverter.ToUInt16(propBytes, offset + 2);
                        if (pId == 0x3705) // PR_ATTACH_METHOD
                        {
                            attachMethod = BitConverter.ToInt32(propBytes, offset + 8);
                        }
                        else if (pId == 0x3714) // PR_ATTACH_FLAGS
                        {
                            attachFlags = BitConverter.ToInt32(propBytes, offset + 8);
                        }
                    }
                }
            }

            att.IsEmbeddedMsg = (attachMethod == 5);
            if ((attachFlags & 0x00000004) != 0) // ATT_MHTML_REF
            {
                att.IsInline = true;
            }

            foreach (DirectoryEntry child in storage.Children)
            {
                if (child.ObjectType == CfObjectTypes.Stream && child.Name.StartsWith("__substg1.0_"))
                {
                    string tag = child.Name.Substring(12).ToUpperInvariant();
                    if (tag.Length == 8)
                    {
                        string pId = tag.Substring(0, 4);
                        string pType = tag.Substring(4, 4);
                        byte[] data = cf.ReadStream(child);

                        string text = pType == "001F" ? Encoding.Unicode.GetString(data).TrimEnd('\0')
                                    : pType == "001E" ? Encoding.Default.GetString(data).TrimEnd('\0')
                                    : null;

                        if (pId == "3707" && !string.IsNullOrEmpty(text)) // AttachLongFilename
                        {
                            att.FileName = text;
                        }
                        else if (pId == "3704" && string.IsNullOrEmpty(att.FileName) && !string.IsNullOrEmpty(text)) // AttachFilename
                        {
                            att.FileName = text;
                        }
                        else if (pId == "3001" && string.IsNullOrEmpty(att.FileName) && !string.IsNullOrEmpty(text)) // DisplayName
                        {
                            att.FileName = text;
                        }
                        else if (pId == "370E" && !string.IsNullOrEmpty(text)) // AttachMimeTag
                        {
                            att.MimeType = text;
                        }
                        else if (pId == "3716" && !string.IsNullOrEmpty(text)) // AttachContentId
                        {
                            att.ContentId = text.Trim('<', '>');
                        }
                        else if (pId == "3701" && pType == "0102") // AttachDataBinary
                        {
                            att.Data = data;
                        }
                    }
                }
                else if (child.ObjectType == CfObjectTypes.Storage && child.Name.StartsWith("__substg1.0_3701000D")) // Embedded message
                {
                    att.IsEmbeddedMsg = true;
                    if (string.IsNullOrEmpty(att.FileName))
                    {
                        att.FileName = "attached_message.msg";
                    }
                }
            }

            if (string.IsNullOrEmpty(att.FileName))
            {
                att.FileName = att.IsEmbeddedMsg ? "attached_message.msg" : "attachment.dat";
            }

            // Guess MimeType if empty
            if (string.IsNullOrEmpty(att.MimeType))
            {
                string ext = Path.GetExtension(att.FileName).ToLowerInvariant();
                switch (ext)
                {
                    case ".png": att.MimeType = "image/png"; break;
                    case ".jpg": case ".jpeg": att.MimeType = "image/jpeg"; break;
                    case ".gif": att.MimeType = "image/gif"; break;
                    case ".bmp": att.MimeType = "image/bmp"; break;
                    case ".pdf": att.MimeType = "application/pdf"; break;
                    case ".txt": att.MimeType = "text/plain"; break;
                    case ".msg": att.MimeType = "application/vnd.ms-outlook"; break;
                    default: att.MimeType = "application/octet-stream"; break;
                }
            }

            return att;
        }

        private static void ResolveBody(MsgMessage msg)
        {
            // 1. Try HTML body
            string finalHtml = msg.HtmlBody;

            // 2. If no HTML, try extracting HTML from RTF
            if (string.IsNullOrEmpty(finalHtml) && !string.IsNullOrEmpty(msg.RtfBody))
            {
                finalHtml = RtfDecompressor.ExtractHtmlFromRtf(msg.RtfBody);
            }

            // 3. If still no HTML, format plain text as HTML
            if (string.IsNullOrEmpty(finalHtml) && !string.IsNullOrEmpty(msg.PlainTextBody))
            {
                string encoded = System.Security.SecurityElement.Escape(msg.PlainTextBody);
                finalHtml = string.Format(
                    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>" +
                    "body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; " +
                    "font-size: 14px; line-height: 1.5; color: #222; margin: 12px; }} " +
                    "pre {{ font-family: inherit; white-space: pre-wrap; word-wrap: break-word; margin: 0; }} " +
                    "</style></head><body><pre>{0}</pre></body></html>",
                    encoded);
            }

            // 4. Default empty body
            if (string.IsNullOrEmpty(finalHtml))
            {
                finalHtml = "<!DOCTYPE html><html><body></body></html>";
            }

            // Inline image replacement (cid: -> data:...)
            foreach (MsgAttachment att in msg.Attachments)
            {
                if (!string.IsNullOrEmpty(att.ContentId) && att.Data != null && att.Data.Length > 0)
                {
                    string pattern = @"(?i)(src\s*=\s*[""'])cid:(" + Regex.Escape(att.ContentId) + @")([""'])";
                    if (Regex.IsMatch(finalHtml, pattern))
                    {
                        att.IsInline = true;
                        string base64 = Convert.ToBase64String(att.Data);
                        string dataUri = string.Format("data:{0};base64,{1}", att.MimeType, base64);
                        finalHtml = Regex.Replace(finalHtml, pattern, string.Format("$1{0}$3", dataUri));
                    }
                }
            }

            // Ensure UTF-8 meta and clean styling
            if (!finalHtml.Contains("<head>") && !finalHtml.Contains("<HEAD>"))
            {
                finalHtml = "<head><meta charset=\"utf-8\"></head>" + finalHtml;
            }

            msg.HtmlBody = finalHtml;
        }
    }
}
