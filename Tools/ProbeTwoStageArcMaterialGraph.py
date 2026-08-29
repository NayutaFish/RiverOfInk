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
    unreal.log("=== GRAPH {} ===".format(path))
    for index, expression in enumerate(unreal.MaterialEditingLibrary.get_material_expressions(material)):
        cls = expression.get_class().get_name()
        try:
            inputs = unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, expression)
        except Exception as exc:
            inputs = "INPUTS_ERROR {}".format(exc)
        try:
            input_names = unreal.MaterialEditingLibrary.get_material_expression_input_names(expression)
        except Exception as exc:
            input_names = "NAMES_ERROR {}".format(exc)
        try:
            output_names = unreal.MaterialEditingLibrary.get_material_expression_output_names(expression)
        except Exception as exc:
            output_names = "OUTPUTS_ERROR {}".format(exc)
        unreal.log("EXPR {} {} inputs={} input_names={} output_names={}".format(index, cls, inputs, input_names, output_names))

unreal.log("MATERIAL_PROPERTY_NAMES {}".format([name for name in dir(unreal.MaterialProperty) if name.startswith("MP_")]))
unreal.log("SMOOTHSTEP_CLASS {}".format([name for name in dir(unreal.MaterialExpressionSmoothStep) if not name.startswith("_")]))
unreal.log("TEXSAMPLE_CLASS {}".format([name for name in dir(unreal.MaterialExpressionTextureSampleParameter2D) if not name.startswith("_")]))
unreal.log("SCALAR_CLASS {}".format([name for name in dir(unreal.MaterialExpressionScalarParameter) if not name.startswith("_")]))
