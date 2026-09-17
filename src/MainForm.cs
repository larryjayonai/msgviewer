using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;
using System.Windows.Forms;

namespace MsgViewer
{
    public class MainForm : Form
    {
        // Top Toolbar Controls
        private Panel _topBarPanel;
        private FlowLayoutPanel _leftButtonPanel;
        private FlowLayoutPanel _rightButtonPanel;
        private Button _btnOpen;
        private Button _btnClose;
        private Button _btnLang;
        private Button _btnExit;
        private ContextMenuStrip _langMenu;

        // Header Controls
        private Panel _headerPanel;
        private Label _lblFromTitle;
        private TextBox _txtFrom;
        private Label _lblDateTitle;
        private TextBox _txtDate;
        private Label _lblToTitle;
        private TextBox _txtTo;
        private Label _lblCcTitle;
        private TextBox _txtCc;
        private Label _lblBccTitle;
        private TextBox _txtBcc;
        private Label _lblSubjectTitle;
        private TextBox _txtSubject;

        // Body Controls
        private Panel _bodyPanel;
        private WebBrowser _webBrowser;

        // Attachment Controls
        private Panel _attachContainerPanel;
        private Label _lblAttachSectionTitle;
        private FlowLayoutPanel _attachListPanel;

        // State
        private MsgMessage _currentMsg;
        private string _currentFilePath;
        private List<string> _tempFiles = new List<string>();

        public MainForm()
        {
            InitializeComponent();
            ApplyLanguage();
            ClearMessageView();
        }

