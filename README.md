# MSG Viewer (오프라인 휴대용 MSG 뷰어) — v1.1

Windows 10/11 x86·x64 환경에서 외부 설치나 인터넷 연결 없이 즉시 실행 가능한 초경량 오프라인 Outlook `.msg` 파일 뷰어입니다.

## 주요 기능 및 사양 (버전 1.1)
- **단일 x86 Standalone EXE**: 파일 크기 약 42KB (목표치 7~8MB 대비 0.5% 미만).
- **단축키 및 툴바**:
  - `열기 (F2)`: `F2` 키로 파일 열기 다이얼로그 호출
  - `닫기 (F4)`: `F4` 키로 현재 메일 및 임시자원 정리 후 빈 화면 전환
  - `언어 / Language / Langue / 言語 (F8)`:
    - `F8` 키: 한국어 → English → Français → 日本語 순으로 즉시 순환(Cycle) 전환
    - 마우스 클릭: 4개 언어 드롭다운 메뉴 팝업 (닫힘 시 버튼 눌림 상태 정상 복원)
  - `종료`: 창 닫기 및 `Alt+F4`
- **헤더 6종 정보 및 빈 필드 처리**:
  - 보내는 사람, 전송 일시(`YYYY-MM-DD HH:mm:ss`), 받는 사람, 참조, 숨은참조, 제목
  - 받는 사람 또는 제목이 비어 있는 경우 첨부파일 없음과 동일하게 언어별 `없음`(`None` / `Aucun` / `なし`)으로 명시 표기
- **본문 및 첨부파일**:
  - HTML, 일반 텍스트, 압축 RTF 서식·표 및 인라인 이미지 완벽 렌더링
  - 본문 인라인 이미지는 첨부 목록에서 제외되며, 일반 첨부파일은 클릭 시 바탕화면 기본 저장 지원
- **완전 오프라인 격리**: 외부 인터넷 요청 및 원격 연결을 원천 차단.

## 빌드 및 테스트
Windows 기본 내장 .NET Framework 4.8의 `csc.exe`를 사용하여 빌드합니다:
```powershell
# 배포용 바이너리 빌드
& "C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe" /target:winexe /platform:x86 /optimize+ /out:dist\MsgViewer.exe src\*.cs

# 단위 및 인수 테스트 실행
& "C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe" /target:exe /platform:x86 /optimize+ /out:tests\TestRunner.exe src\CompoundFile.cs src\RtfDecompressor.cs src\MsgReader.cs src\Localization.cs tests\TestRunner.cs
.\tests\TestRunner.exe
```

