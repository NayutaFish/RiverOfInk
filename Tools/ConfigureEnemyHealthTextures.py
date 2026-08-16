import unreal


def choose_enum(enum_type, names):
    for name in names:
        value = getattr(enum_type, name, None)
        if value is not None:
            return value
    return None


compression = choose_enum(
    unreal.TextureCompressionSettings,
    ("TC_USER_INTERFACE2D", "TC_EDITOR_ICON", "TC_DEFAULT"),
)
mip_gen = choose_enum(
    unreal.TextureMipGenSettings,
    ("TMGS_NO_MIPMAPS", "TMGS_FROM_TEXTURE_GROUP"),
)

for asset_path in (
    "/Game/RawContent/UI/Health/Textures/T_UI_EnemyHealth_Normal_Frame",
    "/Game/RawContent/UI/Health/Textures/T_UI_EnemyHealth_Elite_Frame",
):
    texture = unreal.load_asset(asset_path)
    if texture is None:
        unreal.log_error("Enemy health texture missing: {}".format(asset_path))
        continue

    if compression is not None:
        texture.set_editor_property("compression_settings", compression)
    if mip_gen is not None:
        texture.set_editor_property("mip_gen_settings", mip_gen)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("lod_bias", 0)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.log(
        "Configured enemy health texture: {} compression={} mip_gen={} srgb=True never_stream=True".format(
            asset_path, compression, mip_gen
        )
    )
