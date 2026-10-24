<#
.SYNOPSIS
Build, size-check and flash Tesla Head-Up Display on a LILYGO H619.
.EXAMPLE
.\scripts\flash.ps1 COM3
.EXAMPLE
.\scripts\flash.ps1 COM3 -Layout road_scene -NoMonitor
.EXAMPLE
.\scripts\flash.ps1 -Mode display_self_test -DryRun
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$Port,
    [ValidateSet('true_hud', 'road_scene')]
    [string]$Layout = 'true_hud',
    [ValidateSet('normal', 'display_self_test')]
    [string]$Mode = 'normal',
    [ValidateRange(115200, 2000000)]
    [int]$Baud = 460800,
    [string]$IdfPath = $env:IDF_PATH,
    [switch]$NoMonitor,
    [switch]$BuildOnly,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$firmwareDirectory = Join-Path $projectRoot 'firmware'
$buildDirectory = Join-Path $projectRoot "build-flash-h619-$Layout-$Mode"
$sdkconfigPath = Join-Path $buildDirectory 'sdkconfig'
$binaryPath = Join-Path $buildDirectory 'tesla_headup_firmware.bin'
$sizeChecker = Join-Path $PSScriptRoot 'check_firmware_size.py'
$commonArguments = @(
    '-C', $firmwareDirectory, '-B', $buildDirectory,
    '-DIDF_TARGET=esp32s3', '-DTESLA_HEADUP_BOARD_PROFILE=h619',
    "-DTESLA_HEADUP_DASHBOARD_LAYOUT=$Layout",
    "-DTESLA_HEADUP_FIRMWARE_MODE=$Mode", "-DSDKCONFIG=$sdkconfigPath"
)
$flashArguments = @($commonArguments)
if ($Port) { $flashArguments += @('-p', $Port) }
$flashArguments += @('-b', "$Baud", 'flash')
if (-not $NoMonitor) { $flashArguments += 'monitor' }

function Show-Command([string]$Program, [string[]]$CommandArguments) {
    $quoted = @($Program) + @($CommandArguments) | ForEach-Object {
        "'" + $_.Replace("'", "''") + "'"
    }
    Write-Host ('& ' + ($quoted -join ' '))
}

# A dry run has no SDK, USB, filesystem-write or build dependency.
if ($DryRun) {
    Write-Host "LILYGO H619 | $Layout | $Mode | port: $(if ($Port) { $Port } else { 'auto' })"
    Show-Command 'idf.py' ($commonArguments + @('build'))
    Show-Command 'python' @($sizeChecker, $binaryPath, '--max-bytes', '1179648')
    if (-not $BuildOnly) { Show-Command 'idf.py' $flashArguments }
    exit 0
}

function Find-Idf([string]$RequestedPath) {
    if ($RequestedPath) {
        $candidates = @($RequestedPath)
    } else {
        $candidates = @()
        $command = Get-Command 'idf.py' -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command -and $command.Source) {
            $candidates += Split-Path (Split-Path $command.Source -Parent) -Parent
        }
        $candidates += @(
            'C:/Espressif-5/frameworks/esp-idf-v5.4.4',
            'C:/Espressif/frameworks/esp-idf-v5.4.4'
        )
    }
    foreach ($candidate in $candidates) {
        if ((Test-Path -LiteralPath (Join-Path $candidate 'export.ps1')) -and
            (Test-Path -LiteralPath (Join-Path $candidate 'tools/idf.py'))) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'ESP-IDF was not found. Install ESP-IDF 5.4.4, activate its environment, or pass -IdfPath C:\path\to\esp-idf.'
}

function Invoke-Checked([string]$Program, [string[]]$CommandArguments) {
    Show-Command $Program $CommandArguments
    & $Program @CommandArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE."
    }
}

function Find-IdfPython {
    $candidates = @()
    if ($env:IDF_PYTHON_ENV_PATH) {
        $candidates += Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
    }
    $toolsDirectory = if ($env:IDF_TOOLS_PATH) { $env:IDF_TOOLS_PATH } else {
        Join-Path $env:USERPROFILE '.espressif'
    }
    $environmentDirectory = Join-Path $toolsDirectory 'python_env'
    if (Test-Path -LiteralPath $environmentDirectory) {
        # Use the SDK's installed venv, not whichever unrelated Python is on PATH.
        $environments = Get-ChildItem -LiteralPath $environmentDirectory -Directory -Filter 'idf5.4_py*_env' |
            Sort-Object Name -Descending
        foreach ($environment in $environments) {
            $candidates += Join-Path $environment.FullName 'Scripts/python.exe'
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
    }
    throw 'The ESP-IDF 5.4 Python environment was not found. Run the SDK install.ps1 first, or activate its installed environment.'
}

try {
    $resolvedIdf = Find-Idf $IdfPath
    # Espressif's Windows installer keeps tools beside the frameworks folder.
    # Respect an explicitly configured IDF_TOOLS_PATH.
    if (-not $env:IDF_TOOLS_PATH) {
        $installerRoot = Split-Path (Split-Path $resolvedIdf -Parent) -Parent
        if (Test-Path -LiteralPath (Join-Path $installerRoot 'python_env')) {
            $env:IDF_TOOLS_PATH = $installerRoot
        }
    }
    $env:IDF_PATH = $resolvedIdf
    $bootstrapPython = Find-IdfPython
    $pythonScripts = Split-Path $bootstrapPython -Parent
    $env:IDF_PYTHON_ENV_PATH = Split-Path $pythonScripts -Parent
    $env:PATH = $pythonScripts + ';' + $env:PATH
    . (Join-Path $resolvedIdf 'export.ps1')
    if (-not $?) { throw 'ESP-IDF environment activation failed.' }
    $pythonCommand = Get-Command 'python.exe' -ErrorAction Stop | Select-Object -First 1
    $pythonPath = $pythonCommand.Source
    $idfScript = Join-Path $resolvedIdf 'tools/idf.py'

    Write-Host "Building LILYGO H619: $Layout ($Mode)"
    Invoke-Checked $pythonPath (@($idfScript) + $commonArguments + @('build'))
    Invoke-Checked $pythonPath @($sizeChecker, $binaryPath, '--max-bytes', '1179648')
    if (-not $BuildOnly) {
        Invoke-Checked $pythonPath (@($idfScript) + $flashArguments)
    }
} catch {
    Write-Error $_
    exit 1
}
