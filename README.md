# MSG Viewer (오프라인 휴대용 MSG 뷰어) — v1.2

Windows 10/11 x86·x64 환경에서 외부 설치나 인터넷 연결 없이 즉시 실행 가능한 초경량 오프라인 Outlook `.msg` 파일 뷰어입니다.

---

## 주요 기능 및 사양 (버전 1.2)

- **순수 C Win32 Standalone Native 실행 파일 (`v1.2-revB`)**:
  - 파일 크기 **약 182 KB** (목표치 8 MiB 대비 약 2.1% 사용).
  - .NET Framework, VC++ 재배포 패키지(VC++ Runtime / MSVCR / VCRUNTIME) 의존성 **0%**.
  - Windows 10/11 32비트/64비트 전 에디션, 구형 빌드, 경량화 OS, WinPE 및 폐쇄망에서 더블 클릭 즉시 구동.
  - `/LARGEADDRESSAWARE` 지원 (32비트 프로세스 3~4GB 가상 주소 확장).
- **5대 메모리 안전 설계 (누수 0% 달성)**:
  1. **16MB 가상 메모리 아레나 (`VirtualAlloc`/`MEM_RESERVE`)**: 새 메일을 열거나 닫을 때 포인터 리셋(O(1))으로 파편화 및 힙 누수 원천 차단.
  2. **단일 브라우저 인스턴스 재사용**: 문서 전환 시 COM 브라우저 객체를 재성하지 않고 `IPersistStreamInit`을 통해 스트림만 덮어써 COM 누수 방지.
  3. **메모리 매핑 I/O (`CreateFileMappingW`) + 스트리밍 폴백**: 가상 메모리/페이징 파일이 0MB로 설정된 극한 환경에서도 정상 동작.
  4. **대용량 파일(30~40MB+) 64KB 지연 스트리밍**: 첨부파일을 메모리에 일괄 적재하지 않고 저장 시 디스크로 직접 스트리밍 (RAM 점유율 < 25MB).
  5. **단일 정리 경로(`cleanup:`, `SAFE_RELEASE`) 및 안전 문자열 API**.
- **무결점 본문 렌더링 11대 기능**:
  - 프로세스 레벨 IE11 에뮬레이션(`11001`) 자동 등록 및 `<meta http-equiv="X-UA-Compatible" content="IE=Edge">` 주입.
  - 외부 원격 이미지(`http/https`) 오프라인 안전 차단 및 안내 플레이스홀더 제공.
  - 순수 RTF to HTML 파서 내장 (표, 폰트 색상, 서식, 글머리 기호 완벽 복원).
  - 하이브리드 인라인 이미지 리졸버 (`cid:`, 파일명 매칭 후 Base64 Data URI 자동 변환).
  - MAPI 코드페이지(`0x3FDE`) 및 CJK/UTF-8 문자셋 자동 감지.
  - 본문 줌 제어 (`Ctrl + 휠`, `Ctrl +/-/0`).
  - RichEdit 폴백 안전망.
- **4단계 상세 에러 진단 모달**:
  - 메일 열람 실패 시 실패 단계(1단계: I/O, 2단계: CFB 구조, 3단계: MAPI 속성, 4단계: 렌더링), 대상 파일 경로, 오류 유형, 기술 상세, 권장 조치 방법을 4개 언어로 상세 안내.
- **단축키 및 툴바**:
  - `열기 (F2)`: 파일 열기 다이얼로그
  - `닫기 (F4)`: 현재 메일 및 메모리 자원 즉시 정리
  - `언어 / Language / Langue / 言語 (F8)`: 클릭 시 메뉴 팝업, F8 입력 시 한국어 → 영어 → 프랑스어 → 일본어 순환
  - `종료`: 창 닫기 및 `ESC` / `Alt+F4`
- **헤더 6종 정보 및 빈 필드 처리**:
  - 보내는 사람, 전송 일시, 받는 사람, 참조, 숨은참조, 제목 (드래그 복사 가능).
  - 빈 필드는 언어별 `없음`(`None` / `Aucun` / `なし`) 명시.

---

## 빌드 및 테스트

### 1. v1.2-revB (순수 C Win32 Native — 기본 채택)
MSVC x86 컴파일러를 통해 빌드합니다:
```cmd
build_native.bat
```
또는 수동 빌드:
```cmd
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
cl /nologo /O2 /MT /W3 /utf-8 /D_UNICODE /DUNICODE /I src_native src_native\*.c /link /SUBSYSTEM:WINDOWS /LARGEADDRESSAWARE /OPT:REF /OPT:ICF user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib /OUT:dist\MsgViewer.exe
```

단위 및 통합 테스트 실행:
```cmd
cl /nologo /O2 /W3 /utf-8 /Isrc_native /D_CRT_SECURE_NO_WARNINGS tests\test_native.c src_native\arena.c src_native\cfb_reader.c src_native\encoding.c src_native\rtf_decompressor.c src_native\rtf_to_html.c src_native\html_sanitizer.c src_native\localization.c src_native\error_diag.c /link /OUT:tests\TestNative.exe ole32.lib shell32.lib comdlg32.lib user32.lib
tests\TestNative.exe
```

### 2. v1.2-revA (.NET Framework 4.0 C# — 보존용 대안)
추후 유지보수 또는 C# 기반 작업이 필요할 경우 [docs/SPEC_v1.2_revA_csharp.md](file:///docs/SPEC_v1.2_revA_csharp.md) 명세에 따라 언제든지 빌드할 수 있습니다:
```powershell
& "C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe" /target:winexe /platform:x86 /optimize+ /out:dist\MsgViewer.exe src\*.cs
```


