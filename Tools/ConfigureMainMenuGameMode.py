"""Make MainMenu use its existing GameMode without spawning a default Pawn.

The asset keeps its existing parent class and PlayerController configuration.
Only DefaultPawnClass is cleared, then the MainMenu World Settings override is
pointed at this GameMode.
"""

import unreal


MAIN_MENU_MAP = "/Game/Level/MainMenu"
GAME_MODE_BP_PATH = "/Game/Blueprint/GameSystem/MainMenu/BP_MainMenuGameMode"


def log(message):
    unreal.log("[ConfigureMainMenuGameMode] " + message)


def fail(message):
    raise RuntimeError("[ConfigureMainMenuGameMode] " + message)


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


def generated_class_for(blueprint):
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    generated_class_path = "{}.{}_C".format(GAME_MODE_BP_PATH, blueprint.get_name())
    generated_class = unreal.load_class(None, generated_class_path)
    if not generated_class:
        fail("Blueprint has no generated class: {}".format(generated_class_path))
    return generated_class


def configure_game_mode():
    blueprint = unreal.EditorAssetLibrary.load_asset(GAME_MODE_BP_PATH)
    if not blueprint:
        fail("Missing GameMode Blueprint {}".format(GAME_MODE_BP_PATH))

    generated_class = generated_class_for(blueprint)
    game_mode_cdo = unreal.get_default_object(generated_class)
    if not game_mode_cdo:
        fail("Could not load GameMode CDO")

    # Do not change the Blueprint's parent, PlayerController, HUD, or other
    # existing menu behavior. Clearing this property prevents GameMode from
    # creating DefaultPawn0/PlayerCharacter when MainMenu starts.
    game_mode_cdo.set_editor_property("default_pawn_class", None)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    generated_class = generated_class_for(blueprint)
    game_mode_cdo = unreal.get_default_object(generated_class)
    if game_mode_cdo.get_editor_property("default_pawn_class") is not None:
        fail("DefaultPawnClass did not remain None after Blueprint compile")

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        fail("Could not save {}".format(GAME_MODE_BP_PATH))
    log("GameMode={} DefaultPawnClass=None".format(
        generated_class.get_path_name(),
    ))
    return generated_class


def assign_main_menu_game_mode(game_mode_class):
    load_main_menu()
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", game_mode_class)

    assigned = world_settings.get_editor_property("default_game_mode")
    if game_mode_class.get_path_name() not in str(assigned):
        fail("MainMenu GameMode override verification failed: {}".format(assigned))
    if not unreal.EditorLevelLibrary.save_current_level():
        fail("Could not save {}".format(MAIN_MENU_MAP))
    log("MainMenu World Settings GameMode override={}".format(assigned))


game_mode_class = configure_game_mode()
assign_main_menu_game_mode(game_mode_class)
log("SUCCESS")

