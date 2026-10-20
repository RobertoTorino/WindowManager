// Stand-in "game" for the demo video (see record-demo.ps1): an animated scene that prints its own client
// size and fps, so every resize the app makes is visible. Starts at WM_DEMO_X / WM_DEMO_Y (physical pixels).
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;

class DemoGame : Form
{
    [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr c);
    struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);

    readonly Timer timer = new Timer();
    readonly DateTime start = DateTime.Now;
    int frames; double fps; DateTime fpsAt = DateTime.Now;

    DemoGame()
    {
        Text = "Demo Game";
        ClientSize = new Size(854, 480);
        StartPosition = FormStartPosition.Manual;
        Location = new Point(EnvInt("WM_DEMO_X", 40), EnvInt("WM_DEMO_Y", 120));
        BackColor = Color.Black;
        DoubleBuffered = true;
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
        timer.Interval = 15;
        timer.Tick += (s, e) => Invalidate();
        timer.Start();
    }

    static int EnvInt(string name, int fallback)
    {
        int v;
        return int.TryParse(Environment.GetEnvironmentVariable(name), out v) ? v : fallback;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        RECT rc; GetClientRect(Handle, out rc);   // the real size: WinForms misses some outside resizes
        int w = Math.Max(1, rc.R - rc.L), h = Math.Max(1, rc.B - rc.T);
        double t = (DateTime.Now - start).TotalSeconds;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        using (var sky = new LinearGradientBrush(new Rectangle(0, 0, w, h), Color.FromArgb(20, 24, 70), Color.FromArgb(230, 110, 60), 90f))
            g.FillRectangle(sky, 0, 0, w, h);

        float sr = h * 0.12f;
        using (var sun = new SolidBrush(Color.FromArgb(255, 220, 120)))
            g.FillEllipse(sun, w * 0.7f - sr, h * 0.55f - sr, sr * 2, sr * 2);

        DrawHills(g, w, h, t * 40, h * 0.68f, h * 0.08f, Color.FromArgb(60, 40, 80));
        DrawHills(g, w, h, t * 110, h * 0.80f, h * 0.06f, Color.FromArgb(25, 20, 40));

        for (int i = 0; i < 6; i++) {
            double p = t * (0.6 + i * 0.15) + i;
            float x = (float)((Math.Sin(p) * 0.4 + 0.5) * w);
            float y = (float)(h * 0.75 - Math.Abs(Math.Sin(p * 2.1)) * h * 0.45);
            float r = h * 0.025f;
            using (var b = new SolidBrush(Color.FromArgb(220, 80 + i * 30, 200, 255 - i * 25)))
                g.FillEllipse(b, x - r, y - r, r * 2, r * 2);
        }

        frames++;
        if ((DateTime.Now - fpsAt).TotalSeconds >= 0.5) {
            fps = frames / (DateTime.Now - fpsAt).TotalSeconds; frames = 0; fpsAt = DateTime.Now;
        }
        float fs = Math.Max(10f, h / 24f);
        using (var font = new Font("Segoe UI", fs, FontStyle.Bold, GraphicsUnit.Pixel))
        using (var shadow = new SolidBrush(Color.FromArgb(160, 0, 0, 0)))
        using (var white = new SolidBrush(Color.White)) {
            string text = "DEMO GAME   " + w + " x " + h + "   " + fps.ToString("0") + " fps";
            g.DrawString(text, font, shadow, fs * 0.8f + 2, fs * 0.6f + 2);
            g.DrawString(text, font, white, fs * 0.8f, fs * 0.6f);
        }
        using (var pen = new Pen(Color.FromArgb(200, 255, 255, 255), 3))
            g.DrawRectangle(pen, 1, 1, w - 3, h - 3);   // shows the edges of the picture
    }

    static void DrawHills(Graphics g, int w, int h, double shift, float baseY, float amp, Color c)
    {
        var pts = new PointF[w / 8 + 3];
        for (int i = 0; i <= w / 8; i++) {
            double x = i * 8;
            pts[i] = new PointF((float)x, (float)(baseY - amp * (Math.Sin((x + shift) / (w * 0.13)) + 0.5 * Math.Sin((x + shift) / (w * 0.05)))));
        }
        pts[w / 8 + 1] = new PointF(w, h);
        pts[w / 8 + 2] = new PointF(0, h);
        using (var b = new SolidBrush(c)) g.FillPolygon(b, pts);
    }

    // Allow any size (the downscaled view makes the window larger than the screen).
    protected override void WndProc(ref Message m)
    {
        base.WndProc(ref m);
        if (m.Msg == 0x24) {   // WM_GETMINMAXINFO: ptMaxTrackSize
            Marshal.WriteInt32(m.LParam, 32, 16384);
            Marshal.WriteInt32(m.LParam, 36, 16384);
        }
    }

    [STAThread]
    static void Main()
    {
        SetProcessDpiAwarenessContext(new IntPtr(-4));   // per monitor: sizes match what the app reports
        Application.EnableVisualStyles();
        Application.Run(new DemoGame());
    }
}
