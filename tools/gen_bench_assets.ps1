# Тяжёлые ассеты для замеров загрузки: assets/bench/tex_XX.png и mesh_XX.obj
# Запуск из корня проекта:  powershell -ExecutionPolicy Bypass -File tools\gen_bench_assets.ps1
param(
    [string]$OutDir = (Join-Path $PSScriptRoot "..\assets\bench"),
    [int]$TextureCount = 24,
    [int]$TextureSize = 2048,
    [int]$MeshCount = 8,
    [int]$MeshGrid = 400
)

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

public static class BenchGen {
    // PNG RGBA: R - градиент, G - шум (плохо сжимается), B - узор, A - 255
    public static void WritePng(string path, int size, int seed) {
        var rnd = new Random(seed);
        var bmp = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        var data = bmp.LockBits(new Rectangle(0, 0, size, size), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        var row = new byte[size * 4];
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int i = x * 4;                       // порядок байт в памяти: B, G, R, A
                row[i + 0] = (byte)((x ^ y) & 0xFF);
                row[i + 1] = (byte)rnd.Next(256);
                row[i + 2] = (byte)((x + y + seed * 17) & 0xFF);
                row[i + 3] = 255;
            }
            Marshal.Copy(row, 0, data.Scan0 + y * data.Stride, row.Length);
        }
        bmp.UnlockBits(data);
        bmp.Save(path, ImageFormat.Png);
        bmp.Dispose();
    }

    // OBJ: плоская сетка n x n вершин, по 2 треугольника на клетку
    public static void WriteObj(string path, int n) {
        var sb = new StringBuilder(16 * 1024 * 1024);
        var ci = System.Globalization.CultureInfo.InvariantCulture;   // точка, а не запятая
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++)
                sb.Append("v ").Append(((double)i / n).ToString("F5", ci)).Append(' ')
                  .Append(((double)j / n).ToString("F5", ci)).Append(" 0\n");
        for (int j = 0; j < n; j++)
            for (int i = 0; i < n; i++)
                sb.Append("vt ").Append(((double)i / n).ToString("F5", ci)).Append(' ')
                  .Append(((double)j / n).ToString("F5", ci)).Append('\n');
        sb.Append("vn 0 0 1\n");
        for (int j = 0; j < n - 1; j++) {
            for (int i = 0; i < n - 1; i++) {
                int a = j * n + i + 1, b = a + 1, c = a + n, d = a + n + 1;   // в OBJ индексы с 1
                sb.Append("f ").Append(a).Append('/').Append(a).Append("/1 ")
                  .Append(b).Append('/').Append(b).Append("/1 ")
                  .Append(d).Append('/').Append(d).Append("/1\n");
                sb.Append("f ").Append(a).Append('/').Append(a).Append("/1 ")
                  .Append(d).Append('/').Append(d).Append("/1 ")
                  .Append(c).Append('/').Append(c).Append("/1\n");
            }
        }
        File.WriteAllText(path, sb.ToString(), new UTF8Encoding(false));
    }
}
"@

New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

for ($i = 0; $i -lt $TextureCount; $i++) {
    $file = Join-Path $OutDir ("tex_{0:D2}.png" -f $i)
    [BenchGen]::WritePng($file, $TextureSize, $i)
    Write-Host ("texture {0}/{1}" -f ($i + 1), $TextureCount)
}
for ($i = 0; $i -lt $MeshCount; $i++) {
    $file = Join-Path $OutDir ("mesh_{0:D2}.obj" -f $i)
    [BenchGen]::WriteObj($file, $MeshGrid)
    Write-Host ("mesh {0}/{1}" -f ($i + 1), $MeshCount)
}
Write-Host "Done: $OutDir"
