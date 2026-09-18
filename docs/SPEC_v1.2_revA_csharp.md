# [보존용 아카이브] MSG Viewer v1.2-revA (.NET Framework 4.0 C#) 구현 명세

> **보존 목적**: 추후 .NET 기반으로 유지보수하거나 C# 버전의 재작성이 필요할 때 즉시 활용할 수 있도록 보존한 완결형 명세서입니다.

---

## 1. 아키텍처 개요
- **기술 스택**: C# (.NET Framework 4.0 / CLR 4.0)
- **빌드 도구**: Windows 기본 내장 `C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe` (추가 개발 도구 설치 0%)
- **호환성 범위**:
  - Windows 10의 모든 빌드 (2015년 최초 빌드 10240부터 최신 22H2까지 32비트/64비트 전 에디션) 100% 기본 내장
  - Windows 11 전 에디션 100% 기본 내장
  - .NET 4.8이 미설치된 구형 윈도우 10 32비트 PC에서도 인터넷 다운로드 창 없이 더블 클릭 즉시 구동
  - Windows 8 / 8.1 및 Windows 7 SP1 (.NET 4.0 이상 설치 시)
- **배포 크기**: 약 180 ~ 250 KB (단일 Standalone x86 EXE, 8 MiB 한도의 3% 수준)

---

## 2. 11대 무결점 본문 렌더링 명세
1. **IE11 표준 모드 강제 에뮬레이션 (`IE=Edge`) & 오프라인 리셋 CSS**:
   본문 HTML 헤더에 `<meta http-equiv="X-UA-Compatible" content="IE=Edge">` 및 인라인 오프라인 리셋 CSS 주입.
2. **순수 RTF → HTML 변환 엔진 (`src/RtfConverter.cs`)**:
   `\fromhtml`이 없는 순수 Word/Outlook 서식 메일의 볼드, 이탤릭, 밑줄, 글자색, 배경색, 글머리 기호, 정렬, 표(`\cell`, `\row`)를 온전한 HTML로 파싱/변환.
3. **오프라인 HTML Sanitizer (`src/HtmlSanitizer.cs`)**:
   `<script>`, `<object>`, `<iframe>`, 인라인 이벤트 핸들러 제거 및 닫히지 않은 태그 스택 자동 닫기.
4. **레거시 다국어 인코딩 자동 감지 (`src/EncodingDetector.cs`)**:
   헤더 문자셋 누락/오선언 메일의 UTF-8 유효성 및 CJK 완성형 바이트 패턴 분석을 통한 자동 디코딩.
5. **첨부파일 형식별 Hi-DPI 아이콘**:
   PDF, DOCX, XLSX, PPTX, ZIP, MSG, TXT, IMG 등 전용 16/32px 리소스 제공.
6. **MAPI 코드페이지(`PR_INTERNET_CPID`, `PR_MESSAGE_CODEPAGE`) 우선 파싱**:
   0x3FDE 및 0x3FFD 속성을 추출하여 OS 로캘과 무관하게 원본 코드페이지(949, 932, 65001 등) 적용.
7. **하이브리드 인라인 이미지 리졸버**:
   `cid:`, `Content-Location`, 파일명 3단계 매칭 후 Base64 Data URI 치환 (빨간 X박스 박멸).
8. **프로세스 레벨 `FEATURE_BROWSER_EMULATION` 자동 등록**:
   앱 시작 시 `HKCU\Software\Microsoft\Internet Explorer\Main\FeatureControl\FEATURE_BROWSER_EMULATION`에 `MsgViewer.exe = 11001` 자동 등록.
9. **본문 줌 컨트롤 및 시스템 폰트 스택**:
   `Ctrl + 휠`, `Ctrl +/-/0` 단축키 지원 및 `Segoe UI, "맑은 고딕", sans-serif` 주입.
10. **렌더러 장애 시 무결점 Fallback 안전망**:
    MSHTML 렌더링 실패 시 WinForms `RichTextBox` 보조 뷰어로 즉시 자동 전환.
11. **원격 이미지 오프라인 안전 차단 및 플레이스홀더**:
    외부 HTTP/HTTPS 요청을 100% 원천 차단하고 `[ 🖼️ 외부 이미지 (오프라인 차단됨) ]` 상자 및 툴팁 표시.

---

## 3. 대용량 파일(30~40MB+) 메모리 보호 및 4단계 에러 진단
- **대용량 첨부 지연 스트리밍**: 파싱 시 바이트를 RAM에 올리지 않고, 저장 클릭 시 64KB 청크 단위로 파일에서 디스크로 다이렉트 스트리밍.
- **인라인 이미지 버퍼 즉시 파기**: Base64 변환 즉시 원본 바이트 배열 메모리 해제.
- **4단계 상세 에러 진단**:
  - 1단계 (파일 열기 및 I/O)
  - 2단계 (OLE 복합 파일 구조 분석)
  - 3단계 (MAPI 속성 및 본문 추출)
  - 4단계 (본문 렌더링)
  각 단계별 실패 위치, 오류 유형, 기술 상세, 조치 방법을 4개 언어로 팝업 안내.

---

## 4. 빌드 명령
```powershell
& "C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe" /target:winexe /platform:x86 /optimize+ /out:dist\MsgViewer.exe src\*.cs
```
