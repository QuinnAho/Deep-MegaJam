# Builds the FX lane's Niagara systems through the editor's NiagaraToolset (MCP), because Python cannot reach the
# Niagara stack in this engine. Start an editor first (`tools\ue.ps1 editor -Port 8012`), then run:
#   powershell -File deep\Content\Deep\FX\build_fx_systems.ps1 -Port 8012
# Materials come first from build_fx_materials.py (headless). Recipe and sources: devlog, Nov 15 "bubbles with Niagara".
#   NS_Bubbles  : from Epic's DirectionalBurst template system; one CPU sprite emitter in WORLD space (fundamentals:
#                 "attachment controls spawn origin, space setting controls post-spawn behavior"; bubbles must stay
#                 in the water where they were breathed out, not ride the diver). A burst of User.Count, buoyancy
#                 as an upward "gravity", linear drag for a terminal rise speed, curl noise for the wobble, a size
#                 that grows a little as they rise, alpha fade from the template. M_Bubble on the sprite renderer.
#   NS_SiltPuff : the same template; a slow low ring that spreads, hangs and settles. M_SiltPuff.
param([int]$Port = 8012, [string]$Only = '')
$ErrorActionPreference = 'Stop'
$Mcp = Join-Path $PSScriptRoot '..\..\..\..\tools\mcp.ps1'
$Template = '/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst'
$E = 'DirectionalBurst'

function Call([string]$tool, [string]$json) {
    $out = & $Mcp call ('NiagaraToolsets.NiagaraToolset_System.' + $tool) $json -Port $Port -MaxChars 400
    if ($LASTEXITCODE -ne 0) { Write-Host ('FAIL ' + $tool + ' ' + $json); Write-Host $out } else { Write-Host ('ok   ' + $tool) }
}
function Ref([string]$sys, [string]$script, [string]$module, [string[]]$stack) {
    $o = @{ system = @{ refPath = $sys }; emitterName = $E }
    if ($script) { $o.scriptName = $script }
    if ($module) { $o.moduleName = $module }
    if ($stack) { $o.inputNameStack = @($stack) }
    return $o
}
function SetIn($sys, $script, $module, [string[]]$stack, [string]$struct, $value) {
    $body = @{ stackInputRef = (Ref $sys $script $module $stack); inputData = @{ struct = @{ refPath = $struct }; value = $value } }
    Call 'SetStackInputData' ($body | ConvertTo-Json -Depth 12 -Compress)
}
function F($sys, $script, $module, [string[]]$stack, [double]$v) { SetIn $sys $script $module $stack '/Script/Niagara.NiagaraFloat' @{ value = $v } }
function I($sys, $script, $module, [string[]]$stack, [int]$v) { SetIn $sys $script $module $stack '/Script/Niagara.NiagaraInt32' @{ value = $v } }
function V3($sys, $script, $module, [string[]]$stack, $x, $y, $z) { SetIn $sys $script $module $stack '/Script/CoreUObject.Vector3f' @{ x = $x; y = $y; z = $z } }
function V2($sys, $script, $module, [string[]]$stack, $x, $y) { SetIn $sys $script $module $stack '/Script/CoreUObject.Vector2f' @{ x = $x; y = $y } }
function En($sys, $script, $module, [string[]]$stack, [string]$enumPath, [string]$name) {
    SetIn $sys $script $module $stack '/Script/NiagaraEditor.NiagaraExt_StackInputData_Enum' @{ enum = @{ refPath = $enumPath }; enumName = $name }
}
function Linked($sys, $script, $module, [string[]]$stack, [string]$var, [string]$type) {
    SetIn $sys $script $module $stack '/Script/NiagaraEditor.NiagaraExt_StackInputData_Linked' @{ linkedVariable = @{ name = $var; type = @{ classStructOrEnum = @{ refPath = $type }; bIsStatic = $false } } }
}
function AddMod($sys, [string]$script, [string]$before, [string]$asset) {
    $body = @{ moduleLocationRef = (Ref $sys $script $before $null); moduleAsset = @{ refPath = $asset } }
    Call 'AddModule' ($body | ConvertTo-Json -Depth 8 -Compress)
}
function RemoveMod($sys, [string]$script, [string]$module) {
    Call 'RemoveModule' ((@{ moduleRef = (Ref $sys $script $module $null) }) | ConvertTo-Json -Depth 8 -Compress)
}
function NewSystem([string]$name) {
    $sys = '/Game/Deep/FX/' + $name + '.' + $name
    Call 'CreateNiagaraSystem' ((@{ assetName = $name; assetPath = '/Game/Deep/FX'; templateSystem = @{ refPath = $Template } }) | ConvertTo-Json -Compress)
    Call 'RemoveEmitter' ((@{ emitterRef = @{ system = @{ refPath = $sys }; emitterName = 'LocationBasedRibbon' } }) | ConvertTo-Json -Depth 5 -Compress)
    RemoveMod $sys 'ParticleUpdateScript' 'GenerateLocationEvent'
    RemoveMod $sys 'ParticleUpdateScript' 'ScaleSpriteSizeBySpeed'
    return $sys
}
function UserVar([string]$name, [string]$type, $value) { return @{ name = $name; type = @{ classStructOrEnum = @{ refPath = $type }; bIsStatic = $false }; defaultValue = @{ struct = @{ refPath = $type }; value = $value } } }
function Save([string]$sys) {
    $out = & $Mcp call 'EditorToolset.AssetToolset.SaveAsset' ((@{ assetPath = $sys.Split('.')[0] }) | ConvertTo-Json -Compress) -Port $Port -MaxChars 400
    Write-Host ('save ' + $sys + ' -> ' + ($out -join ' '))
}
$PS = 'ParticleSpawnScript'; $PU = 'ParticleUpdateScript'; $EU = 'EmitterUpdateScript'
$Rnd = '/Niagara/Enums/ENiagaraRandomnessMode.ENiagaraRandomnessMode'
$Size = '/Niagara/Enums/ENiagara_SizeScaleMode.ENiagara_SizeScaleMode'
$Space = '/Script/Niagara.ENiagaraCoordinateSpace'

