import struct
import os
import olefile

def create_msg(filename, subject, sender_name, sender_email, display_to, display_cc, display_bcc, html_body, attachments=None):
    if attachments is None:
        attachments = []

    streams = {}

    # Top-level properties stream (header 32 bytes)
    # Tag 0x00390040 = ClientSubmitTime (FILETIME)
    filetime_val = 134342874000000000
    prop_header = b'\x00' * 32
    prop_entry_time = struct.pack('<HHIQ', 0x0040, 0x0039, 0, filetime_val)
    streams['__properties_version1.0'] = prop_header + prop_entry_time

    if subject is not None:
        streams['__substg1.0_0037001F'] = (subject + '\0').encode('utf-16le')
    if sender_name is not None:
        streams['__substg1.0_0C1A001F'] = (sender_name + '\0').encode('utf-16le')
    if sender_email is not None:
        streams['__substg1.0_0C1F001F'] = (sender_email + '\0').encode('utf-16le')
    if display_to is not None:
        streams['__substg1.0_0E04001F'] = (display_to + '\0').encode('utf-16le')
    if display_cc is not None:
        streams['__substg1.0_0E03001F'] = (display_cc + '\0').encode('utf-16le')
    if display_bcc is not None:
        streams['__substg1.0_0E02001F'] = (display_bcc + '\0').encode('utf-16le')
    if html_body is not None:
        streams['__substg1.0_10130102'] = html_body.encode('utf-8')

    for i, att in enumerate(attachments):
        prefix = f"__attach_version1.0_#{i:08X}"
        streams[f"{prefix}/__substg1.0_3707001F"] = (att['filename'] + '\0').encode('utf-16le')
        if 'mime' in att:
            streams[f"{prefix}/__substg1.0_370E001F"] = (att['mime'] + '\0').encode('utf-16le')
        if 'cid' in att:
            streams[f"{prefix}/__substg1.0_3716001F"] = (att['cid'] + '\0').encode('utf-16le')
        data_bytes = att.get('data', b'')
        streams[f"{prefix}/__substg1.0_37010102"] = data_bytes
        flags = 4 if att.get('inline', False) else 0
        streams[f"{prefix}/__properties_version1.0"] = b'\x00' * 8 + struct.pack('<HHIQ', 0x0003, 0x3714, 0, flags)

    root_children = []
    for k in streams:
        if '/' not in k:
            root_children.append(k)
    for i in range(len(attachments)):
        root_children.append(f"__attach_version1.0_#{i:08X}")

    dir_entries = []
    dir_entries.append({
        'name': 'Root Entry',
        'type': 5,
        'size': 0,
        'start_sector': 0xFFFFFFFE,
        'left': -1,
        'right': -1,
        'child': 1 if len(root_children) > 0 else -1,
        'path': ''
    })

    child_start_idx = 1
    for idx, cname in enumerate(root_children):
        is_storage = cname.startswith("__attach_")
        right_id = (child_start_idx + idx + 1) if (idx + 1 < len(root_children)) else -1
        dir_entries.append({
            'name': cname,
            'type': 1 if is_storage else 2,
            'size': 0 if is_storage else len(streams[cname]),
            'start_sector': 0xFFFFFFFE,
            'left': -1,
            'right': right_id,
            'child': -1,
            'path': cname
        })

    for i in range(len(attachments)):
        prefix = f"__attach_version1.0_#{i:08X}"
        storage_idx = -1
        for idx, de in enumerate(dir_entries):
            if de['name'] == prefix:
                storage_idx = idx
                break
        att_streams = [k for k in streams if k.startswith(prefix + "/")]
        if att_streams:
            att_child_start = len(dir_entries)
            dir_entries[storage_idx]['child'] = att_child_start
            for s_idx, s_path in enumerate(att_streams):
                s_name = s_path.split('/')[-1]
                right_id = (att_child_start + s_idx + 1) if (s_idx + 1 < len(att_streams)) else -1
                dir_entries.append({
                    'name': s_name,
                    'type': 2,
                    'size': len(streams[s_path]),
                    'start_sector': 0xFFFFFFFE,
                    'left': -1,
                    'right': right_id,
                    'child': -1,
                    'path': s_path
                })

    num_dir_sectors = (len(dir_entries) * 128 + 511) // 512
    dir_start_sector = 1
    data_start_sector = 1 + num_dir_sectors

    current_sector = data_start_sector
    stream_sectors = {}
    sector_data_blocks = []

    for de in dir_entries:
        if de['type'] == 2 and de['size'] > 0:
            data = streams[de['path']]
            de['start_sector'] = current_sector
            num_sec = (len(data) + 511) // 512
            sec_list = []
            for s in range(num_sec):
                sec_id = current_sector + s
                sec_list.append(sec_id)
                chunk = data[s*512 : (s+1)*512]
                if len(chunk) < 512:
                    chunk += b'\x00' * (512 - len(chunk))
                sector_data_blocks.append(chunk)
            stream_sectors[de['path']] = sec_list
            current_sector += num_sec

    total_sectors = current_sector
    fat_size = ((total_sectors + 127) // 128) * 128
    fat = [0xFFFFFFFF] * fat_size
    fat[0] = 0xFFFFFFFD # FATSECT

    for d in range(num_dir_sectors):
        sec = dir_start_sector + d
        if d + 1 < num_dir_sectors:
            fat[sec] = sec + 1
        else:
            fat[sec] = 0xFFFFFFFE

    for path, sec_list in stream_sectors.items():
        for i in range(len(sec_list)):
            sec = sec_list[i]
            if i + 1 < len(sec_list):
                fat[sec] = sec_list[i+1]
            else:
                fat[sec] = 0xFFFFFFFE

    fat_bytes = b''.join(struct.pack('<I', val) for val in fat[:128])

    dir_bytes_list = []
    for de in dir_entries:
        raw_name = (de['name'] + '\0').encode('utf-16le')
        if len(raw_name) > 64:
            raw_name = raw_name[:64]
        name_buf = raw_name + b'\x00' * (64 - len(raw_name))
        name_len = len(raw_name)
        obj_type = de['type']
        color = 1
        left = de['left'] if de['left'] >= 0 else 0xFFFFFFFF
        right = de['right'] if de['right'] >= 0 else 0xFFFFFFFF
        child = de['child'] if de['child'] >= 0 else 0xFFFFFFFF
        clsid = b'\x00' * 16
        flags = 0
        ctime = 0
        mtime = 0
        start_sec = de['start_sector']
        size = de['size']

        entry_buf = name_buf + struct.pack('<HBBIII16sIQQIQ',
            name_len, obj_type, color,
            left, right, child,
            clsid, flags, ctime, mtime,
            start_sec, size)
        dir_bytes_list.append(entry_buf)

    dir_data = b''.join(dir_bytes_list)
    if len(dir_data) < num_dir_sectors * 512:
        dir_data += b'\x00' * (num_dir_sectors * 512 - len(dir_data))

    # Build Header strictly per MS-CFB 76-byte prefix + 436-byte DIFAT
    # fmt_header = '<8s16sHHHHHHLLLLLLLLLL'
    magic = b'\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1'
    clsid = b'\x00' * 16
    minor_ver = 0x003E
    dll_ver = 3 # v3 (512-byte sectors)
    byte_order = 0xFFFE
    sector_shift = 9
    mini_sector_shift = 6
    reserved1 = 0
    reserved2 = 0
    csectDir = 0 # must be 0 for v3
    csectFat = 1 # 1 FAT sector
    sectDirStart = dir_start_sector
    signature = 0
    miniSectorCutoff = 4096
    sectMiniFatStart = 0xFFFFFFFE
    csectMiniFat = 0
    sectDifStart = 0xFFFFFFFE
    csectDif = 0

    header_76 = struct.pack('<8s16sHHHHHHLLLLLLLLLL',
        magic, clsid, minor_ver, dll_ver, byte_order,
        sector_shift, mini_sector_shift, reserved1,
        reserved2,
        csectDir, csectFat, sectDirStart, signature,
        miniSectorCutoff, sectMiniFatStart, csectMiniFat,
        sectDifStart, csectDif)

    # 109 DIFAT entries: entry 0 = sector 0 (FAT), rest = 0xFFFFFFFF
    difat = bytearray(436)
    difat[0:4] = struct.pack('<I', 0)
    for i in range(1, 109):
        difat[i*4 : (i+1)*4] = struct.pack('<I', 0xFFFFFFFF)

    header = header_76 + bytes(difat)

    with open(filename, 'wb') as f:
        f.write(header)
        f.write(fat_bytes)
        f.write(dir_data)
        for block in sector_data_blocks:
            f.write(block)

    print(f"Created {filename} (Size: {os.path.getsize(filename)} bytes)")

# Sample 1: Standard message with table and PDF attachment
create_msg(
    r"C:\Users\LG\Documents\ChatGPT\msgviewer\tests\samples\sample_standard.msg",
    subject="오프라인 MSG 뷰어 구현 진행 보고",
    sender_name="김철수",
    sender_email="chulsoo@example.com",
    display_to="이영희 <younghee@example.com>; 박민수 <minsu@example.com>",
    display_cc="홍길동 <hong@example.com>",
    display_bcc=None,
    html_body="""<!DOCTYPE html>
<html>
<head><meta charset="utf-8"></head>
<body>
<h2>프로젝트 진행 상황 안내</h2>
<p>안녕하세요. 오프라인 MSG 뷰어 개발 현황입니다.</p>
<table border="1" cellpadding="6" style="border-collapse: collapse; width: 80%;">
  <tr style="background-color: #f0f0f0;"><th>구분</th><th>상태</th><th>비고</th></tr>
  <tr><td>단일 x86 EXE 빌드</td><td><b>완료</b></td><td>용량 41 KB</td></tr>
  <tr><td>완전 오프라인 모드</td><td><b>완료</b></td><td>외부 통신 차단</td></tr>
  <tr><td>다국어 지원</td><td><b>완료</b></td><td>ko, en, fr, ja</td></tr>
</table>
<p>상세 내용은 첨부파일을 확인해 주세요.</p>
</body>
</html>""",
    attachments=[
        {'filename': 'architecture_report.pdf', 'mime': 'application/pdf', 'data': b'%PDF-1.4 Mock PDF Content for Acceptance Testing'}
    ]
)

# Sample 2: Inline Image message (cid:) + regular text attachment
png_1x1 = b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f\x15c4\x00\x00\x00\rIDATx\x9cc\xf8\xff\xff?\x00\x05\xfe\x02\xfe\xa7Vk\xca\x00\x00\x00\x00IEND\xaeB`\x82'
create_msg(
    r"C:\Users\LG\Documents\ChatGPT\msgviewer\tests\samples\sample_inline_image.msg",
    subject="사내 뉴스레터 (로고 포함)",
    sender_name="사내홍보팀",
    sender_email="pr@company.com",
    display_to="전직원 <all@company.com>",
    display_cc=None,
    display_bcc=None,
    html_body="""<!DOCTYPE html>
<html>
<head><meta charset="utf-8"></head>
<body>
<p><img src="cid:logo.png" alt="Company Logo"></p>
<h3>이번 주 사내 소식</h3>
<p>로고 이미지는 본문에 삽입되었으므로 첨부파일 목록에서 제외되어야 합니다.</p>
</body>
</html>""",
    attachments=[
        {'filename': 'logo.png', 'mime': 'image/png', 'cid': 'logo.png', 'inline': True, 'data': png_1x1},
        {'filename': 'announcement.txt', 'mime': 'text/plain', 'data': b'Company weekly newsletter text content.'}
    ]
)

# Sample 3: Zero attachments message (should show "없음" / "None")
create_msg(
    r"C:\Users\LG\Documents\ChatGPT\msgviewer\tests\samples\sample_no_attachments.msg",
    subject="회의 일정 공유 (첨부파일 없음)",
    sender_name="팀장님",
    sender_email="leader@company.com",
    display_to="개발팀 <dev@company.com>",
    display_cc="인사팀 <hr@company.com>",
    display_bcc=None,
    html_body="""<!DOCTYPE html>
<html>
<head><meta charset="utf-8"></head>
<body>
<p>내일 오전 10시 주간 회의가 예정되어 있습니다.</p>
<p>첨부파일이 없으므로 하단 첨부파일 영역에 '없음'이 표시되어야 합니다.</p>
</body>
</html>""",
    attachments=[]
)

# Sample 4: Long multi-line subject
create_msg(
    r"C:\Users\LG\Documents\ChatGPT\msgviewer\tests\samples\sample_long_subject.msg",
    subject="[공지] 매우 긴 제목 테스트를 위한 메일 제목입니다. 이 제목은 화면 폭을 초과하여 두 줄 또는 세 줄 이상으로 자연스럽게 자동 줄바꿈되어야 하며, 강제로 말줄임표나 세 줄 제한으로 잘리지 않아야 합니다. (SPEC.md 5절 요구사항 검증)",
    sender_name="시스템 알림",
    sender_email="noreply@service.com",
    display_to="테스터 <test@service.com>",
    display_cc=None,
    display_bcc="보안감사팀 <audit@service.com>",
    html_body="""<!DOCTYPE html>
<html>
<head><meta charset="utf-8"></head>
<body>
<p>제목 자동 줄바꿈 테스트 본문입니다.</p>
</body>
</html>""",
    attachments=[]
)