        private void InitializeComponent()
        {
            this.Font = new Font("Segoe UI", 9.5f, FontStyle.Regular);
            this.MinimumSize = new Size(680, 520);
            this.Size = new Size(840, 680);
            this.StartPosition = FormStartPosition.CenterScreen;

            // 1. Top Bar
            _topBarPanel = new Panel();
            _topBarPanel.Dock = DockStyle.Top;
            _topBarPanel.Height = 44;
            _topBarPanel.Padding = new Padding(6, 6, 6, 6);
            _topBarPanel.BackColor = SystemColors.Control;

            _leftButtonPanel = new FlowLayoutPanel();
            _leftButtonPanel.Dock = DockStyle.Left;
            _leftButtonPanel.AutoSize = true;
            _leftButtonPanel.FlowDirection = FlowDirection.LeftToRight;
            _leftButtonPanel.WrapContents = false;

            _btnOpen = new Button();
            _btnOpen.AutoSize = true;
            _btnOpen.Height = 30;
            _btnOpen.Padding = new Padding(8, 2, 8, 2);
            _btnOpen.Click += BtnOpen_Click;

            _btnClose = new Button();
            _btnClose.AutoSize = true;
            _btnClose.Height = 30;
            _btnClose.Padding = new Padding(8, 2, 8, 2);
            _btnClose.Click += BtnClose_Click;

            _leftButtonPanel.Controls.Add(_btnOpen);
            _leftButtonPanel.Controls.Add(_btnClose);

            _rightButtonPanel = new FlowLayoutPanel();
            _rightButtonPanel.Dock = DockStyle.Right;
            _rightButtonPanel.AutoSize = true;
            _rightButtonPanel.FlowDirection = FlowDirection.LeftToRight;
            _rightButtonPanel.WrapContents = false;

            _btnLang = new Button();
            _btnLang.AutoSize = true;
            _btnLang.Height = 30;
            _btnLang.Padding = new Padding(8, 2, 8, 2);
            _btnLang.Text = Localization.FixedLanguageButtonText;
            _btnLang.Click += BtnLang_Click;

            _btnExit = new Button();
            _btnExit.AutoSize = true;
            _btnExit.Height = 30;
            _btnExit.Padding = new Padding(8, 2, 8, 2);
            _btnExit.Click += (s, e) => this.Close();

            _rightButtonPanel.Controls.Add(_btnLang);
            _rightButtonPanel.Controls.Add(_btnExit);

            _topBarPanel.Controls.Add(_leftButtonPanel);
            _topBarPanel.Controls.Add(_rightButtonPanel);

            // Language Dropdown Menu
            _langMenu = new ContextMenuStrip();
            ToolStripMenuItem mnuKo = new ToolStripMenuItem("한국어", null, (s, e) => SwitchLanguage(AppLanguage.Korean));
            ToolStripMenuItem mnuEn = new ToolStripMenuItem("English", null, (s, e) => SwitchLanguage(AppLanguage.English));
            ToolStripMenuItem mnuFr = new ToolStripMenuItem("Français", null, (s, e) => SwitchLanguage(AppLanguage.French));
            ToolStripMenuItem mnuJa = new ToolStripMenuItem("日本語", null, (s, e) => SwitchLanguage(AppLanguage.Japanese));
            _langMenu.Items.AddRange(new ToolStripItem[] { mnuKo, mnuEn, mnuFr, mnuJa });

            // 2. Header Panel
            _headerPanel = new Panel();
            _headerPanel.Dock = DockStyle.Top;
            _headerPanel.Height = 150;
            _headerPanel.Padding = new Padding(10, 6, 10, 6);
            _headerPanel.BackColor = Color.FromArgb(248, 249, 250);
            _headerPanel.Paint += (s, e) =>
            {
                using (Pen p = new Pen(Color.FromArgb(218, 220, 224)))
                {
                    e.Graphics.DrawLine(p, 0, _headerPanel.Height - 1, _headerPanel.Width, _headerPanel.Height - 1);
                }
            };

            ContextMenuStrip copyMenu = new ContextMenuStrip();
            ToolStripMenuItem copyItem = new ToolStripMenuItem("복사");
            copyItem.Click += (s, e) =>
            {
                Control c = copyMenu.SourceControl;
                if (c is TextBox)
                {
                    TextBox tb = (TextBox)c;
                    if (!string.IsNullOrEmpty(tb.SelectedText))
                    {
                        Clipboard.SetText(tb.SelectedText);
                    }
                    else if (!string.IsNullOrEmpty(tb.Text))
                    {
                        Clipboard.SetText(tb.Text);
                    }
                }
            };
            copyMenu.Items.Add(copyItem);

            _lblFromTitle = CreateHeaderLabel(0, 8, 90);
            _txtFrom = CreateHeaderTextBox(95, 8, 380, copyMenu);
            _lblDateTitle = CreateHeaderLabel(485, 8, 55);
            _txtDate = CreateHeaderTextBox(545, 8, 200, copyMenu);

            _lblToTitle = CreateHeaderLabel(0, 34, 90);
            _txtTo = CreateHeaderTextBox(95, 34, 650, copyMenu);

            _lblCcTitle = CreateHeaderLabel(0, 60, 90);
            _txtCc = CreateHeaderTextBox(95, 60, 650, copyMenu);

            _lblBccTitle = CreateHeaderLabel(0, 86, 90);
            _txtBcc = CreateHeaderTextBox(95, 86, 650, copyMenu);

            _lblSubjectTitle = CreateHeaderLabel(0, 112, 90);
            _txtSubject = CreateHeaderTextBox(95, 112, 650, copyMenu);
            _txtSubject.Multiline = true;
            _txtSubject.ScrollBars = ScrollBars.Vertical;
            _txtSubject.Height = 32;

            _headerPanel.Controls.AddRange(new Control[] {
                _lblFromTitle, _txtFrom, _lblDateTitle, _txtDate,
                _lblToTitle, _txtTo,
                _lblCcTitle, _txtCc,
                _lblBccTitle, _txtBcc,
                _lblSubjectTitle, _txtSubject
            });

            _headerPanel.Resize += HeaderPanel_Resize;

            // 3. Attachment Panel (Dock Bottom)
            _attachContainerPanel = new Panel();
            _attachContainerPanel.Dock = DockStyle.Bottom;
            _attachContainerPanel.Height = 85;
            _attachContainerPanel.BackColor = Color.FromArgb(248, 249, 250);
            _attachContainerPanel.Padding = new Padding(10, 4, 10, 4);
            _attachContainerPanel.Paint += (s, e) =>
            {
                using (Pen p = new Pen(Color.FromArgb(218, 220, 224)))
                {
                    e.Graphics.DrawLine(p, 0, 0, _attachContainerPanel.Width, 0);
                }
            };

            _lblAttachSectionTitle = new Label();
            _lblAttachSectionTitle.Dock = DockStyle.Top;
            _lblAttachSectionTitle.Height = 22;
            _lblAttachSectionTitle.Font = new Font(this.Font, FontStyle.Bold);
            _lblAttachSectionTitle.ForeColor = Color.FromArgb(60, 64, 67);

            _attachListPanel = new FlowLayoutPanel();
            _attachListPanel.Dock = DockStyle.Fill;
            _attachListPanel.AutoScroll = true;
            _attachListPanel.FlowDirection = FlowDirection.TopDown;
            _attachListPanel.WrapContents = false;

            _attachContainerPanel.Controls.Add(_attachListPanel);
            _attachContainerPanel.Controls.Add(_lblAttachSectionTitle);

            // 4. Body Panel (Fill Center)
            _bodyPanel = new Panel();
            _bodyPanel.Dock = DockStyle.Fill;

            _webBrowser = new WebBrowser();
            _webBrowser.Dock = DockStyle.Fill;
            _webBrowser.AllowWebBrowserDrop = false;
            _webBrowser.IsWebBrowserContextMenuEnabled = true;
            _webBrowser.WebBrowserShortcutsEnabled = true;
            _webBrowser.ScriptErrorsSuppressed = true;
            _webBrowser.Navigating += WebBrowser_Navigating;

            _bodyPanel.Controls.Add(_webBrowser);

            // Add all main panels
            this.Controls.Add(_bodyPanel);
            this.Controls.Add(_attachContainerPanel);
            this.Controls.Add(_headerPanel);
            this.Controls.Add(_topBarPanel);

            this.FormClosing += MainForm_FormClosing;
        }