# ---------------------------------------------------------------- NS_Bubbles
if (-not $Only -or $Only -eq 'NS_Bubbles') {
$sys = NewSystem 'NS_Bubbles'
Call 'AddUserVariables' ((@{ system = @{ refPath = $sys }; variablesToAdd = @((UserVar 'User.Count' '/Script/Niagara.NiagaraInt32' @{ value = 6 }), (UserVar 'User.SizeScale' '/Script/Niagara.NiagaraFloat' @{ value = 1.0 })) }) | ConvertTo-Json -Depth 10 -Compress)
Linked $sys $EU 'SpawnBurst_Instantaneous' @('Spawn Count') 'User.Count' '/Script/Niagara.NiagaraInt32'
# Lifetime 1.8 to 3 s at a terminal rise of about 0.7 m/s: gone within two metres or so.
F $sys $PS 'InitializeParticle' @('Lifetime Min') 1.8
F $sys $PS 'InitializeParticle' @('Lifetime Max') 3.0
SetIn $sys $PS 'InitializeParticle' @('Color') '/Script/CoreUObject.LinearColor' @{ r = 1; g = 1; b = 1; a = 1 }
En $sys $PS 'InitializeParticle' @('Sprite Size Mode') $Size 'ENiagara_SizeScaleMode::NewEnumerator1'
F $sys $PS 'InitializeParticle' @('Uniform Sprite Size Min') 0.6
F $sys $PS 'InitializeParticle' @('Uniform Sprite Size Max') 2.4
# A small sideways puff from the mouth; the rise comes from buoyancy below.
F $sys $PS 'AddVelocityInCone' @('Cone Angle') 55
En $sys $PS 'AddVelocityInCone' @('Cone Axis Coordinate Space') $Space 'ENiagaraCoordinateSpace::World'
F $sys $PS 'AddVelocityInCone' @('Velocity Strength', 'Minimum') 10
F $sys $PS 'AddVelocityInCone' @('Velocity Strength', 'Maximum') 45
V3 $sys $PS 'AddVelocityInCone' @('Cone Axis') 0 0 1
# Buoyancy as an upward acceleration; linear drag 3 to 5 gives a terminal rise of 50 to 90 cm/s.
V3 $sys $PU 'GravityForce' @('Gravity') 0 0 260
F $sys $PU 'Drag' @('Drag', 'Minimum') 3.0
F $sys $PU 'Drag' @('Drag', 'Maximum') 5.0
# AddModule appends at the end of the stack, after the solver, where a force is lost (fundamentals: forces, then
# collision, then the solve, once per tick). So add the force, then move the solver to the end again.
AddMod $sys $PU '' '/Niagara/Modules/Update/Forces/CurlNoiseForce.CurlNoiseForce'
RemoveMod $sys $PU 'SolveForcesAndVelocity'
AddMod $sys $PU '' '/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity'
AddMod $sys $PU '' '/Niagara/Modules/Update/Size/ScaleSpriteSize.ScaleSpriteSize'
# Wobble: curl noise with a 20 cm period, so a bubble rising at 0.7 m/s weaves at about 3 Hz.
F $sys $PU 'CurlNoiseForce' @('Noise Strength') 120
F $sys $PU 'CurlNoiseForce' @('Noise Frequency') 0.05
# Grows a little as it rises (less pressure above): 0.85 to 1.4 of its size over its life, times User.SizeScale.
SetIn $sys $PU 'ScaleSpriteSize' @('Uniform Curve Sprite Scale') '/Script/NiagaraEditor.NiagaraExt_StackInputData_DataInterface' @{ propertyValues = '{"Curve":{"keys":[{"interpMode":"RCIM_Linear","time":0,"value":0.85},{"interpMode":"RCIM_Linear","time":1,"value":1.4}]}}' }
Linked $sys $PU 'ScaleSpriteSize' @('Uniform Curve Scale') 'User.SizeScale' '/Script/Niagara.NiagaraFloat'
# Renderer: the lit bubble material; translucent sprites cast no shadow.
Call 'SetRendererData' ((@{ rendererRef = @{ system = @{ refPath = $sys }; emitterName = $E; rendererIndex = 0 }; rendererData = @{ propertyValues = '{"Material":{"refPath":"/Game/Deep/FX/M_Bubble.M_Bubble"},"bCastShadows":false}' } }) | ConvertTo-Json -Depth 8 -Compress)
Call 'GetStackIssues' ((@{ system = @{ refPath = $sys } }) | ConvertTo-Json -Compress)
Save $sys
}

