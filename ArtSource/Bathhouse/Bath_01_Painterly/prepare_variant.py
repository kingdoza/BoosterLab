"""Read the original builder without modifying it; write a self-contained variant builder."""
from pathlib import Path
src=Path(r'C:\UnrealProjects\BathhouseSim\ArtSource\Bathhouse\Bath_01')
out=src.with_name('Bath_01_Painterly')
code=(src/'build_bath.py').read_text(encoding='utf-8').replace('Bath_01','Bath_01_Painterly')
start=code.index('def material(')
end=code.index("grout=material(",start)
code=code[:start]+"import sys\nsys.path.insert(0,str(ROOT))\nfrom painterly_materials import material, prepare_uv\n\n"+code[end:]
code=code.replace("    objects=groups[key]\n    select_only(objects)","    objects=groups[key]\n    prepare_uv(objects)\n    select_only(objects)")
code=code.replace("    bpy.ops.object.convert(target='MESH')\n    bpy.ops.object.join()", "    bpy.ops.object.convert(target='MESH')\n    for part in bpy.context.selected_objects:\n        if part.data.uv_layers:\n            keep=part.data.uv_layers.get('UV0') or part.data.uv_layers[0]\n            for layer in list(part.data.uv_layers):\n                if layer != keep:part.data.uv_layers.remove(layer)\n            keep.name='UV0'\n    bpy.ops.object.join()")
old="""    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.007)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.data.uv_layers.active.name='UV0'
    # Non-overlapping secondary UV channel for Unreal lightmaps.
    obj.data.uv_layers.new(name='UV1_Lightmap',do_init=True)
    obj.data.uv_layers.active_index=0"""
new="""    if not obj.data.uv_layers:
        obj.data.uv_layers.new(name='UV0')
    obj.data.uv_layers[0].name='UV0'
    obj.data.uv_layers.new(name='UV1_Lightmap',do_init=True)
    obj.data.uv_layers.active_index=1
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.007)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.data.uv_layers.active_index=0
    obj.data.uv_layers[0].active_render=True"""
assert old in code
code=code.replace(old,new)
(out/'build_bath_painterly.py').write_text(code,encoding='utf-8')
validation=(src/'validate_bath.py').read_text(encoding='utf-8').replace('Bath_01','Bath_01_Painterly')
(out/'validate_bath.py').write_text(validation,encoding='utf-8')
