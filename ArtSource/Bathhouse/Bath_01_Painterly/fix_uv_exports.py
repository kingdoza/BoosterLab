import bpy
import math
import json
from pathlib import Path
root=Path(r'C:\UnrealProjects\BathhouseSim\ArtSource\Bathhouse\Bath_01_Painterly')
bpy.ops.wm.open_mainfile(filepath=str(root/'Bath_01_Painterly.blend'))
names=['SM_Bath_01_Painterly_'+n for n in ['Body','Hardware','Details','Water']]
objects=[bpy.data.objects[n] for n in names]
def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:obj.hide_set(False);obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
for obj in objects:
    if [u.name for u in obj.data.uv_layers]==['UV0','UV1_Lightmap']:continue
    select([obj])
    keep=obj.data.uv_layers['UV0']
    for layer in list(obj.data.uv_layers):
        if layer!=keep:obj.data.uv_layers.remove(layer)
    obj.data.uv_layers.new(name='UV1_Lightmap',do_init=True)
    obj.data.uv_layers.active_index=1
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.007)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.data.uv_layers.active_index=0;obj.data.uv_layers[0].active_render=True

collisions=[o for o in bpy.data.objects if o.name.startswith('UCX_')]
for filename,parts in [('SM_Bath_01_Painterly_Body.fbx',[objects[0]]+collisions),
                       ('SM_Bath_01_Painterly_Hardware.fbx',objects[1:3]),
                       ('SM_Bath_01_Painterly_Water.fbx',[objects[3]])]:
    select(parts)
    bpy.ops.export_scene.fbx(filepath=str(root/filename),use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
        bake_space_transform=False,use_mesh_modifiers=True,mesh_smooth_type='FACE',
        path_mode='COPY',embed_textures=True,add_leaf_bones=False)
for obj in collisions:obj.hide_set(True)
select(objects)
bpy.ops.export_scene.gltf(filepath=str(root/'Bath_01_Painterly.glb'),export_format='GLB',use_selection=True,
    export_apply=True,export_texcoords=True,export_normals=True,export_materials='EXPORT')
bpy.ops.wm.save_as_mainfile(filepath=str(root/'Bath_01_Painterly.blend'))
manifest=json.loads((root/'asset_manifest.json').read_text())
for obj in objects:manifest[obj.name]['uv_channels']=[u.name for u in obj.data.uv_layers]
(root/'asset_manifest.json').write_text(json.dumps(manifest,indent=2))
result={'uv_channels':{o.name:[u.name for u in o.data.uv_layers] for o in objects}}
