<#
.SYNOPSIS
    Build, test, run or package ClipViewerDesktop with the Qt/MinGW toolchain in C:\Qt.

.EXAMPLE
    .\dev.ps1 build            # configure (first time) and build Debug
    .\dev.ps1 test             # build, then run all tests
    .\dev.ps1 run -- --editor  # build, then run the app with arguments
    .\dev.ps1 dist -Config release   # Release build + deployable folder in dist\
#>
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'test', 'run', 'dist', 'clean')]
    [string] $Command = 'build',

    [ValidateSet('debug', 'release')]
    [string] $Config = 'debug',

    [string] $QtRoot = $(if ($env:QT_ROOT) { $env:QT_ROOT } else { 'C:\Qt\6.10.3\mingw_64' }),
    [string] $MingwRoot = $(if ($env:MINGW_ROOT) { $env:MINGW_ROOT } else { 'C:\Qt\Tools\mingw1310_64' }),

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $AppArgs
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$buildDir = Join-Path $root "build\$Config"

# cmake/ninja may come from pip (--user), which isn't always on PATH.
$pipScripts = & python -c "import sysconfig; print(sysconfig.get_path('scripts', 'nt_user'))" 2>$null
$env:PATH = "$QtRoot\bin;$MingwRoot\bin;$pipScripts;$env:PATH"

function Invoke-Checked([scriptblock] $Block) {
    & $Block
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Build {
    if (-not (Test-Path (Join-Path $buildDir 'build.ninja'))) {
        Invoke-Checked { cmake --preset $Config }
    }
    Invoke-Checked { cmake --build $buildDir }
}

switch ($Command) {
    'build' { Build }
    'test' {
        Build
        Invoke-Checked { ctest --test-dir $buildDir --output-on-failure }
    }
    'run' {
        Build
        $appArgs = @($AppArgs | Where-Object { $_ -ne '--' })
        & (Join-Path $buildDir 'ClipViewerDesktop.exe') @appArgs
    }
    'dist' {
        Build
        $dist = Join-Path $root 'dist'
        if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
        Invoke-Checked { cmake --install $buildDir --prefix $dist }
        Write-Host "Deployed to $dist\bin"
    }
    'clean' {
        if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
    }
}
