param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\assets\audio\narrator'),
    [string]$ClipName = ''
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech
[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

$voiceName = 'Microsoft Zira Desktop'
$clips = [ordered]@{
    'briefing' = 'Storm surge has split the catchment. Secure the west gauge, then the east and south. Restore the rescue beacon.'
    'west_task' = 'First task: clear the salvage crew from the west gauge, then press E to turn the wheel.'
    'after_west' = 'Western gauge is online. Water is rising. Move east and secure the second wheel.'
    'east_task' = 'Second task: clear the crew at the east gauge, then press E to turn its wheel.'
    'after_east' = 'Eastern gauge is online. Water is falling. Continue south and secure the final gauge.'
    'south_task' = 'Third task: clear the crew at the south gauge, then press E to turn its wheel.'
    'after_south' = 'All three gauges are online. Reach the rescue beacon and transmit the evacuation signal.'
    'beacon_task' = 'Final task: reach the rescue beacon and press E to transmit the evacuation signal.'
    'beacon_sent' = 'Beacon transmitting. Search and rescue is inbound. Hold the coast.'
    'player_down' = 'You are down. The team is returning you to the safe position. Regroup and try again.'
    'contested_gauge' = 'The salvage crew is holding the wheel. Clear the area before you operate the gauge.'
    'pickup_controls' = 'Four wheel drive is ready. W and S control throttle and braking; A and D steer. Press C for cockpit or chase view.'
    'plane_controls' = 'Power up with W; pitch and bank to climb and turn. Reduce power and align with the runway to land.'
    'helicopter_controls' = 'Collective lifts and descends. WASD controls cyclic movement. Land before you exit. C changes views.'
    'slow_before_exit' = 'Reduce speed before you leave the pickup.'
    'land_before_exit' = 'Bring the aircraft down and land before you exit.'
}

foreach ($entry in $clips.GetEnumerator()) {
    if ($ClipName -and $entry.Key -ne $ClipName) { continue }
    $synth = [System.Speech.Synthesis.SpeechSynthesizer]::new()
    try {
        $synth.SelectVoiceByHints([System.Speech.Synthesis.VoiceGender]::Female,[System.Speech.Synthesis.VoiceAge]::Adult,0,[System.Globalization.CultureInfo]::GetCultureInfo('en-US'))
        if ($synth.Voice.Name -ne $voiceName) { throw "Expected $voiceName, selected $($synth.Voice.Name)." }
        $synth.Rate = -1
        $synth.Volume = 88
        $path = Join-Path $OutputDirectory ($entry.Key + '.wav')
        $synth.SetOutputToWaveFile($path)
        $escaped = [System.Security.SecurityElement]::Escape($entry.Value)
        $ssml = "<speak version='1.0' xml:lang='en-US'><prosody rate='-5%'>$escaped</prosody></speak>"
        $synth.SpeakSsml($ssml)
    }
    finally { $synth.Dispose() }
}

Get-ChildItem -LiteralPath $OutputDirectory -Filter '*.wav' | Sort-Object Name | Select-Object Name,Length
