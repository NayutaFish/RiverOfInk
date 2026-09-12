"""Copy the authored InkOverlayPreview scene layer into MainMenu.

The preview map is the source of truth for scene dressing.  This script only
copies tagged scenery, lighting, post process, the camera transform, and the
ink overlay surface; it deliberately leaves MainMenu's director/menu widget
state and Level Blueprint untouched.
"""

import unreal


SOURCE_MAP = "/Game/Level/InkOverlayPreview"
TARGET_MAP = "/Game/Level/MainMenu"


SCENE_SYNC_TAG = "StageIntroMainMenuScene"
OVERLAY_TAG = "Stage01InkOverlay"
CAMERA_TAG = "Stage01IntroCamera"
DIRECTOR_LABEL = "Stage01IntroDirector"

STATIC_ACTORS = (
    ("StageIntro_FormalScroll", "StageIntroFormalScroll"),
    ("砚台", None),
    ("镇纸", None),
    ("StageIntro_Tabletop", "StageIntro_Tabletop"),
)
LIGHT_LABELS = (
    "Stage01_KeyLight",
    "StageIntro_KeyDirectional",
    "StageIntro_LowAmbient",
    "StageIntro_WarmKey",
    "StageIntro_SoftFill",
    "StageIntro_RollRim",
    "StageIntro_InkstoneKicker",
    "StageIntro_PaperweightKicker",
)
POST_PROCESS_LABEL = "StageIntro_WarmGrade"


def log(message):
    unreal.log("[StageIntroSceneSync] " + message)


def fail(message):
    raise RuntimeError("[StageIntroSceneSync] " + message)


def try_get(target, property_name, default=None):
    try:
        return target.get_editor_property(property_name)
    except Exception:
        return default


def try_set(target, property_name, value):
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception:
        return False


def current_map_path():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        return ""
    return world.get_path_name().split(".")[0]


def load_map(map_path):
    if current_map_path() == map_path:
        return
    if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
        fail("Could not load {}".format(map_path))
    if current_map_path() != map_path:
        fail("Expected {} but found {}".format(map_path, current_map_path()))


def actor_tags(actor):
    return [str(tag) for tag in actor.tags]


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
    director = find_actor_by_label(DIRECTOR_LABEL)
    if director:
        return director
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if "Stage01IntroDirector" in actor.get_class().get_name():
            return actor
    return None


def asset_path(asset):
    return asset.get_path_name() if asset else ""


def load_asset(path):
    return unreal.EditorAssetLibrary.load_asset(path) if path else None


def snapshot_transform(actor):
    return {
        "location": actor.get_actor_location(),
        "rotation": actor.get_actor_rotation(),
        "scale": actor.get_actor_scale3d(),
    }


def apply_transform(actor, transform):
    actor.set_actor_location_and_rotation(
        transform["location"], transform["rotation"], False, False
    )
    actor.set_actor_scale3d(transform["scale"])


def snapshot_static_mesh(actor):
    component = try_get(actor, "static_mesh_component")
    if not component:
        fail("{} has no StaticMeshComponent".format(actor.get_actor_label()))
    materials = []
    for index in range(component.get_num_materials()):
        materials.append(asset_path(component.get_material(index)))
    return {
        "kind": "static_mesh",
        "label": actor.get_actor_label(),
        "tags": actor_tags(actor),
        "transform": snapshot_transform(actor),
        "mesh": asset_path(try_get(component, "static_mesh")),
        "materials": materials,
        "collision": try_get(component, "collision_enabled"),
    }


def snapshot_overlay(actor):
    return {
        "kind": "overlay",
        "label": actor.get_actor_label(),
        "tags": actor_tags(actor),
        "transform": snapshot_transform(actor),
    }


LIGHT_PROPERTIES = (
    "mobility",
    "intensity",
    "light_color",
    "cast_shadows",
    "shadow_resolution_scale",
    "shadow_bias",
    "contact_shadow_length",
    "attenuation_radius",
    "source_width",
    "source_height",
    "barn_door_angle",
    "barn_door_length",
    "source_angle",
    "source_soft_angle",
    "real_time_capture",
)


