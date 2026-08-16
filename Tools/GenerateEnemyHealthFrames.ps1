Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = 'Stop'

function New-ArgbColor([int]$A, [int]$R, [int]$G, [int]$B) {
    return [System.Drawing.Color]::FromArgb($A, $R, $G, $B)
}

function New-Brush([System.Drawing.Color]$Color) {
    return New-Object System.Drawing.SolidBrush($Color)
}

function New-Pen([System.Drawing.Color]$Color, [float]$Width) {
    $Pen = New-Object System.Drawing.Pen($Color, $Width)
    $Pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $Pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $Pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    return $Pen
}

function Draw-Line($Graphics, [System.Drawing.Color]$Color, [float]$Width, [float]$X1, [float]$Y1, [float]$X2, [float]$Y2) {
    $Pen = New-Pen $Color $Width
    try {
        $Graphics.DrawLine($Pen, $X1, $Y1, $X2, $Y2)
    }
    finally {
        $Pen.Dispose()
    }
}

function Draw-Diamond($Graphics, [float]$CenterX, [float]$CenterY, [float]$RadiusX, [float]$RadiusY, [System.Drawing.Color]$FillColor, [System.Drawing.Color]$LineColor, [float]$LineWidth) {
    $Points = [System.Drawing.PointF[]]@(
        [System.Drawing.PointF]::new($CenterX, $CenterY - $RadiusY),
        [System.Drawing.PointF]::new($CenterX + $RadiusX, $CenterY),
        [System.Drawing.PointF]::new($CenterX, $CenterY + $RadiusY),
        [System.Drawing.PointF]::new($CenterX - $RadiusX, $CenterY)
    )
    $Path = New-Object System.Drawing.Drawing2D.GraphicsPath
    try {
        $Path.AddPolygon($Points)
        $Brush = New-Brush $FillColor
        $Pen = New-Pen $LineColor $LineWidth
        try {
            $Graphics.FillPath($Brush, $Path)
            $Graphics.DrawPath($Pen, $Path)
        }
        finally {
            $Brush.Dispose()
            $Pen.Dispose()
        }
    }
    finally {
        $Path.Dispose()
    }
}

function Draw-DryBrush($Graphics, [System.Random]$Random, [float]$X, [float]$Y, [float]$Direction, [float]$Spread, [float]$Length, [int]$Count, [int]$Alpha) {
    for ($Index = 0; $Index -lt $Count; $Index++) {
        $OffsetY = ($Random.NextDouble() * 2.0 - 1.0) * $Spread
        $StartX = $X + ($Random.NextDouble() * 0.35 * $Length) * $Direction
        $EndX = $StartX + $Length * (0.65 + $Random.NextDouble() * 0.35) * $Direction
        $StartY = $Y + $OffsetY
        $EndY = $StartY + (($Random.NextDouble() * 2.0 - 1.0) * $Spread * 0.28)
        $StrokeAlpha = [Math]::Max(28, [Math]::Min(220, $Alpha - $Random.Next(0, 70)))
        $StrokeColor = New-ArgbColor $StrokeAlpha 36 35 33
        $StrokeWidth = 1.0 + $Random.NextDouble() * 2.2
        Draw-Line $Graphics $StrokeColor $StrokeWidth $StartX $StartY $EndX $EndY
    }

    $DotBrush = New-Brush (New-ArgbColor ([Math]::Max(25, $Alpha - 80)) 36 35 33)
    try {
        for ($Index = 0; $Index -lt [Math]::Max(2, [int]($Count * 0.7)); $Index++) {
            $DotX = $X + ($Random.NextDouble() * $Length * 1.1) * $Direction
            $DotY = $Y + ($Random.NextDouble() * 2.0 - 1.0) * $Spread * 1.15
            $DotRadius = 0.8 + $Random.NextDouble() * 2.0
            $Graphics.FillEllipse($DotBrush, $DotX - $DotRadius, $DotY - $DotRadius, $DotRadius * 2.0, $DotRadius * 2.0)
        }
    }
    finally {
        $DotBrush.Dispose()
    }
}

