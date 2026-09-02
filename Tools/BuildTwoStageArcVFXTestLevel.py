"""Build the isolated TwoStageArc VFX test level in the Unreal Editor.

This script is intended to be run from Unreal Editor's Execute Python Script
command.  It is deliberately idempotent so it can be re-run while tuning the
test layout.
"""

import unreal


SOURCE_MAP = "/Game/Level/TestMap_0"
TARGET_MAP = "/Game/Level/TwoStageArcVFXTest"
CHECKER_MATERIAL_PATHS = (
    "/Engine/OpenWorldTemplate/LandscapeMaterial/MI_ProcGrid",
    "/Engine/OpenWorldTemplate/LandscapeMaterial/MI_ProcGrid.MI_ProcGrid",
    "/Engine/EngineMaterials/WorldGridMaterial",
    "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial",
)
ENEMY_CLASS_PATH = "/Game/Blueprint/GamePlay/Enemy/EnemyTest1/BP_EnemyTest1.BP_EnemyTest1_C"
SPAWN_POINT_CLASS_PATH = "/Game/Blueprint/GamePlay/LevelRoom/BP_EnemySpawnPoint.BP_EnemySpawnPoint_C"
REWARD_MANAGER_CLASS_PATH = "/Game/Blueprint/GameSystem/GameMode/Roguelike/BP_RoguelikeRewardManger.BP_RoguelikeRewardManger_C"
TEST_MANAGER_CLASS_PATH = "/Script/RiverOfInk.TwoStageArcVFXTestManager"

GENERATED_LABELS = {
    "TwoStageArcVFXTestManager",
    "TwoStageArcVFXTestSpawnPoint",
    "TwoStageArcVFXTestRewardManager",
}


def log(message):
    unreal.log("[TwoStageArcVFXTest] " + message)


def log_warning(message):
    unreal.log_warning("[TwoStageArcVFXTest] " + message)


def load_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        log_warning("Asset not found: {}".format(path))
    return asset


def load_class(path):
    actor_class = unreal.load_class(None, path)
    if not actor_class:
        log_warning("Class not found: {}".format(path))
    return actor_class


def set_editor_property(actor, property_name, value):
    try:
        actor.set_editor_property(property_name, value)
        return True
    except Exception as error:
        log_warning("Could not set {}.{}: {}".format(actor.get_name(), property_name, error))
        return False


def set_actor_transform(actor, location, rotation=None, scale=None):
    actor.set_actor_location(unreal.Vector(*location), False, False)
    if rotation is not None:
        actor.set_actor_rotation(unreal.Rotator(*rotation), False)
    if scale is not None:
        actor.set_actor_scale3d(unreal.Vector(*scale))


def find_actor_by_label(label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def find_actor_by_class_name(class_name):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_class().get_name() == class_name:
            return actor
    return None


def destroy_generated_actors():
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        if actor.get_actor_label() in GENERATED_LABELS:
            unreal.EditorLevelLibrary.destroy_actor(actor)


def duplicate_and_load_map():
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP):
        raise RuntimeError(
            "Test map {} is missing. Copy TestMap_0.umap to that path and open it before running this script."
            .format(TARGET_MAP)
        )

    current_world = unreal.EditorLevelLibrary.get_editor_world()
    current_map = current_world.get_path_name().split(".")[0] if current_world else ""
    if current_map != TARGET_MAP:
        raise RuntimeError(
            "Open {} in the editor before running this script (current map: {})."
            .format(TARGET_MAP, current_map)
        )


def configure_arena_plane():
    plane = find_actor_by_class_name("StaticMeshActor")
    if not plane:
        plane = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor,
            unreal.Vector(0.0, 0.0, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )

    plane.set_actor_label("TwoStageArcVFXTestArena")
    set_actor_transform(plane, (0.0, 0.0, 0.0), scale=(60.0, 60.0, 1.0))

    mesh_component = plane.get_component_by_class(unreal.StaticMeshComponent)
    if mesh_component:
        set_editor_property(mesh_component, "mobility", unreal.ComponentMobility.STATIC)
        basic_plane = load_asset("/Engine/BasicShapes/Plane")
        if basic_plane:
            set_editor_property(mesh_component, "static_mesh", basic_plane)

        checker_material = None
        for material_path in CHECKER_MATERIAL_PATHS:
            checker_material = load_asset(material_path)
            if checker_material:
                break
        if checker_material:
            mesh_component.set_material(0, checker_material)
            log("Arena plane material: {}".format(checker_material.get_name()))
        else:
            log_warning("No checker/grid material could be loaded for the arena plane.")

    return plane


