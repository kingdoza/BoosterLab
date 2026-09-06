import bpy
import json
from pathlib import Path

root=Path(r'C:\UnrealProjects\BathhouseSim\ArtSource\Bathhouse\Bath_01_Painterly')
names=['SM_Bath_01_Painterly_'+n for n in ['Body','Hardware','Details','Water']]
materials={}
for name in names:
    for mat in bpy.data.objects[name].data.materials:
        if mat.name in materials:continue
        p=mat.node_tree.nodes.get('Principled BSDF')
        textures=[n.image for n in mat.node_tree.nodes if n.type=='TEX_IMAGE' and n.image]
        record={'base_color_linear':list(mat.diffuse_color[:3]),
            'roughness':p.inputs['Roughness'].default_value,
            'metallic':p.inputs['Metallic'].default_value,
            'transmission_weight':p.inputs['Transmission Weight'].default_value,
            'ior':p.inputs['IOR'].default_value}
        for semantic,suffix in [('base_color_texture','_BaseColor'),('roughness_texture','_Roughness')]:
            match=next((i for i in textures if i.name.endswith(suffix)),None)
            record[semantic]=('textures/'+Path(match.filepath).name) if match else None
        materials[mat.name]=record
(root/'materials.json').write_text(json.dumps(materials,indent=2),encoding='utf-8')
manifest=json.loads((root/'asset_manifest.json').read_text())
validation=json.loads((root/'validation_report.json').read_text())
manifest['measured_bounds_m']={k:v['bounds_m'] for k,v in validation['components'].items()}
manifest['total_render_triangles']=sum(len(bpy.data.objects[n].data.loop_triangles) for n in names)
manifest['overall_depth_with_steps_m']=4.4255
manifest['variant']='Painterly surface refinement; separate from Bath_01'
manifest['uv0']='Intentional face-local overlap for repeating painted surfaces'
manifest['uv1']='Separate packed lightmap UV'
manifest['comparison']='Same camera, lights and exposure as Bath_01'
(root/'asset_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
result={'materials':len(materials),'render_triangles':manifest['total_render_triangles']}
