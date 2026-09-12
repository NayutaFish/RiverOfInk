"""Create a warm, theatrical tabletop lighting rig for InkOverlayPreview."""

import math
import unreal


TARGET_MAP = "/Game/Level/InkOverlayPreview"
ART_DIR = "/Game/Presentation/StageIntro/FormalArt"
MATERIAL_DIR = ART_DIR + "/Materials"
CAMERA_TAG = "Stage01IntroCamera"
SCROLL_TAG = "StageIntroFormalScroll"
INKSTONE_TAG = "StageIntroFormalInkstone"
PAPERWEIGHT_TAG = "StageIntroFormalPaperweight"
LIGHTING_TAG = "StageIntroLighting"


def log(message):
    unreal.log("[StageIntroLighting] " + message)


def fail(message):
    raise RuntimeError("[StageIntroLighting] " + message)


def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def load_asset(path):
    if not asset_exists(path):
        return None
    return unreal.EditorAssetLibrary.load_asset(path)


def current_map_path():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        return ""
    return world.get_path_name().split(".")[0]


def ensure_target_map_loaded():
    if current_map_path() == TARGET_MAP:
        return
    if not unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP):
        fail("Could not load {}".format(TARGET_MAP))


def find_actor_by_label(label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def find_actor_by_tag(tag):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.actor_has_tag(tag):
            return actor
    return None


def tag_actor(actor, tag):
    tags = list(actor.tags)
    for value in (LIGHTING_TAG, tag):
        name = unreal.Name(value)
        if name not in tags:
            tags.append(name)
    actor.tags = tags


def find_or_spawn(actor_class, label, tag, location):
    actor = find_actor_by_label(label) or find_actor_by_tag(tag)
    if not actor:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location)
        if not actor:
            fail("Could not spawn {}".format(label))
        actor.set_actor_label(label)
    tag_actor(actor, tag)
    return actor


def set_if_present(target, property_name, value):
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception:
        return False


def point_at(actor, target):
    source = actor.get_actor_location()
    dx = target.x - source.x
    dy = target.y - source.y
    dz = target.z - source.z
    horizontal = math.sqrt(dx * dx + dy * dy)
    actor.set_actor_rotation(
        unreal.Rotator(
            roll=0.0,
            pitch=math.degrees(math.atan2(dz, horizontal)),
            yaw=math.degrees(math.atan2(dy, dx)),
        ),
        False,
    )

def configure_light_component(component, intensity, color, shadows):
    set_if_present(component, "mobility", unreal.ComponentMobility.MOVABLE)
    set_if_present(component, "intensity", intensity)
    set_if_present(component, "light_color", color)
    set_if_present(component, "cast_shadows", shadows)
    set_if_present(component, "shadow_resolution_scale", 2.0)
    set_if_present(component, "shadow_bias", 0.20)
    set_if_present(component, "contact_shadow_length", 0.28)


def configure_rect_light(
    label,
    tag,
    location,
    target,
    intensity,
    color,
    width,
    height,
    attenuation_radius,
    barn_door_angle,
    barn_door_length,
):
    actor = find_or_spawn(unreal.RectLight, label, tag, location)
    actor.set_actor_location(location, False, False)
    point_at(actor, target)
    component = actor.get_editor_property("light_component")
    configure_light_component(component, intensity, color, True)
    set_if_present(component, "source_width", width)
    set_if_present(component, "source_height", height)
    set_if_present(component, "barn_door_angle", barn_door_angle)
    set_if_present(component, "barn_door_length", barn_door_length)
    set_if_present(component, "attenuation_radius", attenuation_radius)
    return actor


def configure_directional_light(target):
    actor = find_or_spawn(
        unreal.DirectionalLight,
        "StageIntro_KeyDirectional",
        "StageIntro_KeyDirectional",
        unreal.Vector(0.0, 0.0, 2200.0),
    )
    actor.set_actor_location(unreal.Vector(0.0, 0.0, 2200.0), False, False)
    actor.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-54.0, yaw=-138.0), False)
    component = actor.get_editor_property("light_component")
    configure_light_component(
        component, 1.05, unreal.LinearColor(1.0, 0.55, 0.25, 1.0), True
    )
    set_if_present(component, "source_angle", 2.5)
    set_if_present(component, "source_soft_angle", 3.0)
    return actor


