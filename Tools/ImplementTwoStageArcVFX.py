import unreal


ROOT = "/Game/RawContent/VFX/NiagaraSystem/NS/PlayerTwoStageArc"
TEXTURE_PATH = ROOT + "/Textures/T_TwoStageArc_Crescent"
PATH_TEXTURE_PATH = ROOT + "/Textures/T_TwoStageArc_Path"
MAIN_MATERIAL_PATH = ROOT + "/Materials/M_TwoStageArc_Crescent_Animated"
MIRROR_MATERIAL_PATH = ROOT + "/Materials/M_TwoStageArc_Crescent_Animated_Mirror"
PATH_TEXTURE_FILE = "D:/project/UE/RiverOfInk/Content/RawContent/VFX/NiagaraSystem/NS/PlayerTwoStageArc/Textures/T_TwoStageArc_Path.png"


def set_property(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:
        unreal.log_warning("SET_PROPERTY_FAILED {} {} {}".format(obj.get_class().get_name(), name, exc))


def create(material, cls, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)
    if expression is None:
        raise RuntimeError("Unable to create {}".format(cls.get_name()))
    return expression


def connect(source, source_output, target, target_input):
    ok = unreal.MaterialEditingLibrary.connect_material_expressions(source, source_output, target, target_input)
    if ok is False:
        raise RuntimeError("Connect failed: {} -> {}".format(source.get_class().get_name(), target.get_class().get_name()))


def expressions(material):
    try:
        return list(material.get_editor_property("expressions"))
    except Exception as exc:
        unreal.log_warning("EXPRESSIONS_READ_FAILED {}".format(exc))
        return []


def find_parameter(material, name):
    for expression in expressions(material):
        if expression.get_class().get_name() != "MaterialExpressionTextureSampleParameter2D":
            continue
        try:
            if str(expression.get_editor_property("parameter_name")) == name:
                return expression
        except Exception:
            pass
    return None


def find_scalar(material, name):
    for expression in expressions(material):
        if expression.get_class().get_name() != "MaterialExpressionScalarParameter":
            continue
        try:
            if str(expression.get_editor_property("parameter_name")) == name:
                return expression
        except Exception:
            pass
    return None


def find_texture_coordinate(material):
    for expression in expressions(material):
        if expression.get_class().get_name() == "MaterialExpressionTextureCoordinate":
            return expression
    return None


def find_desc(material, text):
    for expression in expressions(material):
        try:
            if str(expression.get_editor_property("desc")) == text:
                return expression
        except Exception:
            pass
    return None


def scalar(material, name, value, x, y):
    expression = find_scalar(material, name)
    if expression is None:
        expression = create(material, unreal.MaterialExpressionScalarParameter, x, y)
    set_property(expression, "parameter_name", name)
    set_property(expression, "default_value", value)
    return expression


def texture_sample(material, name, texture, x, y):
    expression = find_parameter(material, name)
    if expression is None:
        expression = create(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    set_property(expression, "parameter_name", name)
    set_property(expression, "texture", texture)
    return expression


def add_comment(expression, text):
    set_property(expression, "desc", text)


def import_path_texture():
    path_texture = unreal.load_asset(PATH_TEXTURE_PATH)
    if path_texture is None:
        task = unreal.AssetImportTask()
        set_property(task, "filename", PATH_TEXTURE_FILE)
        set_property(task, "destination_path", ROOT + "/Textures")
        set_property(task, "destination_name", "T_TwoStageArc_Path")
        set_property(task, "automated", True)
        set_property(task, "replace_existing", True)
        set_property(task, "save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        path_texture = unreal.load_asset(PATH_TEXTURE_PATH)
    if path_texture is None:
        raise RuntimeError("Path mask import failed: {}".format(PATH_TEXTURE_PATH))
    set_property(path_texture, "srgb", False)
    set_property(path_texture, "never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(path_texture)
    unreal.log("PATH_TEXTURE_READY {}".format(path_texture.get_path_name()))
    return path_texture


def disconnect_outputs(material):
    try:
        unreal.MaterialEditingLibrary.disconnect_material_property(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.disconnect_material_property(material, unreal.MaterialProperty.MP_OPACITY)
    except Exception as exc:
        unreal.log_warning("DISCONNECT_FAILED {}".format(exc))
    material.modify()


def build_material(material_path, path_texture, mirror=False):
    material = unreal.load_asset(material_path)
    texture = unreal.load_asset(TEXTURE_PATH)
    if material is None or texture is None:
        raise RuntimeError("Missing material or source texture for {}".format(material_path))

    # Keep the authored texture/sample nodes intact. Deleting all material
    # expressions can trip an Unreal assertion on rooted materials; only the
    # material output connections are replaced below.
    disconnect_outputs(material)

    uv = find_texture_coordinate(material)
    if uv is None:
        uv = create(material, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    add_comment(uv, "Stable 0-1 sprite UV; no texture panning")

    sample_uv = uv
    if mirror:
        append_uv = find_desc(material, "TwoStageArc_PathUV_Mirror")
        if append_uv is None:
            u_mask = create(material, unreal.MaterialExpressionComponentMask, -1000, -140)
            set_property(u_mask, "r", True)
            set_property(u_mask, "g", False)
            set_property(u_mask, "b", False)
            set_property(u_mask, "a", False)
            flip_u = create(material, unreal.MaterialExpressionOneMinus, -820, -140)
            v_mask = create(material, unreal.MaterialExpressionComponentMask, -1000, 140)
            set_property(v_mask, "r", False)
            set_property(v_mask, "g", True)
            set_property(v_mask, "b", False)
            set_property(v_mask, "a", False)
            append_uv = create(material, unreal.MaterialExpressionAppendVector, -620, 0)
            connect(uv, "", u_mask, "")
            connect(u_mask, "", flip_u, "")
            connect(uv, "", v_mask, "")
            connect(flip_u, "", append_uv, "A")
            connect(v_mask, "", append_uv, "B")
            add_comment(append_uv, "TwoStageArc_PathUV_Mirror")
        sample_uv = append_uv

    visual = texture_sample(material, "TwoStageArcTexture", texture, -620, 360)
    path = texture_sample(material, "TwoStageArcPath", path_texture, -620, 640)
    connect(sample_uv, "", visual, "UVs")
    connect(sample_uv, "", path, "UVs")
    add_comment(visual, "Legacy crescent artwork")
    add_comment(path, "R = progress from upper crescent tip to lower crescent tip")

    relative_time = create(material, unreal.MaterialExpressionParticleRelativeTime, -620, 900)
    reveal_speed = scalar(material, "RevealSpeed", 2.22, -620, 1040)
    reveal_softness = scalar(material, "RevealSoftness", 0.022, -620, 1180)
    fade_start = scalar(material, "FadeStart", 0.58, 100, 1180)
    fade_end = scalar(material, "FadeEnd", 1.0, 320, 1180)

    reveal_time = create(material, unreal.MaterialExpressionMultiply, -300, 900)
    reveal_progress = create(material, unreal.MaterialExpressionSaturate, -80, 900)
    connect(relative_time, "", reveal_time, "A")
    connect(reveal_speed, "", reveal_time, "B")
    connect(reveal_time, "", reveal_progress, "")

    path_before = create(material, unreal.MaterialExpressionSubtract, -300, 560)
    path_after = create(material, unreal.MaterialExpressionAdd, -300, 700)
    connect(path, "R", path_before, "A")
    connect(reveal_softness, "", path_before, "B")
    connect(path, "R", path_after, "A")
    connect(reveal_softness, "", path_after, "B")

    reveal_mask = create(material, unreal.MaterialExpressionSmoothStep, 140, 700)
    connect(path_before, "", reveal_mask, "Min")
    connect(path_after, "", reveal_mask, "Max")
    connect(reveal_progress, "", reveal_mask, "Value")
    add_comment(reveal_mask, "Reveal threshold travels along the crescent path")

    fade_curve = create(material, unreal.MaterialExpressionSmoothStep, 140, 980)
    connect(fade_start, "", fade_curve, "Min")
    connect(fade_end, "", fade_curve, "Max")
    connect(relative_time, "", fade_curve, "Value")
    fade = create(material, unreal.MaterialExpressionOneMinus, 360, 980)
    connect(fade_curve, "", fade, "")

    alpha_factor = create(material, unreal.MaterialExpressionMultiply, 560, 760)
    connect(reveal_mask, "", alpha_factor, "A")
    connect(fade, "", alpha_factor, "B")
    add_comment(alpha_factor, "Reveal 0-0.45, hold 0.45-0.58, fade 0.58-1.0")

    emissive = create(material, unreal.MaterialExpressionMultiply, 800, 420)
    opacity = create(material, unreal.MaterialExpressionMultiply, 800, 700)
    connect(visual, "RGB", emissive, "A")
    connect(alpha_factor, "", emissive, "B")
    connect(visual, "A", opacity, "A")
    connect(alpha_factor, "", opacity, "B")

    unreal.MaterialEditingLibrary.connect_material_property(
        emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log("MATERIAL_READY {} mirror={} RevealSpeed=2.22 FadeStart=0.58 FadeEnd=1.0".format(material_path, mirror))
    return material


path_texture = import_path_texture()
build_material(MAIN_MATERIAL_PATH, path_texture, mirror=False)
build_material(MIRROR_MATERIAL_PATH, path_texture, mirror=True)
unreal.log("TWOSTAGEARC_VFX_IMPLEMENTED")