def snapshot_light(actor):
    component = try_get(actor, "light_component")
    if not component:
        fail("{} has no LightComponent".format(actor.get_actor_label()))
    properties = {}
    for property_name in LIGHT_PROPERTIES:
        value = try_get(component, property_name)
        if value is not None:
            properties[property_name] = value
    return {
        "kind": "light",
        "label": actor.get_actor_label(),
        "tags": actor_tags(actor),
        "class_name": actor.get_class().get_name(),
        "transform": snapshot_transform(actor),
        "properties": properties,
    }


def snapshot_post_process(actor):
    return {
        "kind": "post_process",
        "label": actor.get_actor_label(),
        "tags": actor_tags(actor),
        "transform": snapshot_transform(actor),
        "unbound": try_get(actor, "unbound"),
        "blend_weight": try_get(actor, "blend_weight"),
        "settings": try_get(actor, "settings"),
    }


def snapshot_camera(actor):
    component = try_get(actor, "camera_component")
    properties = {}
    if component:
        for property_name in (
            "current_focal_length",
            "current_aperture",
            "filmback",
            "lens_settings",
            "focus_settings",
        ):
            value = try_get(component, property_name)
            if value is not None:
                properties[property_name] = value
    return {
        "kind": "camera",
        "label": actor.get_actor_label(),
        "tags": actor_tags(actor),
        "transform": snapshot_transform(actor),
        "properties": properties,
    }


def take_source_snapshot():
    load_map(SOURCE_MAP)
    snapshots = []

    for label, tag in STATIC_ACTORS:
        actor = find_actor_by_tag(tag) if tag else None
        actor = actor or find_actor_by_label(label)
        if not actor:
            fail("Source scene is missing '{}' (tag '{}')".format(label, tag))
        snapshots.append(snapshot_static_mesh(actor))

    overlay = find_actor_by_tag(OVERLAY_TAG)
    if not overlay:
        fail("Source scene is missing its ink overlay")
    snapshots.append(snapshot_overlay(overlay))

    for label in LIGHT_LABELS:
        actor = find_actor_by_label(label)
        if not actor:
            fail("Source scene is missing '{}'".format(label))
        snapshots.append(snapshot_light(actor))

    post_process = find_actor_by_label(POST_PROCESS_LABEL)
    if not post_process:
        fail("Source scene is missing '{}'".format(POST_PROCESS_LABEL))
    snapshots.append(snapshot_post_process(post_process))

    camera = find_actor_by_tag(CAMERA_TAG)
    if not camera:
        fail("Source scene is missing the authored intro camera")
    snapshots.append(snapshot_camera(camera))

    log("Captured {} scene actors from {}".format(len(snapshots), SOURCE_MAP))
    return snapshots


def class_for_light(class_name):
    if "PointLight" in class_name:
        return unreal.PointLight
    if "RectLight" in class_name:
        return unreal.RectLight
    if "DirectionalLight" in class_name:
        return unreal.DirectionalLight
    if "SkyLight" in class_name:
        return unreal.SkyLight
    fail("Unsupported source light class '{}'".format(class_name))


def set_actor_tags(actor, source_tags):
    tags = list(source_tags)
    if SCENE_SYNC_TAG not in tags:
        tags.append(SCENE_SYNC_TAG)
    actor.tags = [unreal.Name(tag) for tag in tags]


def find_target_actor(snapshot):
    # Tags identify the scene role and are safer than automatically generated
    # actor labels such as Plane_0 when the target map still has legacy props.
    for tag in snapshot["tags"]:
        actor = find_actor_by_tag(tag)
        if actor:
            return actor
    actor = find_actor_by_label(snapshot["label"])
    if actor:
        return actor
    return None


def ensure_actor(snapshot, expected_class, create_allowed=True):
    actor = find_target_actor(snapshot)
    if not actor:
        if not create_allowed:
            fail("MainMenu is missing required actor '{}'".format(snapshot["label"]))
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            expected_class, snapshot["transform"]["location"]
        )
    if not actor:
        fail("Could not create '{}' in MainMenu".format(snapshot["label"]))
    actor.set_actor_label(snapshot["label"])
    set_actor_tags(actor, snapshot["tags"])
    return actor


