import bpy
import json
from pathlib import Path

root=Path(r'C:\UnrealProjects\BathhouseSim\ArtSource\Bathhouse\Bath_01')
names=['SM_Bath_01_Body','SM_Bath_01_Hardware','SM_Bath_01_Details','SM_Bath_01_Water']
materials={}
for name in names:
    for mat in bpy.data.objects[name].data.materials:
        if mat.name in materials:continue
        p=mat.node_tree.nodes.get('Principled BSDF')
        textures=[n.image for n in mat.node_tree.nodes if n.type=='TEX_IMAGE' and n.image]
        materials[mat.name]={
            'base_color_linear':list(mat.diffuse_color[:3]),
            'base_color_texture':('textures/'+Path(textures[0].filepath).name) if textures else None,
            'roughness':p.inputs['Roughness'].default_value,
            'metallic':p.inputs['Metallic'].default_value,
            'transmission_weight':p.inputs['Transmission Weight'].default_value,
            'ior':p.inputs['IOR'].default_value}
(root/'materials.json').write_text(json.dumps(materials,indent=2),encoding='utf-8')
manifest=json.loads((root/'asset_manifest.json').read_text())
validation=json.loads((root/'validation_report.json').read_text())
manifest['measured_bounds_m']={k:v['bounds_m'] for k,v in validation['components'].items()}
manifest['total_render_triangles']=sum(len(bpy.data.objects[n].data.loop_triangles) for n in names)
manifest['overall_depth_with_steps_m']=4.4255
(root/'asset_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
result={'materials':len(materials),'render_triangles':manifest['total_render_triangles']}