function Save-NormalFrame([string]$OutputPath) {
    $Width = 800
    $Height = 80
    $Bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    $Random = New-Object System.Random(3101)
    try {
        $Graphics.Clear([System.Drawing.Color]::Transparent)
        $Graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $Graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
        $Graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $WarmLine = New-ArgbColor 176 112 109 102
        $DarkLine = New-ArgbColor 224 36 35 33
        $SoftLine = New-ArgbColor 112 55 53 49

        # Source is 8x the intended 100x10 render size. The center stays empty
        # so the runtime health layers remain visible beneath this frame.
        Draw-Line $Graphics $SoftLine 4.0 76 26 724 26
        Draw-Line $Graphics $WarmLine 3.0 84 54 716 54
        Draw-Line $Graphics $DarkLine 2.0 66 40 734 40
        Draw-Line $Graphics $SoftLine 2.0 70 58 730 58

        Draw-Diamond $Graphics 50 40 27 28 (New-ArgbColor 218 42 42 40) $WarmLine 3.0
        Draw-Diamond $Graphics 750 40 27 28 (New-ArgbColor 218 42 42 40) $WarmLine 3.0
        Draw-DryBrush $Graphics $Random 54 31 1.0 9.0 46.0 6 145
        Draw-DryBrush $Graphics $Random 746 49 -1.0 8.0 42.0 5 128

        $Directory = Split-Path -Parent $OutputPath
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
        $Bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $Graphics.Dispose()
        $Bitmap.Dispose()
    }
}

function Save-EliteFrame([string]$OutputPath) {
    $Width = 1040
    $Height = 128
    $Bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    $Random = New-Object System.Random(3102)
    try {
        $Graphics.Clear([System.Drawing.Color]::Transparent)
        $Graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $Graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
        $Graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $FrameDark = New-ArgbColor 232 35 35 33
        $FrameWarm = New-ArgbColor 194 126 123 114
        $FrameSoft = New-ArgbColor 126 73 70 65
        $Cinnabar = New-ArgbColor 242 188 64 36

        # Elite keeps the same horizontal fill semantics but adds a restrained
        # double frame, larger left identity diamond, and tapered right end.
        Draw-Line $Graphics $FrameWarm 4.0 122 28 910 28
        Draw-Line $Graphics $FrameDark 5.0 112 42 925 42
        Draw-Line $Graphics $FrameDark 5.0 112 86 925 86
        Draw-Line $Graphics $FrameWarm 4.0 122 100 910 100
        Draw-Line $Graphics $FrameSoft 2.0 130 55 910 55
        Draw-Line $Graphics $FrameSoft 2.0 130 73 910 73

        Draw-Diamond $Graphics 82 64 55 55 (New-ArgbColor 230 39 39 37) $FrameWarm 4.0
        # Small fixed cinnabar mark: identity accent only, never a whole red fill.
        Draw-Line $Graphics $Cinnabar 12.0 70 56 89 74
        Draw-Line $Graphics $Cinnabar 7.0 89 52 96 64

        # A sharp, sparse taper keeps the right end readable at 130x16.
        Draw-Line $Graphics $FrameDark 6.0 910 42 1000 64
        Draw-Line $Graphics $FrameDark 6.0 910 86 1000 64
        Draw-Line $Graphics $FrameWarm 3.0 927 51 994 64
        Draw-Line $Graphics $FrameWarm 3.0 927 77 994 64

        Draw-DryBrush $Graphics $Random 106 36 1.0 16.0 62.0 10 152
        Draw-DryBrush $Graphics $Random 916 91 -1.0 14.0 68.0 9 140

        $Directory = Split-Path -Parent $OutputPath
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
        $Bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $Graphics.Dispose()
        $Bitmap.Dispose()
    }
}

$TextureDirectory = Join-Path $PSScriptRoot '..\Content\RawContent\UI\Health\Textures'
$TextureDirectory = [System.IO.Path]::GetFullPath($TextureDirectory)
Save-NormalFrame (Join-Path $TextureDirectory 'T_UI_EnemyHealth_Normal_Frame.png')
Save-EliteFrame (Join-Path $TextureDirectory 'T_UI_EnemyHealth_Elite_Frame.png')
Write-Output "Generated enemy health frame textures in $TextureDirectory"
