using System;
using System.Collections.Generic;
using System.Globalization;

namespace MsgViewer
{
    public enum AppLanguage
    {
        Korean,
        English,
        French,
        Japanese
    }

    public static class Localization
    {
        public const string FixedLanguageButtonText = "언어 / Language / Langue / 言語";

        public static AppLanguage CurrentLanguage { get; set; }

        static Localization()
        {
            CurrentLanguage = DetectLanguage();
        }

        public static AppLanguage DetectLanguage()
        {
            try
            {
                CultureInfo uiCulture = CultureInfo.CurrentUICulture;
                if (uiCulture == null) return AppLanguage.Korean;

                string name = uiCulture.TwoLetterISOLanguageName.ToLowerInvariant();
                switch (name)
                {
                    case "ko": return AppLanguage.Korean;
                    case "en": return AppLanguage.English;
                    case "fr": return AppLanguage.French;
                    case "ja": return AppLanguage.Japanese;
                    default: return AppLanguage.English;
                }
            }
            catch
            {
                return AppLanguage.Korean;
            }
        }

        public static string AppTitle
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "MSG 뷰어";
                    case AppLanguage.English: return "MSG Viewer";
                    case AppLanguage.French: return "Visualiseur MSG";
                    case AppLanguage.Japanese: return "MSG ビューアー";
                    default: return "MSG Viewer";
                }
            }
        }

        public static string Open
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "열기";
                    case AppLanguage.English: return "Open";
                    case AppLanguage.French: return "Ouvrir";
                    case AppLanguage.Japanese: return "開く";
                    default: return "Open";
                }
            }
        }

        public static string Close
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "닫기";
                    case AppLanguage.English: return "Close";
                    case AppLanguage.French: return "Fermer";
                    case AppLanguage.Japanese: return "閉じる";
                    default: return "Close";
                }
            }
        }

        public static string Exit
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "종료";
                    case AppLanguage.English: return "Exit";
                    case AppLanguage.French: return "Quitter";
                    case AppLanguage.Japanese: return "終了";
                    default: return "Exit";
                }
            }
        }

        public static string From
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "보내는 사람";
                    case AppLanguage.English: return "From";
                    case AppLanguage.French: return "De";
                    case AppLanguage.Japanese: return "差出人";
                    default: return "From";
                }
            }
        }

        public static string SentDate
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "전송";
                    case AppLanguage.English: return "Sent";
                    case AppLanguage.French: return "Envoyé";
                    case AppLanguage.Japanese: return "送信日時";
                    default: return "Sent";
                }
            }
        }

        public static string To
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "받는 사람";
                    case AppLanguage.English: return "To";
                    case AppLanguage.French: return "À";
                    case AppLanguage.Japanese: return "宛先";
                    default: return "To";
                }
            }
        }

        public static string Cc
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "참조";
                    case AppLanguage.English: return "Cc";
                    case AppLanguage.French: return "Cc";
                    case AppLanguage.Japanese: return "㏄";
                    default: return "Cc";
                }
            }
        }

        public static string Bcc
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "숨은참조";
                    case AppLanguage.English: return "Bcc";
                    case AppLanguage.French: return "Cci";
                    case AppLanguage.Japanese: return "Bcc";
                    default: return "Bcc";
                }
            }
        }

        public static string Subject
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "제목";
                    case AppLanguage.English: return "Subject";
                    case AppLanguage.French: return "Objet";
                    case AppLanguage.Japanese: return "件名";
                    default: return "Subject";
                }
            }
        }

        public static string AttachmentsHeader
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "첨부파일";
                    case AppLanguage.English: return "Attachments";
                    case AppLanguage.French: return "Pièces jointes";
                    case AppLanguage.Japanese: return "添付ファイル";
                    default: return "Attachments";
                }
            }
        }

        public static string None
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "없음";
                    case AppLanguage.English: return "None";
                    case AppLanguage.French: return "Aucun";
                    case AppLanguage.Japanese: return "なし";
                    default: return "None";
                }
            }
        }

        public static string NoInformation
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "정보 없음";
                    case AppLanguage.English: return "No information";
                    case AppLanguage.French: return "Aucune information";
                    case AppLanguage.Japanese: return "情報なし";
                    default: return "No information";
                }
            }
        }

        public static string OpenFileDialogTitle
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "MSG 파일 열기";
                    case AppLanguage.English: return "Open MSG File";
                    case AppLanguage.French: return "Ouvrir un fichier MSG";
                    case AppLanguage.Japanese: return "MSGファイルを開く";
                    default: return "Open MSG File";
                }
            }
        }

        public static string MsgFileFilter
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "Outlook 메시지 파일 (*.msg)|*.msg|모든 파일 (*.*)|*.*";
                    case AppLanguage.English: return "Outlook Message Files (*.msg)|*.msg|All Files (*.*)|*.*";
                    case AppLanguage.French: return "Fichiers de message Outlook (*.msg)|*.msg|Tous les fichiers (*.*)|*.*";
                    case AppLanguage.Japanese: return "Outlook メッセージ ファイル (*.msg)|*.msg|すべてのファイル (*.*)|*.*";
                    default: return "Outlook Message Files (*.msg)|*.msg|All Files (*.*)|*.*";
                }
            }
        }

        public static string SaveAttachmentTitle
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "다른 이름으로 저장";
                    case AppLanguage.English: return "Save As";
                    case AppLanguage.French: return "Enregistrer sous";
                    case AppLanguage.Japanese: return "名前を付けて保存";
                    default: return "Save As";
                }
            }
        }

        public static string Error
        {
            get
            {
                switch (CurrentLanguage)
                {
                    case AppLanguage.Korean: return "오류";
                    case AppLanguage.English: return "Error";
                    case AppLanguage.French: return "Erreur";
                    case AppLanguage.Japanese: return "エラー";
                    default: return "Error";
                }
            }
        }

        public static string CannotReadMsgFile(string detail)
        {
            switch (CurrentLanguage)
            {
                case AppLanguage.Korean: return string.Format("MSG 파일을 읽을 수 없습니다:\n{0}", detail);
                case AppLanguage.English: return string.Format("Cannot read MSG file:\n{0}", detail);
                case AppLanguage.French: return string.Format("Impossible de lire le fichier MSG :\n{0}", detail);
                case AppLanguage.Japanese: return string.Format("MSGファイルを読み取れません:\n{0}", detail);
                default: return string.Format("Cannot read MSG file:\n{0}", detail);
            }
        }
    }
}
