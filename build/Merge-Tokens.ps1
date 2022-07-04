<#

.SYNOPSIS
Replace tokens in a file with values.  Originally from https://gist.github.com/niclaslindstedt/8425dbc5db81b779f3f46659f7232f91

.DESCRIPTION
Finds tokens in a given file and replace them with values. It is best used to replace configuration values in a release pipeline.

.PARAMETER InputFile
The file containing the tokens.

.PARAMETER OutputFile
The output file -- if you leave this empty, the input file will be replaced.

.PARAMETER Tokens
A hashtable containing the tokens and the value that should replace them.

.PARAMETER TokenFile
Read tokens as Key=Value pairs from given file. This adds to any tokens specified in Tokens parameter.

.PARAMETER StartTokenPattern
The start of the token, e.g. "{{".

.PARAMETER EndTokenPattern
The end of the token, e.g. "}}".

.PARAMETER NullPattern
The pattern that is used to signify $null. The reason for using this is that
you cannot set an environment variable to null, so instead, set the environment
variable to this pattern, and this script will replace the token with an empty string.

.PARAMETER NoWarning
If this is used, the script will not warn about tokens that cannot be found in the
input file. This is useful when using environment variables to replace tokens since
there will be a lot of warnings that aren't really warnings.

.EXAMPLE
config.template.json:
{
  "url": "{{URL}}",
  "username": "{{USERNAME}}",
  "password": "{{PASSWORD}}"
}

Merge-Tokens `
  -InputFile config.template.json `
  -OutputFile config.json `
  -Tokens @{URL="http://localhost:8080";USERNAME="admin";PASSWORD="Test123"} `
  -StartTokenPattern "{{" `
  -EndTokenPattern "}}"

config.json (result):
{
  "url": "http://localhost:8080",
  "username": "admin",
  "password": "Test123"
}

#>

$PSDefaultParameterValues = @{ 'Out-File:encoding' = 'ASCII' }

function Merge-Tokens {
	[CmdletBinding()]
	param(
		[string]$InputFile,
		[string]$OutputFile = $null,
		[string]$TokenFile = $null,
		[Hashtable]$Tokens = $null,
		[string]$StartTokenPattern = "@",
		[string]$EndTokenPattern = "@",
		[string]$NullPattern = ":::NULL:::",
		[switch]$NoWarning
	)

	function GetTokenCount($line) {
		($line | Select-String -Pattern "$($StartTokenPattern).+?$($EndTokenPattern)" -AllMatches).Matches.Count
	}

	# Check that input exists
	if (-not(Test-Path -Path $InputFile)) {
		Write-Warning "Input file not found: $InputFile"
		Exit 1
	}

	# If the OutputFile is null, we will write to a temporary file
	if ([string]::IsNullOrWhiteSpace($OutputFile)) {
		Write-Verbose "OutputFile was omitted. Replacing InputFile."
		$OutputFile = [System.IO.Path]::GetTempFileName()
		$ReplaceInputFile = $true
	}

	# Empty OutputFile if it already exists
	if (Test-Path -Path $OutputFile) {
		#Write-Verbose "Clearing file $OutputFile"
		#Clear-Content -Encoding UTF8 -Path $OutputFile
		Remove-Item $OutputFile
	}

	if (![string]::IsNullOrWhiteSpace($TokenFile)) {
		(Get-Content $TokenFile) | ForEach-Object {
			$line = $_
			Write-Verbose "Found  $line"
			$kvp = $line -split '='
			if ($kvp.Length -eq 2) {
				$Tokens.Add($kvp[0], $kvp[1])
				Write-Verbose "Found $kvp[0] = $kvp[1]"
			}
		}
	}

	# Go through each line of the InputFile and replace the tokens with their values
	$totalTokens = 0
	$missedTokens = 0
	$usedTokens = New-Object -TypeName "System.Collections.ArrayList"

	#$sw = [System.IO.File]::AppendText($OutputFile)
	# hack to force no-BOM UTF8 on PS v5.x
	" " | Out-File -Encoding ASCII -NoNewline -FilePath $OutputFile
	(Get-Content $InputFile) | ForEach-Object {
		$line = $_
		$totalTokens += GetTokenCount($line)
		foreach ($key in $Tokens.Keys) {
			$token = "$($StartTokenPattern)$($key)$($EndTokenPattern)"
			$value = $Tokens.$key
			if ($line -match $token) {
				$usedTokens.Add($key) | Out-Null
				if ($value -eq $NullPattern) {
					$value = ""
				}
				Write-Verbose "Replacing $token with $value"
				$line = $line -replace "$token", "$value"
			}
		}
		$missedTokens += GetTokenCount($line)
		#$sw.WriteLine($line)
		$line | Out-File -Append -Encoding UTF8 -FilePath $OutputFile
	}

	# If no OutputFile was given, we will replace the InputFile with the temporary file
	if ($ReplaceInputFile) {
		Get-Content -Path $OutputFile | Out-File -FilePath $InputFile -Encoding UTF8
	}

	# Write warning if there were tokens given in the Token parameter which were not replaced
	if (!$NoWarning -and $usedTokens.Count -ne $Tokens.Count) {
		$unusedTokens = New-Object -TypeName "System.Collections.ArrayList"
		foreach ($token in $Tokens.Keys) {
			if (!$usedTokens.Contains($token)) {
				$unusedTokens.Add($token) | Out-Null
			}
		}
		Write-Warning "The following tokens were not used: $($unusedTokens)"
	}

	# Write status message -- warn if there were tokens in the file that were not replaced
	$message = "Processed: $($InputFile) ($($totalTokens - $missedTokens) out of $totalTokens tokens replaced)"
	if ($missedTokens -gt 0) {
		Write-Warning $message
	}
	else {
		Write-Host $message
	}
}
