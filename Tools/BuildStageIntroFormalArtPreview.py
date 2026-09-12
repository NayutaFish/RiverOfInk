"""Import formal Stage 1 art and arrange it in the isolated ink preview map.

The script intentionally keeps the existing preview director and overlay actor.
It only adds tagged formal-art actors, aligns the ink plane with the scroll's
paper area, and updates the straight-on acceptance camera.
"""

import os
import unreal


TARGET_MAP = "/Game/Level/InkOverlayPreview"
RAW_DIR = r"E:/project/UE/demo0803/Content/RawContent/SceneMake/inkoptimization"
ART_DIR = "/Game/Presentation/StageIntro/FormalArt"
MATERIAL_DIR = ART_DIR + "/Materials"
SEQUENCE_PATH = "/Game/Presentation/StageIntro/LS_Stage01Intro"
DIRECTOR_LABEL = "Stage01IntroDirector"
CAMERA_TAG = "Stage01IntroCamera"
OVERLAY_TAG = "Stage01InkOverlay"
FORMAL_TAG = "StageIntroFormalArt"

SCROLL_TARGET_MAJOR = 2500.0
INKSTONE_TARGET_MAJOR = 560.0
PAPERWEIGHT_TARGET_MAJOR = 430.0
PAPER_WIDTH_FRACTION = 0.84
PAPER_HEIGHT_FRACTION = 0.88
OVERLAY_LIFT = 2.0
# The camera is composed around the scroll and inkstone, not the full
# bounding box of every set-dressing prop. This matches the tighter opening
# frame and keeps the small objects readable at menu resolution.
CAMERA_DISTANCE_FACTOR = 1.05
CAMERA_MIN_HEIGHT = 2800.0

ASSETS = (
    {
        "key": "Scroll",
        "label": "StageIntro_FormalScroll",
        "tag": "StageIntroFormalScroll",
        "mesh_file": "卷轴.fbx",
        "mesh_name": "SM_StageIntro_Scroll",
        "material_name": "M_StageIntro_ScrollPBR",
        "base_file": "卷轴_juanzhou_BaseColor.png",
        "normal_file": "卷轴_juanzhou_Normal.png",
        "orm_file": "卷轴_juanzhou_OcclusionRoughnessMetallic.png",
    },
    {
        "key": "Inkstone",
        "label": "StageIntro_FormalInkstone",
        "tag": "StageIntroFormalInkstone",
        "mesh_file": "砚台.fbx",
        "mesh_name": "SM_StageIntro_Inkstone",
        "material_name": "M_StageIntro_InkstonePBR",
        "base_file": "砚台_yantai_BaseColor.png",
        "normal_file": "砚台_yantai_Normal.png",
        "orm_file": "砚台_yantai_OcclusionRoughnessMetallic.png",
    },
    {
        "key": "Paperweight",
        "label": "StageIntro_FormalPaperweight",
        "tag": "StageIntroFormalPaperweight",
        "mesh_file": "镇纸.fbx",
        "mesh_name": "SM_StageIntro_Paperweight",
        "material_name": "M_StageIntro_PaperweightPBR",
        "base_file": "镇纸_zhenzhi_BaseColor.png",
        "normal_file": "镇纸_zhenzhi_Normal.png",
        "orm_file": "镇纸_zhenzhi_OcclusionRoughnessMetallic.png",
    },
)


def log(message):
    unreal.log("[StageIntroFormalArt] " + message)


def fail(message):
    raise RuntimeError("[StageIntroFormalArt] " + message)


def asset_path(directory, name):
    return directory + "/" + name


def source_path(name):
    path = os.path.join(RAW_DIR, name)
    if not os.path.isfile(path):
        fail("Missing source file: {}".format(path))
    return path


def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def load_asset(path):
    if not asset_exists(path):
        return None
    return unreal.EditorAssetLibrary.load_asset(path)


def import_static_mesh(source_file, destination_name):
    destination = asset_path(ART_DIR, destination_name)
    existing = load_asset(destination)
    if existing:
        return existing

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    options.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source_path(source_file))
    task.set_editor_property("destination_path", ART_DIR)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    mesh = load_asset(destination)
    if not mesh:
        fail("Static mesh import failed: {}".format(source_file))
    return mesh


def import_texture(source_file, destination_name, kind):
    destination = asset_path(ART_DIR, destination_name)
    texture = load_asset(destination)
    if not texture:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source_path(source_file))
        task.set_editor_property("destination_path", ART_DIR)
        task.set_editor_property("destination_name", destination_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = load_asset(destination)

    if not texture:
        fail("Texture import failed: {}".format(source_file))

    if kind == "normal":
        texture.set_editor_property("srgb", False)
        texture.set_editor_property(
            "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP
        )
    elif kind == "orm":
        texture.set_editor_property("srgb", False)
        texture.set_editor_property(
            "compression_settings", unreal.TextureCompressionSettings.TC_MASKS
        )
    else:
        texture.set_editor_property("srgb", True)

    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def create_texture_sample(material, texture, x, y):
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, x, y
    )
    sample.set_editor_property("texture", texture)
    return sample


