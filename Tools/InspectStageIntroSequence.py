"""Read-only diagnostic for the Stage 01 opening Level Sequence."""

import unreal


SEQUENCE_PATH = "/Game/Presentation/StageIntro/LS_Stage01Intro"
MAIN_MENU_MAP = "/Game/Level/MainMenu"


def log(message):
    unreal.log("[InspectStageIntroSequence] " + message)


sequence = unreal.EditorAssetLibrary.load_asset(SEQUENCE_PATH)
if not sequence:
    raise RuntimeError("Missing sequence: {}".format(SEQUENCE_PATH))

movie_scene = sequence.get_movie_scene()
log("key interpolation enum={}".format(
    ", ".join(name for name in dir(unreal.MovieSceneKeyInterpolation) if name.isupper())
))
log("sequence={} movie_scene={}".format(sequence.get_name(), movie_scene.get_name()))
log("sequence timing start={} end={} display_rate={} tick_resolution={}".format(
    sequence.get_playback_start(),
    sequence.get_playback_end(),
    sequence.get_display_rate(),
    sequence.get_tick_resolution(),
))
log("sequence timing api={}".format(
    ", ".join(
        name for name in dir(sequence)
        if any(token in name.lower() for token in ("playback", "display_rate", "tick_resolution"))
    )
))
log("movie_scene timing api={}".format(
    ", ".join(
        name for name in dir(movie_scene)
        if any(token in name.lower() for token in ("playback", "display_rate", "tick_resolution"))
    )
))

for binding in sequence.get_bindings():
    log("binding name={} id={}".format(binding.get_name(), binding.get_id()))
    for track in binding.get_tracks():
        log("  track class={} name={}".format(track.get_class().get_name(), track.get_name()))
        for index, section in enumerate(track.get_sections()):
            log("    section[{}] class={} api={}".format(
                index,
                section.get_class().get_name(),
                ", ".join(
                    name for name in dir(section)
                    if any(token in name.lower() for token in ("range", "start", "end"))
                ),
            ))
            log("      section curve api={}".format(
                ", ".join(
                    name for name in dir(section)
                    if any(token in name.lower() for token in ("channel", "curve", "key"))
                ),
            ))
            for property_name in ("channel_proxy", "channels", "transform_mask"):
                try:
                    log("      property {}={}".format(
                        property_name, section.get_editor_property(property_name)
                    ))
                except Exception as error:
                    log("      property {} unavailable: {}".format(property_name, error))
            for channel_index, channel in enumerate(section.get_all_channels()):
                keys = channel.get_keys()
                key_data = []
                for key in keys:
                    try:
                        key_data.append("{}={}".format(key.get_time(), key.get_value()))
                    except Exception:
                        key_data.append(str(key.get_time()))
                log("      channel[{}] {} keys={}".format(
                    channel_index, channel.get_class().get_name(), "; ".join(key_data)
                ))
                if channel_index == 0:
                    log("      channel api={}".format(
                        ", ".join(
                            name for name in dir(channel)
                            if any(token in name.lower() for token in ("key", "default", "remove"))
                        ),
                    ))
                    log("      add_key signature={}".format(channel.add_key.__doc__))
                    if keys:
                        log("      key api={}".format(
                            ", ".join(
                                name for name in dir(keys[0])
                                if any(token in name.lower() for token in ("time", "value", "interpolation", "remove"))
                            ),
                        ))

if not unreal.EditorLoadingAndSavingUtils.load_map(MAIN_MENU_MAP):
    raise RuntimeError("Could not load {}".format(MAIN_MENU_MAP))

for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if actor.actor_has_tag("Stage01IntroCamera") or actor.actor_has_tag("Stage01InkOverlay"):
        loc = actor.get_actor_location()
        rot = actor.get_actor_rotation()
        log("map actor={} tags={} loc=({:.6f},{:.6f},{:.6f}) rot=(pitch={:.6f},yaw={:.6f},roll={:.6f})".format(
            actor.get_actor_label(), actor.tags, loc.x, loc.y, loc.z, rot.pitch, rot.yaw, rot.roll
        ))
    if "Stage01IntroDirector" in actor.get_class().get_name():
        values = []
        for prop in ("intro_duration", "ink_effect_start_time", "ink_effect_duration"):
            try:
                values.append("{}={}".format(prop, actor.get_editor_property(prop)))
            except Exception as error:
                values.append("{}=<{}>".format(prop, error))
        log("map director={} {}".format(actor.get_name(), ", ".join(values)))
