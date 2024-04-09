# Get all files in the directory
Push-Location -LiteralPath $PSScriptRoot
$files = Get-ChildItem

# Compile each shader with the appropriate extensions
foreach ($file in $files) {
    if ($file.Extension -match "\.(vert|frag|comp|geom|tesc|tese)") {
        Write-Host "Compiling" $file.Name
        $outputFile = $file.Name + ".spv"
        & glslangValidator -gVS -V $file.Name -o $outputFile
    }
}
Pop-Location