def create_channel_mask(material, source, red, green, blue, x, y):
    mask = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, x, y
    )
    mask.set_editor_property("r", red)
    mask.set_editor_property("g", green)
    mask.set_editor_property("b", blue)
    unreal.MaterialEditingLibrary.connect_material_expressions(source, "", mask, "Input")
    return mask


def create_pbr_material(definition, base_color, normal, orm):
    material_path = asset_path(MATERIAL_DIR, definition["material_name"])
    material = load_asset(material_path)
    if material:
        return material

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        definition["material_name"],
        MATERIAL_DIR,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        fail("Could not create material: {}".format(material_path))

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    )
    material.set_editor_property("two_sided", True)

    base_sample = create_texture_sample(material, base_color, -900, -220)
    normal_sample = create_texture_sample(material, normal, -900, 0)
    orm_sample = create_texture_sample(material, orm, -900, 220)
    roughness = create_channel_mask(material, orm_sample, False, True, False, -560, 210)
    metallic = create_channel_mask(material, orm_sample, False, False, True, -560, 330)

    unreal.MaterialEditingLibrary.connect_material_property(
        base_sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        normal_sample, "RGB", unreal.MaterialProperty.MP_NORMAL
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic, "", unreal.MaterialProperty.MP_METALLIC
    )
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def import_formal_assets():
    imported = {}
    for definition in ASSETS:
        mesh = import_static_mesh(definition["mesh_file"], definition["mesh_name"])
        base = import_texture(
            definition["base_file"],
            "T_StageIntro_{}_BaseColor".format(definition["key"]),
            "base",
        )
        normal = import_texture(
            definition["normal_file"],
            "T_StageIntro_{}_Normal".format(definition["key"]),
            "normal",
        )
        orm = import_texture(
            definition["orm_file"],
            "T_StageIntro_{}_ORM".format(definition["key"]),
            "orm",
        )
        material = create_pbr_material(definition, base, normal, orm)
        imported[definition["key"]] = (definition, mesh, material)
    return imported


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
    if current_map_path() != TARGET_MAP:
        fail("Expected map {} but found {}".format(TARGET_MAP, current_map_path()))


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


def find_director():
    actor = find_actor_by_label(DIRECTOR_LABEL)
    if actor:
        return actor
    for candidate in unreal.EditorLevelLibrary.get_all_level_actors():
        if "Stage01IntroDirector" in candidate.get_class().get_name():
            return candidate
    return None


def set_property(actor, names, value):
    for name in names:
        try:
            actor.set_editor_property(name, value)
            return
        except Exception:
            pass
    fail("Could not set {} on {}".format(", ".join(names), actor.get_name()))


def find_or_spawn_mesh_actor(label, tag):
    actor = find_actor_by_label(label)
    if actor:
        return actor
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0)
    )
    if not actor:
        fail("Could not spawn {}".format(label))
    actor.set_actor_label(label)
    actor.tags = [unreal.Name(FORMAL_TAG), unreal.Name(tag)]
    return actor


def mesh_rotation_for_horizontal_surface(mesh):
    extent = mesh.get_bounds().box_extent
    sizes = (abs(extent.x), abs(extent.y), abs(extent.z))
    thin_axis = min(range(3), key=lambda index: sizes[index])
    if thin_axis == 2:
        return unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
    if thin_axis == 1:
        return unreal.Rotator(roll=90.0, pitch=0.0, yaw=0.0)
    return unreal.Rotator(roll=0.0, pitch=90.0, yaw=0.0)


def configure_mesh_actor(actor, mesh, material, target_major, x, y):
    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(mesh)
    count = max(1, component.get_num_materials())
    for index in range(count):
        component.set_material(index, material)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

    extent = mesh.get_bounds().box_extent
    raw_sizes = (abs(extent.x) * 2.0, abs(extent.y) * 2.0, abs(extent.z) * 2.0)
    thin_axis = min(range(3), key=lambda index: raw_sizes[index])
    surface_sizes = [raw_sizes[index] for index in range(3) if index != thin_axis]
    raw_major = max(surface_sizes)
    if raw_major <= 0.001:
        fail("Invalid mesh bounds for {}".format(mesh.get_name()))

    scale = target_major / raw_major
    actor.set_actor_location_and_rotation(
        unreal.Vector(x, y, 0.0),
        mesh_rotation_for_horizontal_surface(mesh),
        False,
        False,
    )
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    origin, box_extent = actor.get_actor_bounds(False)
    actor.set_actor_location(
        unreal.Vector(x, y, -(origin.z - box_extent.z)), False, False
    )
    return actor.get_actor_bounds(False)


