"""Configure the isolated ink-overlay preview level.

Run from Unreal Editor with Execute Python Script after opening
/Game/Level/InkOverlayPreview.
"""

import unreal


TARGET_MAP = "/Game/Level/InkOverlayPreview"
SEQUENCE_PATH = "/Game/Presentation/StageIntro/LS_Stage01Intro"
DIRECTOR_CLASS_PATH = "/Game/Presentation/StageIntro/BP_Stage01IntroDirector.BP_Stage01IntroDirector_C"
DIRECTOR_LABEL = "Stage01IntroDirector"
CAMERA_TAG = "Stage01IntroCamera"
OVERLAY_TAG = "Stage01InkOverlay"
PREVIEW_CAMERA_OFFSET = 3200.0


def log(message):
    unreal.log("[InkOverlayPreview] " + message)


def warn(message):
    unreal.log_warning("[InkOverlayPreview] " + message)


def load_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        warn("Asset not found: {}".format(path))
    return asset


def load_class(path):
    actor_class = unreal.load_class(None, path)
    if not actor_class:
        warn("Class not found: {}".format(path))
    return actor_class


def current_map_path():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        return ""
    return world.get_path_name().split(".")[0]


def ensure_target_map_loaded():
    if current_map_path() == TARGET_MAP:
        return

    if not unreal.EditorLoadingAndSavingUtils.load_map(TARGET_MAP):
        raise RuntimeError("Could not load {}".format(TARGET_MAP))

    if current_map_path() != TARGET_MAP:
        raise RuntimeError(
            "Expected current map {} but found {}".format(
                TARGET_MAP, current_map_path()
            )
        )


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
        class_name = candidate.get_class().get_name()
        if "Stage01IntroDirector" in class_name:
            return candidate
    return None


def set_property(actor, names, value):
    for name in names:
        try:
            actor.set_editor_property(name, value)
            return name
        except Exception:
            pass
    raise RuntimeError(
        "Could not set {} on {}".format(",".join(names), actor.get_name())
    )


def configure_director():
    director = find_director()
    if not director:
        raise RuntimeError(
            "BP_Stage01IntroDirector was not found in {}".format(TARGET_MAP)
        )

    sequence = load_asset(SEQUENCE_PATH)
    camera = find_actor_by_tag(CAMERA_TAG)
    overlay = find_actor_by_tag(OVERLAY_TAG)

    if not sequence or not camera or not overlay:
        raise RuntimeError(
            "Required intro references are missing: sequence={}, camera={}, overlay={}".format(
                bool(sequence), bool(camera), bool(overlay)
            )
        )

    set_property(director, ("intro_sequence",), sequence)
    set_property(director, ("intro_camera",), camera)
    set_property(director, ("ink_overlay",), overlay)
    set_property(director, ("preview_only", "b_preview_only"), True)
    set_property(director, ("auto_play_on_begin_play", "b_auto_play_on_begin_play"), True)
    set_property(director, ("auto_play_delay",), 0.25)

    set_property(director, ("preview_use_static_camera", "b_preview_use_static_camera"), True)
    set_property(director, ("preview_loop", "b_preview_loop"), True)
    set_property(director, ("preview_loop_delay",), 0.35)

    overlay_location = overlay.get_actor_location()
    camera_location = unreal.Vector(
        overlay_location.x,
        overlay_location.y,
        overlay_location.z + PREVIEW_CAMERA_OFFSET,
    )
    camera.set_actor_location_and_rotation(
        camera_location,
        unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0),
        False,
        False,
    )
    saved_location = camera.get_actor_location()
    saved_rotation = camera.get_actor_rotation()
    log(
        "Configured director={} camera={} overlay={} loc=({:.1f},{:.1f},{:.1f}) rot=(roll={:.1f},pitch={:.1f},yaw={:.1f})".format(
            director.get_name(), camera.get_name(), overlay.get_name(),
            saved_location.x, saved_location.y, saved_location.z,
            saved_rotation.roll, saved_rotation.pitch, saved_rotation.yaw,
        )
    )


def build_level():
    ensure_target_map_loaded()
    configure_director()

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(TARGET_MAP))

    log("READY map={} auto_play=True preview_only=True".format(TARGET_MAP))


build_level()