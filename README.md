# MSG Viewer — Antigravity 이어받기

이 폴더에는 오프라인 MSG 뷰어의 확정 명세와 수정·검토 이력이 저장되어 있습니다. **현재 단계는 명세 완료, 구현 시작 전**입니다.

1. [ANTIGRAVITY_HANDOFF.md](ANTIGRAVITY_HANDOFF.md)에서 진행 상태와 확정 요구사항을 확인합니다.
2. 기준 문서인 [seed.yaml](seed.yaml)과 [SPEC.md](SPEC.md)를 읽습니다.
3. Antigravity에서 이 폴더를 열고 [ANTIGRAVITY_PROMPT.txt](ANTIGRAVITY_PROMPT.txt)의 내용을 첫 메시지로 사용합니다.

설치된 `agy` CLI를 사용하는 경우 이 폴더의 PowerShell에서 실행할 수 있습니다.

```powershell
.\Start-Antigravity.ps1
```

스크립트 실행 정책 때문에 실행되지 않으면 정책을 바꾸지 않고 다음 명령을 사용할 수 있습니다.

```powershell
Set-Location -LiteralPath 'C:\Users\LG\Documents\ChatGPT\msgviewer'
& "$env:LOCALAPPDATA\agy\bin\agy.exe" --prompt-interactive ([System.IO.File]::ReadAllText((Join-Path (Get-Location).Path 'ANTIGRAVITY_PROMPT.txt')))
```

이 명령은 저장한 인계 프롬프트로 Antigravity의 새 대화를 시작합니다. 존재하지 않는 과거 Antigravity 대화 ID를 재개하는 명령이 아닙니다. 모델·권한 설정은 사용자의 기존 기본값을 유지합니다.

`docs/`에는 수정 기록·명세 QA·스키마 검증 결과가 있습니다. 실제 앱의 테스트 결과는 아직 없으며 향후 `reports/`에 기록합니다. `handoff/`에는 인계 시점 상태, 파일 해시 목록과 백업 ZIP이 저장됩니다.
