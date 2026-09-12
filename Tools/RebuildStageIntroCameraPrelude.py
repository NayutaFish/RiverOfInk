"""Rebuild the MainMenu Stage 01 camera prelude from approved camera poses.

The sequence moves from the menu's clean establishing view to the supplied
InkOverlay close view over 2.4 seconds.  Ink starts only once that move has
settled; the sequence then holds its final camera pose while the existing
3.5-second overlay effect completes.
"""

import math
import unreal


MAIN_MENU_MAP = "/Game/Level/MainMenu"
SEQUENCE_PATH = "/Game/Presentation/StageIntro/LS_Stage01Intro"
CAMERA_TAG = "Stage01IntroCamera"
OVERLAY_TAG = "Stage01InkOverlay"
DIRECTOR_CLASS_TOKEN = "Stage01IntroDirector"
CAMERA_BINDING_NAME = "Stage01_CineCamera"

FRAME_RATE = 30
CAMERA_END_FRAME = 72          # 2.4 seconds
SEQUENCE_END_FRAME = 177       # 2.4s camera + 3.5s ink
CAMERA_KEY_FRAMES = (0, 18, 42, 60, CAMERA_END_FRAME)

OVERLAY_LOCATION = (-141.536652, 127.713942, 21.213441)
START_LOCATION = (-1469.295071, 73.494629, 1396.751412)
START_ROTATION = (0.0, -47.599900, -5.802592)  # Roll, Pitch, Yaw
END_LOCATION = (-371.666996, 132.147541, 581.247407)
END_ROTATION = (0.0, -68.799900, 0.198860)     # Roll, Pitch, Yaw


def log(message):
    unreal.log("[RebuildStageIntroCameraPrelude] " + message)


def fail(message):
    raise RuntimeError("[RebuildStageIntroCameraPrelude] " + message)


def current_map_path():
    world = unreal.EditorLevelLibrary.get_editor_world()
    return world.get_path_name().split(".")[0] if world else ""


def load_main_menu():
    if current_map_path() == MAIN_MENU_MAP:
        return
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAIN_MENU_MAP):
        fail("Could not load {}".format(MAIN_MENU_MAP))
    if current_map_path() != MAIN_MENU_MAP:
        fail("Loaded {}, expected {}".format(current_map_path(), MAIN_MENU_MAP))


def find_actor_by_tag(tag):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.actor_has_tag(tag):
            return actor
    return None


def find_director():
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if DIRECTOR_CLASS_TOKEN in actor.get_class().get_name():
            return actor
    return None


def set_property(actor, property_name, value):
    try:
        actor.set_editor_property(property_name, value)
    except Exception as error:
        fail("Could not set {} on {}: {}".format(
            property_name, actor.get_name(), error
        ))


def vector_distance(location, expected):
    return math.sqrt(
        (location.x - expected[0]) ** 2
        + (location.y - expected[1]) ** 2
        + (location.z - expected[2]) ** 2
    )


def smoothstep(value):
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def lerp_tuple(start, end, alpha):
    return tuple(start[index] + (end[index] - start[index]) * alpha for index in range(3))


def pose_for_frame(frame):
    if frame == 0:
        return START_LOCATION, START_ROTATION
    if frame == CAMERA_END_FRAME:
        return END_LOCATION, END_ROTATION
    alpha = smoothstep(float(frame) / float(CAMERA_END_FRAME))
    return (
        lerp_tuple(START_LOCATION, END_LOCATION, alpha),
        lerp_tuple(START_ROTATION, END_ROTATION, alpha),
    )


def get_transform_channels(sequence):
    binding = None
    for candidate in sequence.get_bindings():
        if candidate.get_name() == CAMERA_BINDING_NAME:
            binding = candidate
            break
    if not binding:
        fail("Could not find camera binding {}".format(CAMERA_BINDING_NAME))

    transform_track = None
    for track in binding.get_tracks():
        if track.get_class().get_name() == "MovieScene3DTransformTrack":
            transform_track = track
            break
    if not transform_track:
        fail("Camera binding has no MovieScene3DTransformTrack")

    sections = transform_track.get_sections()
    if len(sections) != 1:
        fail("Expected one camera transform section, found {}".format(len(sections)))

    section = sections[0]
    channels = list(section.get_all_channels())
    if len(channels) < 6:
        fail("Expected 6 transform channels, found {}".format(len(channels)))
    return section, channels