        private Label CreateHeaderLabel(int x, int y, int width)
        {
            Label lbl = new Label();
            lbl.Location = new Point(x, y);
            lbl.Width = width;
            lbl.Height = 22;
            lbl.Font = new Font(this.Font, FontStyle.Bold);
            lbl.ForeColor = Color.FromArgb(90, 95, 100);
            lbl.TextAlign = ContentAlignment.MiddleRight;
            return lbl;
        }

        private TextBox CreateHeaderTextBox(int x, int y, int width, ContextMenuStrip menu)
        {
            TextBox tb = new TextBox();
            tb.Location = new Point(x, y);
            tb.Width = width;
            tb.Height = 22;
            tb.ReadOnly = true;
            tb.BorderStyle = BorderStyle.None;
            tb.BackColor = Color.FromArgb(248, 249, 250);
            tb.ForeColor = Color.FromArgb(32, 33, 36);
            tb.ContextMenuStrip = menu;
            return tb;
        }

        private void HeaderPanel_Resize(object sender, EventArgs e)
        {
            int w = _headerPanel.ClientSize.Width;
            int rightEdge = w - 16;

            // Date on right
            int dateW = 160;
            int dateX = rightEdge - dateW;
            _txtDate.Location = new Point(dateX, 8);
            _txtDate.Width = dateW;

            int dateTitleW = 55;
            int dateTitleX = dateX - dateTitleW - 4;
            _lblDateTitle.Location = new Point(dateTitleX, 8);
            _lblDateTitle.Width = dateTitleW;

            // From takes space up to date title
            int fromW = Math.Max(100, dateTitleX - 100);
            _txtFrom.Width = fromW;

            // Full-width fields
            int fullW = Math.Max(200, rightEdge - 95);
            _txtTo.Width = fullW;
            _txtCc.Width = fullW;
            _txtBcc.Width = fullW;
            _txtSubject.Width = fullW;
        }

