"""Create the runtime-only ink opening material and its global parameter collection.

The script is idempotent. Run with UnrealEditor-Cmd or Execute Python Script.
"""

import unreal


ASSET_DIR = "/Game/Presentation/StageIntro"
MPC_PATH = ASSET_DIR + "/MPC_InkOpening"
MATERIAL_PATH = ASSET_DIR + "/M_InkPollution_Opening_v8"

SCALAR_DEFAULTS = (
    ("InkGrowth", 0.0),
    ("InkOpacity", 0.0),
    ("InkEdgeWidth", 0.085),
    ("InkWetness", 0.0),
    ("InkFlowSpeed", 0.0),
    ("InkCoreDensity", 0.0),
    ("InkSplatterAmount", 0.0),
    ("InkBlackout", 0.0),
)
VECTOR_DEFAULTS = (
    ("InkOrigin", unreal.LinearColor(0.58, 0.46, 0.0, 0.0)),
)


def fail(message):
    raise RuntimeError("[InkOpeningAssets] " + message)


def load_asset(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        return None
    return unreal.EditorAssetLibrary.load_asset(path)

def get_or_create(path, asset_name, asset_class, factory_class):
    asset = load_asset(path)
    if asset:
        return asset

    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, ASSET_DIR, asset_class, factory_class()
    )
    if not asset:
        fail("Could not create {}".format(path))
    return asset


def set_collection_parameters(collection):
    scalar_parameters = []
    for name, value in SCALAR_DEFAULTS:
        parameter = unreal.CollectionScalarParameter()
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", value)
        scalar_parameters.append(parameter)

    vector_parameters = []
    for name, value in VECTOR_DEFAULTS:
        parameter = unreal.CollectionVectorParameter()
        parameter.set_editor_property("parameter_name", name)
        parameter.set_editor_property("default_value", value)
        vector_parameters.append(parameter)

    collection.set_editor_property("scalar_parameters", scalar_parameters)
    collection.set_editor_property("vector_parameters", vector_parameters)
    unreal.EditorAssetLibrary.save_loaded_asset(collection)


def add_collection_parameter(material, collection, name, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionCollectionParameter, x, y
    )
    expression.set_editor_property("collection", collection)
    expression.set_editor_property("parameter_name", name)
    return expression


def custom_input(name):
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", name)
    return entry


def custom_output(name, output_type):
    entry = unreal.CustomOutput()
    entry.set_editor_property("output_name", name)
    entry.set_editor_property("output_type", output_type)
    return entry


def connect(material, source, source_output, target, target_input):
    unreal.MaterialEditingLibrary.connect_material_expressions(
        source, source_output, target, target_input
    )


def connect_property(material, source, source_output, material_property):
    unreal.MaterialEditingLibrary.connect_material_property(
        source, source_output, material_property
    )


