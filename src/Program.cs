using System;
using System.IO;
using System.Windows.Forms;

namespace MsgViewer
{
    static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            MainForm form = new MainForm();
            if (args != null && args.Length > 0 && !string.IsNullOrEmpty(args[0]) && File.Exists(args[0]))
            {
                // Reflection or public method to load initial file if provided
                try
                {
                    System.Reflection.MethodInfo mi = typeof(MainForm).GetMethod("LoadMsgFile",
                        System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
                    if (mi != null)
                    {
                        mi.Invoke(form, new object[] { args[0] });
                    }
                }
                catch { }
            }

            Application.Run(form);
        }
    }
}
