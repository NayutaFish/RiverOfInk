"""Reimport the authored MainMenu HUD textures into their local UE assets."""

import os

import unreal


TEXTURE_DIRECTORY = "/Game/RawContent/UI/MainMenuHUD"
TEXTURE_NAMES = (
    "T_UI_MainMenuHUD_Title_MoranKaifeng",
    "T_UI_MainMenuHUD_Button_Normal",
    "T_UI_MainMenuHUD_Button_Focus",
    "T_UI_MainMenuHUD_InkPanel",
)


def fail(message):
    unreal.log_error("[ReimportMainMenuHudTextures] {}".format(message))
    raise RuntimeError(message)


def source_file(texture_name):
    return os.path.join(
        unreal.Paths.project_content_dir(),
        "RawContent",
        "UI",
        "MainMenuHUD",
        "{}.png".format(texture_name),
    )


interchange_manager = unreal.InterchangeManager.get_interchange_manager_scripted()
if interchange_manager is None:
    fail("Unreal Interchange Manager is unavailable")


for texture_name in TEXTURE_NAMES:
    asset_path = "{}/{}".format(TEXTURE_DIRECTORY, texture_name)
    png_path = source_file(texture_name)

    if not os.path.isfile(png_path):
        fail("Source PNG is missing: {}".format(png_path))

    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        fail("Expected a Texture2D at {}, found {}".format(asset_path, texture))

    # Reimport in place: the current machine remains authoritative for the
    # .uasset container, object path and all existing references.
    import_parameters = unreal.ImportAssetParameters()
    reimported_assets = interchange_manager.reimport_asset(
        texture,
        import_parameters,
    )
    if not reimported_assets:
        fail("Unreal could not reimport {} from {}".format(asset_path, png_path))

    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        fail("Could not save {} after reimport".format(asset_path))

    unreal.log(
        "[ReimportMainMenuHudTextures] {} reimported from {} ({}x{})".format(
            asset_path,
            png_path,
            texture.blueprint_get_size_x(),
            texture.blueprint_get_size_y(),
        )
    )

unreal.log("[ReimportMainMenuHudTextures] SUCCESS")
