import unreal


paths = (
    "/Game/RawContent/VFX/NiagaraSystem/NS/PlayerTwoStageArc/Materials/M_TwoStageArc_Crescent_Animated",
    "/Game/RawContent/VFX/NiagaraSystem/NS/PlayerTwoStageArc/Materials/M_TwoStageArc_Crescent_Animated_Mirror",
)

for path in paths:
    material = unreal.load_asset(path)
    if material is None:
        unreal.log_error("Missing material: {}".format(path))
        continue

    unreal.log("MATERIAL {}".format(path))
    unreal.log("SCALARS {}".format(unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)))
    unreal.log("TEXTURES {}".format(unreal.MaterialEditingLibrary.get_texture_parameter_names(material)))
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    unreal.log("EXPRESSION_COUNT {}".format(len(expressions)))
    for index, expression in enumerate(expressions):
        cls = expression.get_class().get_name()
        name = unreal.MaterialEditingLibrary.get_name(expression)
        pos = unreal.MaterialEditingLibrary.get_material_expression_node_position(expression)
        unreal.log("EXPR {} {} name={} pos={}".format(index, cls, name, pos))
        try:
            names = unreal.MaterialEditingLibrary.get_material_expression_input_names(expression)
            inputs = unreal.MaterialEditingLibrary.get_inputs_for_material_expression(expression)
            unreal.log("  INPUTS names={} values={}".format(names, inputs))
        except Exception as exc:
            unreal.log_warning("  INPUTS_FAILED {}".format(exc))

    for property_name in ("MP_EmissiveColor", "MP_Opacity", "MP_OpacityMask", "MP_WorldPositionOffset"):
        try:
            expression = unreal.MaterialEditingLibrary.get_material_property_input_node(material, getattr(unreal.MaterialProperty, property_name))
            unreal.log("PROPERTY {} -> {}".format(property_name, expression))
        except Exception as exc:
            unreal.log_warning("PROPERTY_FAILED {} {}".format(property_name, exc))