        private void WebBrowser_Navigating(object sender, WebBrowserNavigatingEventArgs e)
        {
            // Block all external web requests completely (OFFLINE ENFORCEMENT)
            string url = e.Url.ToString().ToLowerInvariant();
            if (url == "about:blank" || url.StartsWith("about:") || url.StartsWith("javascript:"))
            {
                return;
            }

            // Cancel any external or file navigation attempt
            e.Cancel = true;
        }

        private void SwitchLanguage(AppLanguage lang)
        {
            Localization.CurrentLanguage = lang;
            ApplyLanguage();
        }

        private void ApplyLanguage()
        {
            this.Text = Localization.AppTitle;

            _btnOpen.Text = Localization.Open;
            _btnClose.Text = Localization.Close;
            _btnExit.Text = Localization.Exit;
            _btnLang.Text = Localization.FixedLanguageButtonText;

            _lblFromTitle.Text = Localization.From + ":";
            _lblDateTitle.Text = Localization.SentDate + ":";
            _lblToTitle.Text = Localization.To + ":";
            _lblCcTitle.Text = Localization.Cc + ":";
            _lblBccTitle.Text = Localization.Bcc + ":";
            _lblSubjectTitle.Text = Localization.Subject + ":";
            _lblAttachSectionTitle.Text = Localization.AttachmentsHeader;

            // Re-render current message attachments view with updated language
            if (_currentMsg != null)
            {
                UpdateAttachmentList();
                if (string.IsNullOrEmpty(_currentMsg.DisplayBcc))
                {
                    _txtBcc.Text = Localization.NoInformation;
                }
                if (!_currentMsg.SentDate.HasValue)
                {
                    _txtDate.Text = Localization.NoInformation;
                }
            }
        }

        private void BtnLang_Click(object sender, EventArgs e)
        {
            _langMenu.Show(_btnLang, new Point(0, _btnLang.Height));
        }

