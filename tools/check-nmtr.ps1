# check-nmtr.ps1 - 校验源码里所有 NmTr("hex") 的十六进制能否正确解码
# 用法：powershell -File tools/check-nmtr.ps1 <目录或文件>...
$ErrorActionPreference = 'Stop'

$targets = @()
foreach ($a in $args) {
    if (Test-Path $a -PathType Container) {
        $targets += Get-ChildItem -LiteralPath $a -Recurse -Include *.hpp,*.cpp -File | Select-Object -ExpandProperty FullName
    } else {
        $targets += (Resolve-Path $a).Path
    }
}

$bad = 0
$rx = [regex]'NmTr\("([0-9A-Fa-f]+)"\)'
foreach ($f in $targets) {
    $text = [IO.File]::ReadAllText($f)
    foreach ($m in $rx.Matches($text)) {
        $hex = $m.Groups[1].Value
        try {
            $bytes = New-Object byte[] ($hex.Length / 2)
            for ($i = 0; $i -lt $bytes.Length; $i++) {
                $bytes[$i] = [Convert]::ToByte($hex.Substring($i * 2, 2), 16)
            }
            $decoded = [System.Text.Encoding]::UTF8.GetString($bytes)
            # 严格校验：重新编码应与原字节一致（防止无效序列被替换）
            $reenc = [System.Text.Encoding]::UTF8.GetBytes($decoded)
            if ($reenc.Length -ne $bytes.Length) { throw 'roundtrip mismatch' }
            $line = ($text.Substring(0, $m.Index) -split "`n").Count
            Write-Output ("{0}:{1}: {2}" -f (Split-Path $f -Leaf), $line, $decoded)
        } catch {
            $bad++
            Write-Output ("BAD HEX in {0}: {1}" -f $f, $hex)
        }
    }
}
if ($bad -gt 0) { exit 1 }
exit 0
