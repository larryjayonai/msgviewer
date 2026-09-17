using System;
using System.IO;
using System.Text;

namespace MsgViewer
{
    public static class RtfDecompressor
    {
        private const string Prebuf =
            "{\\rtf1\\ansi\\mac\\deff0\\deflang1033{\\fonttbl{\\f0\\fnil\\fcharset0 Times New Roman;}" +
            "{\\f1\\fnil\\fcharset0 Symbol;}{\\f2\\fswiss\\fcharset0 Arial;}}{\\colortbl;\\red0\\green0\\blue0;" +
            "\\red0\\green0\\blue255;\\red0\\green255\\blue255;\\red0\\green255\\blue0;\\red255\\green0\\blue255;" +
            "\\red255\\green0\\blue0;\\red255\\green255\\blue0;\\red255\\green255\\blue255;\\red128\\green128\\blue128;}" +
            "{\\stylesheet{\\normal\\fi0\\li0\\ri0\\sa0\\sb0\\fs24\\cf0 Normal;}{\\*\\cs10\\fs20\\cf0 Default Paragraph Font;}}" +
            "{\\info{\\version1\\edmins0\\nofpages1\\nofwords0\\nofchars0\\vern32454}}";

        public static byte[] Decompress(byte[] compressedBytes)
        {
            if (compressedBytes == null || compressedBytes.Length < 16)
                return compressedBytes;

            using (MemoryStream ms = new MemoryStream(compressedBytes))
            using (BinaryReader reader = new BinaryReader(ms))
            {
                uint compSize = reader.ReadUInt32();
                uint uncompSize = reader.ReadUInt32();
                uint magic = reader.ReadUInt32();
                uint crc = reader.ReadUInt32();

                // Magic 0x414d454c = "LZFu" in little-endian byte order
                if (magic == 0x414d454c)
                {
                    // Compressed LZFu
                    byte[] dictionary = new byte[4096];
                    byte[] prebufBytes = Encoding.ASCII.GetBytes(Prebuf);
                    Array.Copy(prebufBytes, 0, dictionary, 0, Math.Min(prebufBytes.Length, dictionary.Length));
                    int writeOffset = prebufBytes.Length;

                    byte[] output = new byte[uncompSize];
                    int outIdx = 0;

                    while (ms.Position < ms.Length && outIdx < uncompSize)
                    {
                        int controlByte = ms.ReadByte();
                        if (controlByte < 0) break;

                        for (int bit = 0; bit < 8 && outIdx < uncompSize; bit++)
                        {
                            bool isLiteral = ((controlByte >> bit) & 1) == 1;
                            if (isLiteral)
                            {
                                int b = ms.ReadByte();
                                if (b < 0) break;
                                output[outIdx++] = (byte)b;
                                dictionary[writeOffset] = (byte)b;
                                writeOffset = (writeOffset + 1) % 4096;
                            }
                            else
                            {
                                int b1 = ms.ReadByte();
                                int b2 = ms.ReadByte();
                                if (b1 < 0 || b2 < 0) break;

                                int offset = (b1 << 4) | (b2 >> 4);
                                int length = (b2 & 0x0F) + 2;

                                for (int i = 0; i < length && outIdx < uncompSize; i++)
                                {
                                    byte b = dictionary[(offset + i) % 4096];
                                    output[outIdx++] = b;
                                    dictionary[writeOffset] = b;
                                    writeOffset = (writeOffset + 1) % 4096;
                                }
                            }
                        }
                    }
                    return output;
                }
                else if (magic == 0x75465a4d) // Uncompressed RTF marker "MZFu"
                {
                    byte[] output = new byte[uncompSize];
                    int read = ms.Read(output, 0, (int)uncompSize);
                    return output;
                }
                else
                {
                    // Unknown header, return as-is
                    return compressedBytes;
                }
            }
        }

        public static string ExtractHtmlFromRtf(string rtfText)
        {
            if (string.IsNullOrEmpty(rtfText)) return null;
            if (!rtfText.Contains("\\fromhtml")) return null;

            StringBuilder sb = new StringBuilder();
            int idx = 0;
            while (idx < rtfText.Length)
            {
                int htmlTagIdx = rtfText.IndexOf("\\htmltag", idx, StringComparison.OrdinalIgnoreCase);
                if (htmlTagIdx < 0) break;

                int spaceIdx = htmlTagIdx + 8;
                while (spaceIdx < rtfText.Length && char.IsDigit(rtfText[spaceIdx]))
                {
                    spaceIdx++;
                }
                if (spaceIdx < rtfText.Length && rtfText[spaceIdx] == ' ')
                {
                    spaceIdx++;
                }

                int endTag = spaceIdx;
                while (endTag < rtfText.Length && rtfText[endTag] != '\\' && rtfText[endTag] != '}' && rtfText[endTag] != '{')
                {
                    endTag++;
                }

                if (endTag > spaceIdx)
                {
                    string tagContent = rtfText.Substring(spaceIdx, endTag - spaceIdx);
                    sb.Append(tagContent);
                }
                idx = endTag;
            }

            string html = sb.ToString().Trim();
            return html.Length > 0 ? html : null;
        }
    }
}