        private void BtnOpen_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog ofd = new OpenFileDialog())
            {
                ofd.Title = Localization.OpenFileDialogTitle;
                ofd.Filter = Localization.MsgFileFilter;
                ofd.RestoreDirectory = true;

                if (ofd.ShowDialog(this) == DialogResult.OK)
                {
                    LoadMsgFile(ofd.FileName);
                }
            }
        }

        private void BtnClose_Click(object sender, EventArgs e)
        {
            CloseCurrentMessage();
        }

        private void LoadMsgFile(string filePath)
        {
            try
            {
                // Clean prior resources
                CleanupTempResources();

                MsgMessage msg = MsgReader.Read(filePath);
                _currentMsg = msg;
                _currentFilePath = filePath;

                DisplayMessage(msg);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, Localization.CannotReadMsgFile(ex.Message), Localization.Error,
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void DisplayMessage(MsgMessage msg)
        {
            _txtFrom.Text = msg.SenderFormatted;
            _txtDate.Text = msg.SentDate.HasValue
                ? msg.SentDate.Value.ToString("yyyy-MM-dd HH:mm:ss")
                : Localization.NoInformation;

            _txtTo.Text = msg.DisplayTo ?? "";
            _txtCc.Text = msg.DisplayCc ?? "";
            _txtBcc.Text = !string.IsNullOrEmpty(msg.DisplayBcc) ? msg.DisplayBcc : Localization.NoInformation;
            _txtSubject.Text = msg.Subject ?? "";

            // Adjust subject height based on length
            if (!string.IsNullOrEmpty(msg.Subject) && msg.Subject.Length > 80)
            {
                _txtSubject.Height = 46;
                _headerPanel.Height = 164;
            }
            else
            {
                _txtSubject.Height = 24;
                _headerPanel.Height = 144;
            }

            // Display Body in WebBrowser
            string html = msg.HtmlBody ?? "";
            _webBrowser.DocumentText = html;

            UpdateAttachmentList();
        }

        private void UpdateAttachmentList()
        {
            _attachListPanel.Controls.Clear();

            List<MsgAttachment> regular = _currentMsg != null ? _currentMsg.RegularAttachments : new List<MsgAttachment>();

            if (regular.Count == 0)
            {
                Label lblNone = new Label();
                lblNone.Text = Localization.None;
                lblNone.ForeColor = Color.FromArgb(120, 125, 130);
                lblNone.AutoSize = true;
                lblNone.Margin = new Padding(4, 2, 4, 2);
                _attachListPanel.Controls.Add(lblNone);
            }
            else
            {
                foreach (MsgAttachment att in regular)
                {
                    LinkLabel lnk = new LinkLabel();
                    lnk.Text = att.FileName;
                    lnk.Tag = att;
                    lnk.AutoSize = true;
                    lnk.Margin = new Padding(4, 3, 4, 3);
                    lnk.LinkColor = Color.FromArgb(26, 115, 232);
                    lnk.ActiveLinkColor = Color.FromArgb(11, 87, 208);
                    lnk.VisitedLinkColor = Color.FromArgb(26, 115, 232);
                    lnk.LinkClicked += AttachmentLink_Clicked;
                    _attachListPanel.Controls.Add(lnk);
                }
            }
        }

        private void AttachmentLink_Clicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            LinkLabel lnk = sender as LinkLabel;
            if (lnk == null || !(lnk.Tag is MsgAttachment)) return;

            MsgAttachment att = (MsgAttachment)lnk.Tag;
            SaveAttachment(att);
        }

        private void SaveAttachment(MsgAttachment att)
        {
            using (SaveFileDialog sfd = new SaveFileDialog())
            {
                sfd.Title = Localization.SaveAttachmentTitle;
                sfd.FileName = att.FileName;

                // Desktop as default per SPEC.md
                sfd.InitialDirectory = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
                sfd.RestoreDirectory = true;

                string ext = Path.GetExtension(att.FileName);
                if (!string.IsNullOrEmpty(ext))
                {
                    sfd.Filter = string.Format("{0} (*{1})|*{1}|모든 파일 (*.*)|*.*", ext.TrimStart('.').ToUpperInvariant(), ext);
                }
                else
                {
                    sfd.Filter = "모든 파일 (*.*)|*.*";
                }

                if (sfd.ShowDialog(this) == DialogResult.OK)
                {
                    try
                    {
                        byte[] bytesToSave = att.Data;
                        if (bytesToSave == null || bytesToSave.Length == 0)
                        {
                            // Empty data
                            bytesToSave = new byte[0];
                        }

                        File.WriteAllBytes(sfd.FileName, bytesToSave);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show(this, ex.Message, Localization.Error, MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
        }

        private void CloseCurrentMessage()
        {
            CleanupTempResources();
            ClearMessageView();
        }

        private void ClearMessageView()
        {
            _currentMsg = null;
            _currentFilePath = null;

            _txtFrom.Text = "";
            _txtDate.Text = "";
            _txtTo.Text = "";
            _txtCc.Text = "";
            _txtBcc.Text = "";
            _txtSubject.Text = "";
            _txtSubject.Height = 24;
            _headerPanel.Height = 144;

            _webBrowser.Navigate("about:blank");
            UpdateAttachmentList();
        }

        private void CleanupTempResources()
        {
            foreach (string file in _tempFiles)
            {
                try
                {
                    if (File.Exists(file))
                    {
                        File.Delete(file);
                    }
                }
                catch { }
            }
            _tempFiles.Clear();
        }

        private void MainForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            CleanupTempResources();
        }
    }
}