def configure_sky_light():
    actor = find_or_spawn(
        unreal.SkyLight,
        "StageIntro_LowAmbient",
        "StageIntro_LowAmbient",
        unreal.Vector(0.0, 0.0, 500.0),
    )
    component = actor.get_editor_property("light_component")
    configure_light_component(
        component, 0.045, unreal.LinearColor(0.055, 0.032, 0.014, 1.0), False
    )
    # This closed tabletop set has no SkyAtmosphere/VolumetricCloud capture
    # source. A static low-intensity skylight avoids the red editor warning
    # and is enough to preserve a trace of detail in shadow.
    set_if_present(component, "real_time_capture", False)
    return actor


def create_tabletop_material():
    material_path = MATERIAL_DIR + "/M_StageIntro_Tabletop_Noir"
    existing = load_asset(material_path)
    if existing:
        return existing

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_StageIntro_Tabletop_Noir",
        MATERIAL_DIR,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        fail("Could not create tabletop material")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    )
    base = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -450, -100
    )
    base.set_editor_property("constant", unreal.LinearColor(0.006, 0.003, 0.0015, 1.0))
    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -450, 100
    )
    roughness.set_editor_property("r", 0.97)
    unreal.MaterialEditingLibrary.connect_material_property(
        base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def configure_tabletop(scroll_origin, scroll_extent):
    table = find_or_spawn(
        unreal.StaticMeshActor,
        "StageIntro_Tabletop",
        "StageIntro_Tabletop",
        unreal.Vector(scroll_origin.x, scroll_origin.y, -20.0),
    )
    cube = load_asset("/Engine/BasicShapes/Cube.Cube")
    if not cube:
        fail("Could not load the engine cube mesh")
    component = table.get_editor_property("static_mesh_component")
    component.set_static_mesh(cube)
    component.set_material(0, create_tabletop_material())
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

    span = max(scroll_extent.x * 2.0, scroll_extent.y * 2.0)
    surface_z = scroll_origin.z - scroll_extent.z - 3.0
    thickness_scale = 0.24
    table.set_actor_scale3d(
        unreal.Vector(max(42.0, span * 1.85 / 100.0), max(42.0, span * 1.85 / 100.0), thickness_scale)
    )
    table.set_actor_location(
        unreal.Vector(scroll_origin.x, scroll_origin.y, surface_z - thickness_scale * 50.0),
        False,
        False,
    )
    return table, span


def configure_post_process():
    volume = find_or_spawn(
        unreal.PostProcessVolume,
        "StageIntro_WarmGrade",
        "StageIntro_WarmGrade",
        unreal.Vector(0.0, 0.0, 0.0),
    )
    set_if_present(volume, "unbound", True)
    set_if_present(volume, "blend_weight", 1.0)
    settings = volume.get_editor_property("settings")

    # The dark tabletop is deliberate, so histogram metering would continuously
    # brighten it and wash out the scroll. Use a calibrated manual exposure:
    # it is roughly 2.5 stops brighter than the previous failed manual pass,
    # while still retaining the black surround seen in the reference.
    auto_exposure_enum = getattr(unreal, "AutoExposureMethod", None)
    manual_exposure = (
        getattr(auto_exposure_enum, "AEM_MANUAL", None)
        if auto_exposure_enum
        else None
    )
    if manual_exposure is not None:
        set_if_present(settings, "override_auto_exposure_method", True)
        set_if_present(settings, "auto_exposure_method", manual_exposure)
    for override_name in (
        "override_auto_exposure_min_ev100",
        "override_auto_exposure_max_ev100",
        "override_auto_exposure_min_brightness",
        "override_auto_exposure_max_brightness",
    ):
        set_if_present(settings, override_name, False)
    set_if_present(settings, "override_auto_exposure_bias", True)
    set_if_present(settings, "auto_exposure_bias", 2.0)

    set_if_present(settings, "override_vignette_intensity", True)
    set_if_present(settings, "vignette_intensity", 0.56)
    set_if_present(settings, "override_bloom_intensity", True)
    set_if_present(settings, "bloom_intensity", 0.04)
    set_if_present(settings, "override_lens_flare_intensity", True)
    set_if_present(settings, "lens_flare_intensity", 0.0)
    set_if_present(settings, "override_color_saturation", True)
    set_if_present(settings, "color_saturation", unreal.LinearColor(1.0, 0.90, 0.76, 1.0))
    volume.set_editor_property("settings", settings)
    return volume


def configure_lighting():
    ensure_target_map_loaded()
    scroll = find_actor_by_tag(SCROLL_TAG)
    if not scroll:
        fail("Formal scroll actor is missing from the preview map")
    scroll_origin, scroll_extent = scroll.get_actor_bounds(False)
    table, span = configure_tabletop(scroll_origin, scroll_extent)
    target = unreal.Vector(scroll_origin.x, scroll_origin.y, scroll_origin.z)
    inkstone = find_actor_by_tag(INKSTONE_TAG)
    paperweight = find_actor_by_tag(PAPERWEIGHT_TAG)

    # A focused amber key creates the warm pool on the scroll, while the rest
    # of the tabletop is intentionally allowed to fall toward black.
    configure_directional_light(target)
    configure_rect_light(
        "StageIntro_WarmKey",
        "StageIntro_WarmKey",
        unreal.Vector(scroll_origin.x - span * 0.58, scroll_origin.y + span * 0.52, span * 0.95),
        target,
        98000.0,
        unreal.LinearColor(1.0, 0.49, 0.19, 1.0),
        1500.0,
        820.0,
        7600.0,
        62.0,
        48.0,
    )
    configure_rect_light(
        "StageIntro_SoftFill",
        "StageIntro_SoftFill",
        unreal.Vector(scroll_origin.x + span * 0.56, scroll_origin.y - span * 0.42, span * 0.72),
        target,
        6500.0,
        unreal.LinearColor(0.42, 0.22, 0.09, 1.0),
        1100.0,
        560.0,
        5400.0,
        50.0,
        62.0,
    )
    configure_rect_light(
        "StageIntro_RollRim",
        "StageIntro_RollRim",
        unreal.Vector(scroll_origin.x + span * 0.42, scroll_origin.y + span * 0.62, span * 0.78),
        unreal.Vector(scroll_origin.x + span * 0.12, scroll_origin.y + span * 0.08, scroll_origin.z),
        4200.0,
        unreal.LinearColor(1.0, 0.36, 0.12, 1.0),
        880.0,
        260.0,
        4200.0,
        38.0,
        85.0,
    )
    # Keep the room dark, but give the two small hero props their own narrow
    # pools of light. They are deliberately aimed at the props rather than
    # the paper, so their silhouette reads without flattening the scroll.
    if inkstone:
        ink_origin, ink_extent = inkstone.get_actor_bounds(False)
        ink_target = unreal.Vector(
            ink_origin.x, ink_origin.y, ink_origin.z + ink_extent.z * 0.35
        )
        configure_rect_light(
            "StageIntro_InkstoneKicker",
            "StageIntro_InkstoneKicker",
            unreal.Vector(
                ink_origin.x - span * 0.17,
                ink_origin.y + span * 0.18,
                max(ink_target.z + 520.0, scroll_origin.z + span * 0.42),
            ),
            ink_target,
            26000.0,
            unreal.LinearColor(1.0, 0.62, 0.36, 1.0),
            360.0,
            150.0,
            2800.0,
            30.0,
            95.0,
        )

    if paperweight:
        weight_origin, weight_extent = paperweight.get_actor_bounds(False)
        weight_target = unreal.Vector(
            weight_origin.x,
            weight_origin.y,
            weight_origin.z + weight_extent.z * 0.4,
        )
        configure_rect_light(
            "StageIntro_PaperweightKicker",
            "StageIntro_PaperweightKicker",
            unreal.Vector(
                weight_origin.x + span * 0.15,
                weight_origin.y - span * 0.14,
                max(weight_target.z + 420.0, scroll_origin.z + span * 0.32),
            ),
            weight_target,
            9500.0,
            unreal.LinearColor(1.0, 0.48, 0.22, 1.0),
            270.0,
            90.0,
            2200.0,
            26.0,
            80.0,
        )
    configure_sky_light()
    configure_post_process()

    if not unreal.EditorLevelLibrary.save_current_level():
        fail("Could not save {}".format(TARGET_MAP))
    log("READY: warm scroll pool with readable paper detail, dark tabletop, static low ambient, and controlled vignette")


configure_lighting()