def create_material_graph(material, collection):

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_UNLIT
    )
    material.set_editor_property("two_sided", True)

    uv = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1100, 0
    )
    time = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTime, -1100, 240
    )

    parameters = {}
    for index, (name, _) in enumerate(SCALAR_DEFAULTS):
        parameters[name] = add_collection_parameter(
            material, collection, name, -1100, 360 + index * 135
        )
    parameters["InkOrigin"] = add_collection_parameter(
        material, collection, "InkOrigin", -1100, 1500
    )

    ink = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionCustom, 180, 180
    )
    ink.set_editor_property("description", "Layered wet ink opening: body, bleed, pooled core, wet edge and micro normal")
    ink.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ink.set_editor_property(
        "inputs",
        [
            custom_input("UV"),
            custom_input("InkOrigin"),
            custom_input("InkGrowth"),
            custom_input("InkOpacity"),
            custom_input("InkEdgeWidth"),
            custom_input("InkWetness"),
            custom_input("InkFlowSpeed"),
            custom_input("InkCoreDensity"),
            custom_input("InkSplatterAmount"),
            custom_input("InkBlackout"),
            custom_input("Time"),
        ],
    )
    ink.set_editor_property(
        "additional_outputs",
        [
            custom_output("InkOpacityOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
            custom_output("InkRoughnessOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
            custom_output("InkSpecularOut", unreal.CustomMaterialOutputType.CMOT_FLOAT1),
            custom_output("InkNormalOut", unreal.CustomMaterialOutputType.CMOT_FLOAT3),
        ],
    )
    ink.set_editor_property(
        "code",
        r'''float2 p = UV - InkOrigin.xy;
float flowTime = Time * max(InkFlowSpeed, 0.02);

// Multi-core signed distance field. The delayed side cores make the
// contamination gain weight asymmetrically instead of expanding as one disc.
// Keep the contour alive until the end of the timeline. A radius of 1.10
// covered every UV corner too early, leaving a visible dead zone before black.
float mainRadius = lerp(0.016, 0.86, saturate(InkGrowth));
float mainField = length(p * float2(0.89, 1.05)) - mainRadius;
float sideGateA = smoothstep(0.16, 0.34, InkGrowth);
float sideGateB = smoothstep(0.31, 0.56, InkGrowth);
float sideRadiusA = mainRadius * 0.64 + 0.045;
float sideRadiusB = mainRadius * 0.47 + 0.035;
float sideFieldA = lerp(4.0, length((p - float2(-0.16, 0.11)) * float2(1.12, 0.78)) - sideRadiusA, sideGateA);
float sideFieldB = lerp(4.0, length((p - float2(0.19, -0.14)) * float2(0.76, 1.18)) - sideRadiusB, sideGateB);
float field = min(mainField, min(sideFieldA, sideFieldB));

// Two bilinear value-noise octaves produce non-periodic pooling. This removes
// the old UV.x sine bands that read as vertical stripes.
float2 n0p = UV * 6.7 + float2(flowTime * 0.010, -flowTime * 0.007);
float2 n0i = floor(n0p);
float2 n0f = frac(n0p);
n0f = n0f * n0f * (3.0 - 2.0 * n0f);
float n0a = frac(sin(dot(n0i, float2(127.1, 311.7))) * 43758.5453);
float n0b = frac(sin(dot(n0i + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);
float n0c = frac(sin(dot(n0i + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
float n0d = frac(sin(dot(n0i + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);
float n0 = lerp(lerp(n0a, n0b, n0f.x), lerp(n0c, n0d, n0f.x), n0f.y);
float2 n1p = UV * 22.9 + float2(n0 * 1.9 + flowTime * 0.015, n0 * -1.3);
float2 n1i = floor(n1p);
float2 n1f = frac(n1p);
n1f = n1f * n1f * (3.0 - 2.0 * n1f);
float n1a = frac(sin(dot(n1i, float2(269.5, 183.3))) * 28571.8731);
float n1b = frac(sin(dot(n1i + float2(1.0, 0.0), float2(269.5, 183.3))) * 28571.8731);
float n1c = frac(sin(dot(n1i + float2(0.0, 1.0), float2(269.5, 183.3))) * 28571.8731);
float n1d = frac(sin(dot(n1i + float2(1.0, 1.0), float2(269.5, 183.3))) * 28571.8731);
float n1 = lerp(lerp(n1a, n1b, n1f.x), lerp(n1c, n1d, n1f.x), n1f.y);
float warpAmplitude = lerp(0.024, 0.142, InkGrowth);
field += (n0 - 0.5) * warpAmplitude + (n1 - 0.5) * warpAmplitude * 0.34;

// Deliberate, broad local flows. Their gates and widths differ, preventing a
// symmetric petal shape while preserving a readable source of contamination.
float2 flowDirA = normalize(float2(-0.84, 0.54));
float2 flowSideA = float2(-flowDirA.y, flowDirA.x);
float lobeA = smoothstep(0.04, 0.42, dot(p, flowDirA))
    * (1.0 - smoothstep(0.025, 0.145, abs(dot(p, flowSideA))))
    * (0.045 + n1 * 0.050);
float2 flowDirB = normalize(float2(0.57, 0.82));
float2 flowSideB = float2(-flowDirB.y, flowDirB.x);
float lobeB = smoothstep(0.13, 0.48, dot(p, flowDirB))
    * (1.0 - smoothstep(0.018, 0.105, abs(dot(p, flowSideB))))
    * (0.028 + n0 * 0.040);
float2 flowDirC = normalize(float2(0.95, -0.31));
float2 flowSideC = float2(-flowDirC.y, flowDirC.x);
float lobeC = smoothstep(0.20, 0.58, dot(p, flowDirC))
    * (1.0 - smoothstep(0.012, 0.075, abs(dot(p, flowSideC))))
    * (0.016 + n1 * 0.024);
field -= lobeA + lobeB + lobeC;

// The lower-left page corner is furthest from the authored source. Give the
// spreading ink a delayed, irregular return flow toward it, instead of leaving
// that corner for a uniform blackout fill at the very end.
float2 lowerLeftDir = normalize(float2(-0.78, -0.62));
float lowerLeftT = smoothstep(0.38, 0.94, InkGrowth);
float lowerLeftLength = lerp(0.035, 0.84, lowerLeftT);
float lowerLeftRadius = lerp(0.018, 0.145, lowerLeftT);
float2 lowerLeftA = lowerLeftDir * 0.015;
float2 lowerLeftB = lowerLeftDir * lowerLeftLength;
float2 lowerLeftSegment = lowerLeftB - lowerLeftA;
float lowerLeftSegmentLengthSq = max(dot(lowerLeftSegment, lowerLeftSegment), 0.0001);
float lowerLeftAlong = saturate(dot(p - lowerLeftA, lowerLeftSegment) / lowerLeftSegmentLengthSq);
float2 lowerLeftClosest = lowerLeftA + lowerLeftSegment * lowerLeftAlong;
float lowerLeftField = length(p - lowerLeftClosest) - lowerLeftRadius;
float lowerLeftGate = smoothstep(0.42, 0.52, InkGrowth);
field = min(field, lerp(4.0, lowerLeftField, lowerLeftGate));

float edge = max(0.006, InkEdgeWidth);
float body = 1.0 - smoothstep(-edge * 0.22, edge * 1.05, field);
float denseCore = 1.0 - smoothstep(-mainRadius * 0.53, -mainRadius * 0.05, field);
float innerBody = 1.0 - smoothstep(-edge * 2.0, -edge * 0.28, field);
float wetRing = saturate(body - innerBody);
float bleed = (1.0 - smoothstep(edge * 0.28, edge * 2.05, field)) * (1.0 - body);

// Six fixed UV anchors: each speck is born ahead of the approaching front,
// stays on the paper, and then merges naturally as the body reaches it.
float dotA = 1.0 - smoothstep(0.003, 0.011, length(p - float2(-0.27, 0.18)));
float dotB = 1.0 - smoothstep(0.002, 0.008, length(p - float2(-0.34, 0.10)));
float dotC = 1.0 - smoothstep(0.003, 0.010, length(p - float2(0.30, -0.15)));
float dotD = 1.0 - smoothstep(0.002, 0.007, length(p - float2(0.37, -0.23)));
float dotE = 1.0 - smoothstep(0.002, 0.008, length(p - float2(0.16, 0.34)));
float dotF = 1.0 - smoothstep(0.0015, 0.005, length(p - float2(-0.12, -0.39)));
float dotSpawnA = smoothstep(0.14, 0.23, mainRadius);
float dotSpawnB = smoothstep(0.19, 0.29, mainRadius);
float dotSpawnC = smoothstep(0.22, 0.33, mainRadius);
float dotSpawnD = smoothstep(0.28, 0.40, mainRadius);
float dotSpawnE = smoothstep(0.25, 0.37, mainRadius);
float dotSpawnF = smoothstep(0.31, 0.44, mainRadius);
float fixedDots = (dotA * dotSpawnA + dotB * dotSpawnB + dotC * dotSpawnC + dotD * dotSpawnD + dotE * dotSpawnE + dotF * dotSpawnF);
fixedDots = saturate(fixedDots) * InkSplatterAmount * saturate(1.0 - body);

float pooling = saturate(0.43 + (n0 - 0.5) * 0.50 + (n1 - 0.5) * 0.18);
float coreDensity = saturate(denseCore * (0.88 + pooling * 0.12) * (0.74 + InkCoreDensity * 0.26));
float coreAlpha = denseCore * InkOpacity;
float bodyAlpha = body * InkOpacity * lerp(0.982, 0.998, denseCore);
float baseAlpha = saturate(max(coreAlpha, bodyAlpha) + bleed * InkOpacity * 0.12 + fixedDots * InkOpacity * 0.98);
// Blackout fills the remaining paper in a broad, continuous tail instead of
// waiting to reveal itself as one last alpha jump around the outer edge.
float blackout = saturate(InkBlackout);
// Keep the uncovered paper in motion as the close approaches, but leave the
// spatial completion to the directional return flow above rather than hiding a
// large unpainted corner in the last few frames.
float blackoutFill = blackout * (0.05 + (1.0 - body) * 0.80 + bleed * 0.15);
float alpha = saturate(baseAlpha + blackoutFill);

// Use a restrained warm-neutral charcoal. The emissive unlit layer keeps this
// value stable under the cool key and sky light used by the preview scene.
float3 coreTint = lerp(float3(0.0057, 0.0050, 0.0044), float3(0.00062, 0.00055, 0.00048), coreDensity);
float3 wetTint = float3(0.0190, 0.0172, 0.0155) * (0.80 + pooling * 0.20);
float3 color = lerp(coreTint, wetTint, wetRing * (0.22 + InkWetness * 0.38));
color += wetTint * bleed * 0.12;
color = lerp(color, float3(0.0014, 0.0012, 0.0010), fixedDots);
// Alpha alone cannot visibly change an already opaque ink body. Darken the
// unlit output through the same curve so the final approach to black has
// measurable movement in every rendered frame.
float blackoutWeight = blackout * (0.22 + 0.78 * saturate(body + bleed));
color = lerp(color, float3(0.00006, 0.00005, 0.00004), blackoutWeight);

InkOpacityOut = alpha;
InkRoughnessOut = saturate(0.83 - wetRing * InkWetness * 0.08 - denseCore * 0.02);
InkSpecularOut = saturate(0.008 + wetRing * InkWetness * 0.022);
float2 normalXY = float2((n1 - 0.5) * 2.0, (n0 - 0.5) * 2.0) * wetRing * InkWetness * 0.006;
InkNormalOut = normalize(float3(normalXY, 1.0));
return color;''',
    )

    connect(material, uv, "", ink, "UV")
    connect(material, parameters["InkOrigin"], "", ink, "InkOrigin")
    for name, _ in SCALAR_DEFAULTS:
        connect(material, parameters[name], "", ink, name)
    connect(material, time, "", ink, "Time")

    # Unlit emissive output prevents the cool preview lighting from tinting the
    # ink blue; opacity continues to control the paper-facing diffusion edge.
    connect_property(material, ink, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    connect_property(material, ink, "InkOpacityOut", unreal.MaterialProperty.MP_OPACITY)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def build_assets():
    # The collection is shared by every material revision. Do not save it again
    # when a later version is generated, because an Editor session may be
    # reading the MPC while the new material itself remains safe to create.
    collection = load_asset(MPC_PATH)
    if not collection:
        collection = get_or_create(
            MPC_PATH,
            "MPC_InkOpening",
            unreal.MaterialParameterCollection,
            unreal.MaterialParameterCollectionFactoryNew,
        )
        set_collection_parameters(collection)

    material_exists = unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH)
    material = get_or_create(
        MATERIAL_PATH,
        "M_InkPollution_Opening_v8",
        unreal.Material,
        unreal.MaterialFactoryNew,
    )
    if not material_exists:
        create_material_graph(material, collection)


build_assets()