# ---------------------------------------------------------------- NS_SiltPuff
# A soft ring of silt kicked off the bed: wide and low, slow, spreading and settling, hanging 3 to 5 s in the lamp.
if (-not $Only -or $Only -eq 'NS_SiltPuff') {
$sys = NewSystem 'NS_SiltPuff'
Call 'AddUserVariables' ((@{ system = @{ refPath = $sys }; variablesToAdd = @((UserVar 'User.Count' '/Script/Niagara.NiagaraInt32' @{ value = 12 }), (UserVar 'User.SizeScale' '/Script/Niagara.NiagaraFloat' @{ value = 1.0 })) }) | ConvertTo-Json -Depth 10 -Compress)
Linked $sys $EU 'SpawnBurst_Instantaneous' @('Spawn Count') 'User.Count' '/Script/Niagara.NiagaraInt32'
F $sys $PS 'InitializeParticle' @('Lifetime Min') 3.0
F $sys $PS 'InitializeParticle' @('Lifetime Max') 5.0
SetIn $sys $PS 'InitializeParticle' @('Color') '/Script/CoreUObject.LinearColor' @{ r = 1; g = 1; b = 1; a = 1 }
En $sys $PS 'InitializeParticle' @('Sprite Size Mode') $Size 'ENiagara_SizeScaleMode::NewEnumerator1'
F $sys $PS 'InitializeParticle' @('Uniform Sprite Size Min') 18
F $sys $PS 'InitializeParticle' @('Uniform Sprite Size Max') 40
V3 $sys $PS 'AddVelocityInCone' @('Cone Axis') 0 0 1
F $sys $PS 'AddVelocityInCone' @('Cone Angle') 80
En $sys $PS 'AddVelocityInCone' @('Cone Axis Coordinate Space') $Space 'ENiagaraCoordinateSpace::World'
F $sys $PS 'AddVelocityInCone' @('Velocity Strength', 'Minimum') 20
F $sys $PS 'AddVelocityInCone' @('Velocity Strength', 'Maximum') 60
# Fine silt settles slowly; heavy drag stops the spread within half a metre or so.
V3 $sys $PU 'GravityForce' @('Gravity') 0 0 -12
F $sys $PU 'Drag' @('Drag', 'Minimum') 1.6
F $sys $PU 'Drag' @('Drag', 'Maximum') 2.6
AddMod $sys $PU '' '/Niagara/Modules/Update/Forces/CurlNoiseForce.CurlNoiseForce'
RemoveMod $sys $PU 'SolveForcesAndVelocity'
AddMod $sys $PU '' '/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity'
AddMod $sys $PU '' '/Niagara/Modules/Update/Size/ScaleSpriteSize.ScaleSpriteSize'
F $sys $PU 'CurlNoiseForce' @('Noise Strength') 12
F $sys $PU 'CurlNoiseForce' @('Noise Frequency') 0.01
# Billows: 1 to 2.4 of its size over its life.
SetIn $sys $PU 'ScaleSpriteSize' @('Uniform Curve Sprite Scale') '/Script/NiagaraEditor.NiagaraExt_StackInputData_DataInterface' @{ propertyValues = '{"Curve":{"keys":[{"interpMode":"RCIM_Linear","time":0,"value":1.0},{"interpMode":"RCIM_Linear","time":1,"value":2.4}]}}' }
Linked $sys $PU 'ScaleSpriteSize' @('Uniform Curve Scale') 'User.SizeScale' '/Script/Niagara.NiagaraFloat'
Call 'SetRendererData' ((@{ rendererRef = @{ system = @{ refPath = $sys }; emitterName = $E; rendererIndex = 0 }; rendererData = @{ propertyValues = '{"Material":{"refPath":"/Game/Deep/FX/M_SiltPuff.M_SiltPuff"},"bCastShadows":false}' } }) | ConvertTo-Json -Depth 8 -Compress)
Call 'GetStackIssues' ((@{ system = @{ refPath = $sys } }) | ConvertTo-Json -Compress)
Save $sys
}