def apply_static_mesh(snapshot):
    actor = ensure_actor(snapshot, unreal.StaticMeshActor)
    component = try_get(actor, "static_mesh_component")
    mesh = load_asset(snapshot["mesh"])
    if not mesh:
        fail("Could not load mesh for '{}'".format(snapshot["label"]))
    component.set_static_mesh(mesh)
    for index, material_path in enumerate(snapshot["materials"]):
        material = load_asset(material_path)
        if material:
            component.set_material(index, material)
    if snapshot["collision"] is not None:
        try_set(component, "collision_enabled", snapshot["collision"])
    apply_transform(actor, snapshot["transform"])
    return actor


def apply_overlay(snapshot):
    # Overlay is a Blueprint actor. Keep MainMenu's existing instance so its
    # runtime material setup and director references remain intact.
    actor = find_target_actor(snapshot)
    if not actor:
        fail("MainMenu is missing its ink overlay actor")
    actor.set_actor_label(snapshot["label"])
    set_actor_tags(actor, snapshot["tags"])
    apply_transform(actor, snapshot["transform"])
    return actor


def apply_light(snapshot):
    actor = ensure_actor(snapshot, class_for_light(snapshot["class_name"]))
    component = try_get(actor, "light_component")
    for property_name, value in snapshot["properties"].items():
        try_set(component, property_name, value)
    apply_transform(actor, snapshot["transform"])
    return actor


def apply_post_process(snapshot):
    actor = ensure_actor(snapshot, unreal.PostProcessVolume)
    try_set(actor, "unbound", snapshot["unbound"])
    try_set(actor, "blend_weight", snapshot["blend_weight"])
    try_set(actor, "settings", snapshot["settings"])
    apply_transform(actor, snapshot["transform"])
    return actor


def apply_camera(snapshot):
    # The target camera must be updated in place: the production Level Sequence
    # already owns a binding to it. Do not spawn a replacement camera here.
    actor = ensure_actor(snapshot, unreal.CineCameraActor, create_allowed=False)
    component = try_get(actor, "camera_component")
    if component:
        for property_name, value in snapshot["properties"].items():
            try_set(component, property_name, value)
    apply_transform(actor, snapshot["transform"])
    return actor


def set_director_reference(director, property_names, value):
    for property_name in property_names:
        if try_set(director, property_name, value):
            return
    fail("Could not update director reference {}".format(property_names))


def apply_snapshot(snapshots):
    load_map(TARGET_MAP)
    applied = {}
    for snapshot in snapshots:
        kind = snapshot["kind"]
        if kind == "static_mesh":
            actor = apply_static_mesh(snapshot)
        elif kind == "overlay":
            actor = apply_overlay(snapshot)
        elif kind == "light":
            actor = apply_light(snapshot)
        elif kind == "post_process":
            actor = apply_post_process(snapshot)
        elif kind == "camera":
            actor = apply_camera(snapshot)
        else:
            fail("Unknown scene snapshot kind '{}'".format(kind))
        applied[snapshot["label"]] = actor

    director = find_director()
    if not director:
        fail("MainMenu is missing {}".format(DIRECTOR_LABEL))
    set_director_reference(
        director, ("intro_camera",), applied[next(
            label for label, actor in applied.items() if actor.actor_has_tag(CAMERA_TAG)
        )]
    )
    set_director_reference(
        director, ("ink_overlay",), applied[next(
            label for label, actor in applied.items() if actor.actor_has_tag(OVERLAY_TAG)
        )]
    )

    if not unreal.EditorLevelLibrary.save_current_level():
        fail("Could not save {}".format(TARGET_MAP))
    log("READY: copied {} scene actors into {}; UI and preview-only state were preserved".format(
        len(applied), TARGET_MAP
    ))


def sync():
    apply_snapshot(take_source_snapshot())


sync()
