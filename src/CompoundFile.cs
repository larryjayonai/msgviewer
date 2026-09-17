using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace MsgViewer
{
    public enum CfObjectTypes : byte
    {
        Unknown = 0,
        Storage = 1,
        Stream = 2,
        Root = 5
    }

    public class DirectoryEntry
    {
        public int EntryId { get; set; }
        public string Name { get; set; }
        public CfObjectTypes ObjectType { get; set; }
        public int LeftSiblingId { get; set; }
        public int RightSiblingId { get; set; }
        public int ChildSiblingId { get; set; }
        public uint StartingSector { get; set; }
        public long StreamSize { get; set; }
        public List<DirectoryEntry> Children { get; set; }

        public DirectoryEntry()
        {
            Children = new List<DirectoryEntry>();
        }

        public override string ToString()
        {
            return string.Format("{0} ({1}, Size: {2})", Name, ObjectType, StreamSize);
        }
    }

    public class CompoundFile : IDisposable
    {
        private Stream _stream;
        private bool _ownsStream;
        private ushort _sectorShift;
        private ushort _miniSectorShift;
        private int _sectorSize;
        private int _miniSectorSize;
        private uint _firstDirSector;
        private uint _miniStreamCutoff;
        private uint _firstMiniFatSector;
        private uint _numMiniFatSectors;
        private uint _firstDiFatSector;
        private uint _numDiFatSectors;

        private List<uint> _fat = new List<uint>();
        private List<uint> _miniFat = new List<uint>();
        private List<DirectoryEntry> _dirEntries = new List<DirectoryEntry>();
        private byte[] _miniStreamData;

        public DirectoryEntry RootEntry
        {
            get { return _dirEntries.Count > 0 ? _dirEntries[0] : null; }
        }

        public CompoundFile(string filePath)
        {
            _stream = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            _ownsStream = true;
            Parse();
        }

        public CompoundFile(Stream stream)
        {
            _stream = stream;
            _ownsStream = false;
            Parse();
        }

        public void Dispose()
        {
            if (_ownsStream && _stream != null)
            {
                _stream.Dispose();
                _stream = null;
            }
        }

        private void Parse()
        {
            BinaryReader reader = new BinaryReader(_stream);
            _stream.Seek(0, SeekOrigin.Begin);

            byte[] magic = reader.ReadBytes(8);
            if (magic[0] != 0xD0 || magic[1] != 0xCF || magic[2] != 0x11 || magic[3] != 0xE0 ||
                magic[4] != 0xA1 || magic[5] != 0xB1 || magic[6] != 0x1A || magic[7] != 0xE1)
            {
                throw new InvalidDataException("Invalid OLE Compound File Header Magic");
            }

            _stream.Seek(30, SeekOrigin.Begin);
            _sectorShift = reader.ReadUInt16();
            _miniSectorShift = reader.ReadUInt16();
            _sectorSize = 1 << _sectorShift;
            _miniSectorSize = 1 << _miniSectorShift;

            _stream.Seek(40, SeekOrigin.Begin);
            uint numDirSectors = reader.ReadUInt32();
            uint numFatSectors = reader.ReadUInt32();
            _firstDirSector = reader.ReadUInt32();
            reader.ReadUInt32(); // Transaction signature
            _miniStreamCutoff = reader.ReadUInt32();
            _firstMiniFatSector = reader.ReadUInt32();
            _numMiniFatSectors = reader.ReadUInt32();
            _firstDiFatSector = reader.ReadUInt32();
            _numDiFatSectors = reader.ReadUInt32();

            // Read DIFAT (first 109 entries in header, starting at offset 76)
            _stream.Seek(76, SeekOrigin.Begin);
            List<uint> diFat = new List<uint>();
            for (int i = 0; i < 109; i++)
            {
                uint sector = reader.ReadUInt32();
                if (sector < 0xFFFFFFFC)
                {
                    diFat.Add(sector);
                }
            }

            // Follow DIFAT sectors if any
            uint currentDiFatSector = _firstDiFatSector;
            while (currentDiFatSector < 0xFFFFFFFC && diFat.Count < numFatSectors)
            {
                _stream.Seek((currentDiFatSector + 1) * _sectorSize, SeekOrigin.Begin);
                int entriesInSector = (_sectorSize / 4) - 1;
                for (int i = 0; i < entriesInSector; i++)
                {
                    uint s = reader.ReadUInt32();
                    if (s < 0xFFFFFFFC) diFat.Add(s);
                }
                currentDiFatSector = reader.ReadUInt32();
            }

            // Read FAT
            foreach (uint fatSector in diFat)
            {
                _stream.Seek((fatSector + 1) * _sectorSize, SeekOrigin.Begin);
                int count = _sectorSize / 4;
                for (int i = 0; i < count; i++)
                {
                    _fat.Add(reader.ReadUInt32());
                }
            }

            // Read Directory Sectors
            List<byte> dirBytes = new List<byte>();
            uint curDirSector = _firstDirSector;
            while (curDirSector < 0xFFFFFFFC)
            {
                _stream.Seek((curDirSector + 1) * _sectorSize, SeekOrigin.Begin);
                byte[] buffer = reader.ReadBytes(_sectorSize);
                dirBytes.AddRange(buffer);
                if (curDirSector < _fat.Count)
                    curDirSector = _fat[(int)curDirSector];
                else
                    break;
            }

            // Parse Directory Entries (128 bytes each)
            int numEntries = dirBytes.Count / 128;
            for (int i = 0; i < numEntries; i++)
            {
                int offset = i * 128;
                ushort nameLen = BitConverter.ToUInt16(dirBytes.ToArray(), offset + 64);
                string name = "";
                if (nameLen > 2)
                {
                    name = Encoding.Unicode.GetString(dirBytes.ToArray(), offset, nameLen - 2);
                }

                byte objType = dirBytes[offset + 66];
                int leftSibling = BitConverter.ToInt32(dirBytes.ToArray(), offset + 68);
                int rightSibling = BitConverter.ToInt32(dirBytes.ToArray(), offset + 72);
                int childSibling = BitConverter.ToInt32(dirBytes.ToArray(), offset + 76);
                uint startSector = BitConverter.ToUInt32(dirBytes.ToArray(), offset + 116);
                long streamSize = BitConverter.ToInt64(dirBytes.ToArray(), offset + 120);

                DirectoryEntry entry = new DirectoryEntry();
                entry.EntryId = i;
                entry.Name = name;
                entry.ObjectType = (CfObjectTypes)objType;
                entry.LeftSiblingId = leftSibling;
                entry.RightSiblingId = rightSibling;
                entry.ChildSiblingId = childSibling;
                entry.StartingSector = startSector;
                entry.StreamSize = streamSize;
                _dirEntries.Add(entry);
            }

            // Build hierarchy from tree siblings
            foreach (DirectoryEntry entry in _dirEntries)
            {
                if (entry.ChildSiblingId >= 0 && entry.ChildSiblingId < _dirEntries.Count)
                {
                    TraverseSiblings(entry, entry.ChildSiblingId);
                }
            }

            // Read MiniFAT
            uint curMiniFatSector = _firstMiniFatSector;
            while (curMiniFatSector < 0xFFFFFFFC)
            {
                _stream.Seek((curMiniFatSector + 1) * _sectorSize, SeekOrigin.Begin);
                int count = _sectorSize / 4;
                for (int i = 0; i < count; i++)
                {
                    _miniFat.Add(reader.ReadUInt32());
                }
                if (curMiniFatSector < _fat.Count)
                    curMiniFatSector = _fat[(int)curMiniFatSector];
                else
                    break;
            }

            // Read MiniStream data (from Root Entry)
            if (RootEntry != null && RootEntry.StartingSector < 0xFFFFFFFC && RootEntry.StreamSize > 0)
            {
                _miniStreamData = ReadStreamDataDirect(RootEntry.StartingSector, RootEntry.StreamSize);
            }
        }

        private void TraverseSiblings(DirectoryEntry parent, int childId)
        {
            if (childId < 0 || childId >= _dirEntries.Count) return;
            DirectoryEntry child = _dirEntries[childId];
            if (!parent.Children.Contains(child))
            {
                parent.Children.Add(child);
            }

            if (child.LeftSiblingId >= 0 && child.LeftSiblingId < _dirEntries.Count)
            {
                TraverseSiblings(parent, child.LeftSiblingId);
            }
            if (child.RightSiblingId >= 0 && child.RightSiblingId < _dirEntries.Count)
            {
                TraverseSiblings(parent, child.RightSiblingId);
            }
        }

        public byte[] ReadStreamDataDirect(uint startSector, long size)
        {
            List<byte> result = new List<byte>((int)Math.Min(size, 10 * 1024 * 1024));
            uint curSector = startSector;
            long remaining = size;
            byte[] buf = new byte[_sectorSize];

            while (curSector < 0xFFFFFFFC && remaining > 0)
            {
                _stream.Seek((curSector + 1) * _sectorSize, SeekOrigin.Begin);
                int toRead = (int)Math.Min(remaining, _sectorSize);
                int read = _stream.Read(buf, 0, toRead);
                if (read <= 0) break;
                for (int i = 0; i < read; i++) result.Add(buf[i]);
                remaining -= read;

                if (curSector < _fat.Count)
                    curSector = _fat[(int)curSector];
                else
                    break;
            }

            return result.ToArray();
        }

        public byte[] ReadStream(DirectoryEntry entry)
        {
            if (entry == null || entry.ObjectType != CfObjectTypes.Stream || entry.StreamSize <= 0)
            {
                return new byte[0];
            }

            if (entry.StreamSize < _miniStreamCutoff && _miniStreamData != null)
            {
                // Read from MiniStream
                List<byte> result = new List<byte>((int)entry.StreamSize);
                uint curMiniSector = entry.StartingSector;
                long remaining = entry.StreamSize;

                while (curMiniSector < 0xFFFFFFFC && remaining > 0)
                {
                    long offset = (long)curMiniSector * _miniSectorSize;
                    if (offset >= _miniStreamData.Length) break;
                    int toRead = (int)Math.Min(remaining, _miniSectorSize);
                    if (offset + toRead > _miniStreamData.Length)
                        toRead = (int)(_miniStreamData.Length - offset);

                    for (int i = 0; i < toRead; i++)
                    {
                        result.Add(_miniStreamData[offset + i]);
                    }
                    remaining -= toRead;

                    if (curMiniSector < _miniFat.Count)
                        curMiniSector = _miniFat[(int)curMiniSector];
                    else
                        break;
                }

                return result.ToArray();
            }
            else
            {
                // Read from regular FAT stream
                return ReadStreamDataDirect(entry.StartingSector, entry.StreamSize);
            }
        }

        public DirectoryEntry FindChild(DirectoryEntry parent, string name)
        {
            if (parent == null) return null;
            foreach (DirectoryEntry child in parent.Children)
            {
                if (string.Equals(child.Name, name, StringComparison.OrdinalIgnoreCase))
                    return child;
            }
            return null;
        }

        public List<DirectoryEntry> FindChildrenStartingWith(DirectoryEntry parent, string prefix)
        {
            List<DirectoryEntry> list = new List<DirectoryEntry>();
            if (parent == null) return list;
            foreach (DirectoryEntry child in parent.Children)
            {
                if (child.Name.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                    list.Add(child);
            }
            return list;
        }
    }
}