def configure_lighting():
    directional = find_actor_by_class_name("DirectionalLight")
    if directional:
        set_actor_transform(directional, (0.0, 0.0, 0.0), rotation=(-48.0, -35.0, 0.0))
        light_component = directional.get_component_by_class(unreal.DirectionalLightComponent)
        if light_component:
            set_editor_property(light_component, "intensity", 3.0)
            set_editor_property(light_component, "light_color", unreal.Color(255, 245, 225, 255))
            set_editor_property(light_component, "mobility", unreal.ComponentMobility.MOVABLE)

    sky_light = find_actor_by_class_name("SkyLight")
    if sky_light:
        sky_component = sky_light.get_component_by_class(unreal.SkyLightComponent)
        if sky_component:
            set_editor_property(sky_component, "intensity", 1.0)
            set_editor_property(sky_component, "mobility", unreal.ComponentMobility.MOVABLE)
            set_editor_property(sky_component, "real_time_capture", True)

    if not find_actor_by_class_name("SkyAtmosphere"):
        sky_atmosphere_class = getattr(unreal, "SkyAtmosphere", None)
        if sky_atmosphere_class:
            unreal.EditorLevelLibrary.spawn_actor_from_class(
                sky_atmosphere_class,
                unreal.Vector(0.0, 0.0, 0.0),
                unreal.Rotator(0.0, 0.0, 0.0),
            )


def configure_player_start():
    player_start = find_actor_by_class_name("PlayerStart")
    if not player_start:
        player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.PlayerStart,
            unreal.Vector(-700.0, 0.0, 100.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
    else:
        set_actor_transform(player_start, (-700.0, 0.0, 100.0), rotation=(0.0, 0.0, 0.0))
    player_start.set_actor_label("TwoStageArcVFXTestPlayerStart")


def configure_existing_managers():
    demo_room_manager = find_actor_by_class_name("BP_DemoRoomManager_C")
    if demo_room_manager:
        # Keep the ordinary level manager in the test level, but prevent its
        # finite combat-room flow from competing with the infinite test loop.
        set_editor_property(demo_room_manager, "auto_start", False)
        demo_room_manager.set_actor_label("BP_DemoRoomManager_TestDisabled")

    reward_manager = find_actor_by_class_name("BP_RoguelikeRewardManger_C")
    if not reward_manager:
        reward_class = load_class(REWARD_MANAGER_CLASS_PATH)
        if reward_class:
            reward_manager = unreal.EditorLevelLibrary.spawn_actor_from_class(
                reward_class,
                unreal.Vector(0.0, 0.0, 0.0),
                unreal.Rotator(0.0, 0.0, 0.0),
            )
    if reward_manager:
        reward_manager.set_actor_label("TwoStageArcVFXTestRewardManager")
        # This level is focused on VFX and has no room-clear reward flow.
        set_editor_property(reward_manager, "auto_create_exit_trigger", False)


def create_test_loop():
    spawn_point_class = load_class(SPAWN_POINT_CLASS_PATH)
    enemy_class = load_class(ENEMY_CLASS_PATH)
    test_manager_class = load_class(TEST_MANAGER_CLASS_PATH)
    if not spawn_point_class or not enemy_class or not test_manager_class:
        raise RuntimeError("Required test classes are unavailable.")

    spawn_point = unreal.EditorLevelLibrary.spawn_actor_from_class(
        spawn_point_class,
        unreal.Vector(250.0, 0.0, 100.0),
        unreal.Rotator(0.0, 180.0, 0.0),
    )
    spawn_point.set_actor_label("TwoStageArcVFXTestSpawnPoint")
    set_editor_property(spawn_point, "spawn_radius", 0.0)
    set_editor_property(spawn_point, "fade_interp_speed", 4.0)

    manager = unreal.EditorLevelLibrary.spawn_actor_from_class(
        test_manager_class,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    manager.set_actor_label("TwoStageArcVFXTestManager")
    set_editor_property(manager, "enemy_class", enemy_class)
    set_editor_property(manager, "spawn_point", spawn_point)
    set_editor_property(manager, "initial_spawn_delay", 0.25)
    set_editor_property(manager, "respawn_delay", 0.75)
    set_editor_property(manager, "freeze_test_enemy", True)

    return spawn_point, manager


def build_level():
    duplicate_and_load_map()
    destroy_generated_actors()
    configure_arena_plane()
    configure_lighting()
    configure_player_start()
    configure_existing_managers()
    spawn_point, manager = create_test_loop()

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save the test level.")

    log("READY map={} actors={} spawn={} manager={}".format(
        TARGET_MAP,
        len(unreal.EditorLevelLibrary.get_all_level_actors()),
        spawn_point.get_name(),
        manager.get_name(),
    ))


build_level()
