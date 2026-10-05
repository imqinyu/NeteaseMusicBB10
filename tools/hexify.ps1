# hexify.ps1 - 把中文字符串转成 NmTr() 需要的 UTF-8 十六进制
$lines = Get-Content -LiteralPath $args[0] -Encoding UTF8
foreach ($s in $lines) {
    if ([string]::IsNullOrWhiteSpace($s)) { continue }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($s)
    $hex = ($bytes | ForEach-Object { $_.ToString('X2') }) -join ''
    Write-Output ($s + ' => ' + $hex)
}
