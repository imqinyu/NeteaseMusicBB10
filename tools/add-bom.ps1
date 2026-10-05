# add-bom.ps1 - 给含中文的 ps1 脚本加 UTF-8 BOM（PowerShell 5.1 无 BOM 按 GBK 读会坏）
$files = @(
    'D:\BBWorkspace\NeteaseMusic\tools\hexify.ps1',
    'D:\BBWorkspace\NeteaseMusic\tools\check-nmtr.ps1',
    'D:\BBWorkspace\NeteaseMusic\tools\port-logic.ps1',
    'D:\BBWorkspace\NeteaseMusic\tests\run-tests.ps1',
    'D:\BBWorkspace\NeteaseMusic\build\fix-destdir.ps1',
    'D:\BBWorkspace\NeteaseMusic\build\clean.ps1'
)
foreach ($f in $files) {
    $t = [IO.File]::ReadAllText($f, (New-Object System.Text.UTF8Encoding($false)))
    [IO.File]::WriteAllText($f, $t, (New-Object System.Text.UTF8Encoding($true)))
    Write-Output ('BOM added: ' + $f)
}