def configure_overlay(scroll_bounds):
    overlay = find_actor_by_tag(OVERLAY_TAG)
    if not overlay:
        fail("Could not find the existing ink overlay actor")

    origin, extent = scroll_bounds
    width = extent.x * 2.0
    height = extent.y * 2.0
    overlay.set_actor_location_and_rotation(
        unreal.Vector(origin.x, origin.y, origin.z + extent.z + OVERLAY_LIFT),
        unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0),
        False,
        False,
    )
    # Engine/BasicShapes/Plane is a 100 x 100 world-unit mesh.
    overlay.set_actor_scale3d(
        unreal.Vector(
            width * PAPER_WIDTH_FRACTION / 100.0,
            height * PAPER_HEIGHT_FRACTION / 100.0,
            1.0,
        )
    )
    return overlay


def configure_director_and_camera(overlay, scene_actors):
    director = find_director()
    camera = find_actor_by_tag(CAMERA_TAG)
    sequence = load_asset(SEQUENCE_PATH)
    if not director or not camera or not sequence:
        fail("Preview director, camera, or sequence is missing")

    set_property(director, ("intro_sequence",), sequence)
    set_property(director, ("intro_camera",), camera)
    set_property(director, ("ink_overlay",), overlay)
    set_property(director, ("preview_only", "b_preview_only"), True)
    set_property(director, ("auto_play_on_begin_play", "b_auto_play_on_begin_play"), True)
    set_property(director, ("auto_play_delay",), 0.25)
    set_property(director, ("preview_use_static_camera", "b_preview_use_static_camera"), True)
    set_property(director, ("preview_loop", "b_preview_loop"), True)
    set_property(director, ("preview_loop_delay",), 0.35)

    scroll_origin, scroll_extent = scene_actors[0].get_actor_bounds(False)
    inkstone_origin, inkstone_extent = scene_actors[1].get_actor_bounds(False)
    top_z = max(
        scroll_origin.z + scroll_extent.z,
        inkstone_origin.z + inkstone_extent.z,
    )
    scroll_width = scroll_extent.x * 2.0
    scroll_height = scroll_extent.y * 2.0

    # Bias upward and toward the inkstone. The paperweight remains in shot,
    # but it no longer makes the opening frame unnecessarily wide.
    center_x = scroll_origin.x - scroll_width * 0.06
    center_y = scroll_origin.y + scroll_height * 0.10
    major_span = max(scroll_width * 1.14, scroll_height * 1.30)
    camera_height = max(CAMERA_MIN_HEIGHT, major_span * CAMERA_DISTANCE_FACTOR)
    camera.set_actor_location_and_rotation(
        unreal.Vector(center_x, center_y, top_z + camera_height),
        unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0),
        False,
        False,
    )
    log(
        "Hero camera centered at ({:.1f}, {:.1f}) height {:.1f}; scroll frame {:.1f}".format(
            center_x, center_y, camera_height, major_span
        )
    )


def build_preview():
    ensure_target_map_loaded()
    formal = import_formal_assets()

    scroll_definition, scroll_mesh, scroll_material = formal["Scroll"]
    ink_definition, ink_mesh, ink_material = formal["Inkstone"]
    weight_definition, weight_mesh, weight_material = formal["Paperweight"]

    scroll_actor = find_or_spawn_mesh_actor(
        scroll_definition["label"], scroll_definition["tag"]
    )
    scroll_bounds = configure_mesh_actor(
        scroll_actor, scroll_mesh, scroll_material, SCROLL_TARGET_MAJOR, 0.0, 0.0
    )
    scroll_origin, scroll_extent = scroll_bounds
    scroll_width = scroll_extent.x * 2.0
    scroll_height = scroll_extent.y * 2.0

    inkstone_actor = find_or_spawn_mesh_actor(
        ink_definition["label"], ink_definition["tag"]
    )
    configure_mesh_actor(
        inkstone_actor,
        ink_mesh,
        ink_material,
        INKSTONE_TARGET_MAJOR,
        -scroll_width * 0.42,
        scroll_height * 0.58,
    )

    paperweight_actor = find_or_spawn_mesh_actor(
        weight_definition["label"], weight_definition["tag"]
    )
    configure_mesh_actor(
        paperweight_actor,
        weight_mesh,
        weight_material,
        PAPERWEIGHT_TARGET_MAJOR,
        scroll_width * 0.43,
        -scroll_height * 0.54,
    )

    overlay = configure_overlay(scroll_bounds)
    configure_director_and_camera(
        overlay, (scroll_actor, inkstone_actor, paperweight_actor, overlay)
    )

    if not unreal.EditorLevelLibrary.save_current_level():
        fail("Could not save {}".format(TARGET_MAP))
    log("READY: formal scroll, inkstone, paperweight, and aligned ink overlay")


build_preview()