def rebuild_transform_track(sequence):
    display_rate = sequence.get_display_rate()
    if display_rate.numerator != FRAME_RATE or display_rate.denominator != 1:
        fail("Expected {}fps sequence, found {}".format(FRAME_RATE, display_rate))

    section, channels = get_transform_channels(sequence)
    for channel in channels[:6]:
        for key in list(channel.get_keys()):
            channel.remove_key(key)

    for frame in CAMERA_KEY_FRAMES:
        location, rotation = pose_for_frame(frame)
        # Transform section rotation channels are X/Y/Z = Roll/Pitch/Yaw.
        values = (
            location[0], location[1], location[2],
            rotation[0], rotation[1], rotation[2],
        )
        for channel, value in zip(channels[:6], values):
            channel.add_key(
                unreal.FrameNumber(frame),
                value,
                0.0,
                unreal.MovieSceneTimeUnit.DISPLAY_RATE,
                unreal.MovieSceneKeyInterpolation.AUTO,
            )
        log("K{} @ {:.3f}s loc=({:.3f},{:.3f},{:.3f}) rot=(Pitch={:.3f},Yaw={:.3f},Roll={:.3f})".format(
            frame,
            float(frame) / FRAME_RATE,
            location[0], location[1], location[2],
            rotation[1], rotation[2], rotation[0],
        ))

    section.set_range(0, SEQUENCE_END_FRAME)
    sequence.set_playback_start(0)
    sequence.set_playback_end(SEQUENCE_END_FRAME)


def configure_main_menu_instance():
    camera = find_actor_by_tag(CAMERA_TAG)
    overlay = find_actor_by_tag(OVERLAY_TAG)
    director = find_director()
    if not camera or not overlay or not director:
        fail("Missing MainMenu camera={}, overlay={}, director={}".format(
            bool(camera), bool(overlay), bool(director)
        ))

    overlay_error = vector_distance(overlay.get_actor_location(), OVERLAY_LOCATION)
    if overlay_error > 0.01:
        fail("Overlay location changed by {:.3f}uu; expected approved coordinate {}".format(
            overlay_error, OVERLAY_LOCATION
        ))

    camera.set_actor_location_and_rotation(
        unreal.Vector(*START_LOCATION),
        unreal.Rotator(roll=START_ROTATION[0], pitch=START_ROTATION[1], yaw=START_ROTATION[2]),
        False,
        False,
    )
    set_property(director, "camera_transition_duration", CAMERA_END_FRAME / float(FRAME_RATE))
    set_property(director, "ink_effect_start_time", CAMERA_END_FRAME / float(FRAME_RATE))
    set_property(director, "intro_duration", SEQUENCE_END_FRAME / float(FRAME_RATE))
    log("MainMenu K0 actor and director timing updated; ink starts at {:.3f}s, sequence ends at {:.3f}s.".format(
        CAMERA_END_FRAME / float(FRAME_RATE),
        SEQUENCE_END_FRAME / float(FRAME_RATE),
    ))


def rebuild():
    load_main_menu()
    sequence = unreal.EditorAssetLibrary.load_asset(SEQUENCE_PATH)
    if not sequence:
        fail("Missing sequence {}".format(SEQUENCE_PATH))

    rebuild_transform_track(sequence)
    configure_main_menu_instance()

    if not unreal.EditorAssetLibrary.save_loaded_asset(sequence):
        fail("Could not save {}".format(SEQUENCE_PATH))
    if not unreal.EditorLevelLibrary.save_current_level():
        fail("Could not save {}".format(MAIN_MENU_MAP))
    log("SUCCESS")


rebuild()

