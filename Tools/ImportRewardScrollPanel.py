"""Import the reward screen's shared Bian River xuan-paper panel texture."""

import os

import unreal


SOURCE_PATH = os.path.join(
    unreal.Paths.project_content_dir(),
    "RawContent", "UI", "Reward", "Textures", "T_UI_Reward_ScrollPanel.png"
)
DESTINATION_DIRECTORY = "/Game/RawContent/UI/Reward/Textures"
ASSET_NAME = "T_UI_Reward_ScrollPanel"
ASSET_PATH = DESTINATION_DIRECTORY + "/" + ASSET_NAME


def fail(message):
    unreal.log_error("[ImportRewardScrollPanel] {}".format(message))
    raise RuntimeError(message)


if not os.path.isfile(SOURCE_PATH):
    fail("Source PNG is missing: {}".format(SOURCE_PATH))

texture = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
if texture is None:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SOURCE_PATH)
    task.set_editor_property("destination_path", DESTINATION_DIRECTORY)
    task.set_editor_property("destination_name", ASSET_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
else:
    interchange_manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    if interchange_manager is None:
        fail("Unreal Interchange Manager is unavailable")
    reimported_assets = interchange_manager.reimport_asset(
        texture,
        unreal.ImportAssetParameters(),
    )
    if not reimported_assets:
        fail("Could not reimport {}".format(ASSET_PATH))

if not isinstance(texture, unreal.Texture2D):
    fail("Texture import failed: {}".format(SOURCE_PATH))

compression = getattr(unreal.TextureCompressionSettings, "TC_USER_INTERFACE2D", None)
if compression is None:
    compression = getattr(unreal.TextureCompressionSettings, "TC_EDITOR_ICON", None)
mip_gen = getattr(unreal.TextureMipGenSettings, "TMGS_NO_MIPMAPS", None)
if compression is not None:
    texture.set_editor_property("compression_settings", compression)
if mip_gen is not None:
    texture.set_editor_property("mip_gen_settings", mip_gen)
texture.set_editor_property("srgb", True)
texture.set_editor_property("never_stream", True)
texture.set_editor_property("lod_bias", 0)

if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
    fail("Could not save {}".format(ASSET_PATH))

unreal.log(
    "[ImportRewardScrollPanel] SUCCESS {} ({}x{})".format(
        ASSET_PATH,
        texture.blueprint_get_size_x(),
        texture.blueprint_get_size_y(),
    )
)