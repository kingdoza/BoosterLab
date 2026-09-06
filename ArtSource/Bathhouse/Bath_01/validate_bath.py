"""Reload the saved source and round-trip each FBX in a temporary Blender scene."""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector

root=Path(r'C:\UnrealProjects\BathhouseSim\ArtSource\Bathhouse\Bath_01')
bpy.ops.wm.open_mainfile(filepath=str(root/'Bath_01.blend'))
source_scene=bpy.context.scene
names=['SM_Bath_01_Body','SM_Bath_01_Hardware','SM_Bath_01_Details','SM_Bath_01_Water']
report={'source_reload':True,'checks':[],'components':{}}

def check(label,condition,detail=None):
    report['checks'].append({'check':label,'passed':bool(condition),'detail':detail})

def bounds(obj):
    points=[obj.matrix_world @ Vector(p) for p in obj.bound_box]
    return [[min(p[k] for p in points) for k in range(3)], [max(p[k] for p in points) for k in range(3)]]

source_bounds={name:bounds(bpy.data.objects[name]) for name in names}
for name in names:
    obj=bpy.data.objects[name]
    check(name+' finite vertices',all(math.isfinite(v) for p in obj.data.vertices for v in p.co))
    check(name+' UV channels',len(obj.data.uv_layers)==2)
    check(name+' applied scale',all(abs(v-1)<1e-6 for v in obj.scale))
    check(name+' shared origin',obj.location.length<1e-6)
    check(name+' valid material indices',all(p.material_index<len(obj.data.materials) for p in obj.data.polygons))
    report['components'][name]={'bounds_m':source_bounds[name]}

check('source images packed',all(i.packed_file is not None for i in bpy.data.images if i.name.startswith('M_')))
source_count=len(bpy.data.objects)
temp=bpy.data.scenes.new('Temporary FBX roundtrip validation')
bpy.context.window.scene=temp
imported=[]
for filename,expected in [('SM_Bath_01_Body.fbx',13),('SM_Bath_01_Hardware.fbx',2),('SM_Bath_01_Water.fbx',1)]:
    before=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(root/filename))
    added=list(set(bpy.data.objects)-before);imported.extend(added)
    check(filename+' mesh count',len(added)==expected, len(added))
    for obj in added:
        if obj.name.startswith('UCX_'):
            bm=bmesh.new();bm.from_mesh(obj.data)
            check(obj.name+' closed collision',all(e.is_manifold for e in bm.edges) and abs(bm.calc_volume())>1e-6)
            bm.free()
        else:
            name=next((n for n in names if obj.name.startswith(n)),None)
            check(obj.name+' recognized',name is not None)
            if name:
                actual=bounds(obj)
                error=max(abs(actual[i][k]-source_bounds[name][i][k]) for i in (0,1) for k in range(3))
                check(name+' FBX bounds error < 0.1 mm',error<.0001,error)
                check(name+' FBX UV channels',len(obj.data.uv_layers)==2)
for obj in imported:bpy.data.objects.remove(obj,do_unlink=True)
bpy.context.window.scene=source_scene
bpy.data.scenes.remove(temp)
check('temporary objects removed',len(bpy.data.objects)==source_count)
for filename in ['Bath_01_Hero.png','Bath_01_Empty.png','Bath_01.glb']:
    check(filename+' exists', (root/filename).is_file() and (root/filename).stat().st_size>1000)
report['passed']=all(c['passed'] for c in report['checks'])
report['limitations']=['FBX roundtrip validated in Blender, not imported into Unreal Editor.',
 'Unreal water shader must be assigned independently; FBX does not preserve Cycles transmission.',
 'UV1 contains packed geometry islands; lightmap resolution/padding still needs Unreal lighting validation.']
(root/'validation_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
result={'passed':report['passed'],'checks':len(report['checks']),
 'failures':[c for c in report['checks'] if not c['passed']], 'source_bounds_m':source_bounds}
