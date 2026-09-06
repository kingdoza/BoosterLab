"""Original bath prop, authored through Blender Lab's MCP execution bridge.
Run in an empty, agent-owned Blender session. Dimensions are in metres.
"""
import bpy
import math
import random
import json
from pathlib import Path
from mathutils import Vector
import numpy as np

ROOT = Path(r'C:\UnrealProjects\BathhouseSim\ArtSource\Bathhouse\Bath_01')
random.seed(29)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
for collection in list(bpy.data.collections):
    if collection.name != 'Collection':
        bpy.data.collections.remove(collection)
bpy.ops.outliner.orphans_purge(do_local_ids=True, do_linked_ids=False, do_recursive=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
asset_col = bpy.data.collections.new('BATH_01 | export geometry')
scene.collection.children.link(asset_col)
stage_col = bpy.data.collections.new('PRESENTATION | excluded from export')
scene.collection.children.link(stage_col)
collision_col = bpy.data.collections.new('COLLISION | simple convex pieces')
scene.collection.children.link(collision_col)
groups = {'body': [], 'metal': [], 'water': [], 'detail': []}

def move_to(obj, col):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    col.objects.link(obj)

def material(name, rgb, rough=.48, metal=0., tex=False):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*rgb, 1)
    mat.use_nodes = True
    p = mat.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = (*rgb,1)
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    if tex:
        size = 512
        y,x = np.mgrid[0:size,0:size].astype(np.float32)/size
        rng = np.random.default_rng(317)
        waves = np.zeros((size,size), dtype=np.float32)
        for freq,amp in [(2,.050),(5,.026),(13,.015),(39,.006)]:
            waves += amp*np.sin(x*freq*6.28+rng.random()*8)*np.cos(y*(freq+.4)*6.28+rng.random()*8)
        # Low-contrast glaze variation and fine brushed surface colour, baked to image.
        grain = rng.normal(0,.004,(size,size)).astype(np.float32)
        brushed = .007*np.sin(y*185+x*5)*np.sin(x*36)
        color = np.array(rgb, dtype=np.float32)[None,None,:] * (1+waves[:,:,None]+grain[:,:,None]+brushed[:,:,None])
        rgba=np.concatenate([np.clip(color,0,1),np.ones((size,size,1),np.float32)],axis=2)
        img=bpy.data.images.new(name+'_BaseColor', width=size,height=size)
        img.pixels.foreach_set(rgba.ravel())
        img.filepath_raw=str(ROOT/'textures'/(name+'_BaseColor.png'))
        img.file_format='PNG'
        img.save()
        node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=img
        mat.node_tree.links.new(node.outputs['Color'],p.inputs['Base Color'])
    return mat

grout=material('M_Grout_WarmGrey',(.24,.27,.23),.86)
cream=material('M_Coping_Ivory',(.76,.68,.49),.52,tex=True)
cream_light=material('M_Coping_Light',(.84,.77,.59),.48,tex=True)
teals=[material('M_Glaze_Jade_'+str(i+1),c,.32,tex=True) for i,c in enumerate([
    (.19,.40,.34),(.25,.46,.38),(.29,.49,.40),(.17,.35,.31),(.34,.52,.43)])]
insets=[material('M_Glaze_Celadon_'+str(i+1),c,.34,tex=True) for i,c in enumerate([
    (.46,.65,.55),(.52,.70,.59),(.39,.58,.49)])]
dark=material('M_Edge_DeepTeal',(.075,.19,.16),.5,tex=True)
brass=material('M_Metal_AgedBrass',(.42,.28,.105),.3,.77)
steel=material('M_Metal_SatinNickel',(.40,.48,.45),.23,.86)
red=material('M_Enamel_Oxblood',(.35,.065,.054),.38)
wear=material('M_Edge_ExposedClay',(.47,.38,.23),.82)
water_mat=material('M_Water_Turquoise',(.12,.43,.36),.13)
bs=water_mat.node_tree.nodes.get('Principled BSDF')
bs.inputs['Transmission Weight'].default_value=.68
bs.inputs['IOR'].default_value=1.333
bs.inputs['Coat Weight'].default_value=.32

def register(obj,name,mat,group='body',col=None):
    obj.name=name
    if mat:obj.data.materials.append(mat)
    move_to(obj,col or asset_col)
    if group:groups[group].append(obj)
    return obj

def bevel(obj,width=.015,segments=2):
    m=obj.modifiers.new('Soft handmade edges','BEVEL');m.width=width;m.segments=segments
    m=obj.modifiers.new('Weighted face normals','WEIGHTED_NORMAL');m.keep_sharp=True;m.weight=40
    return obj

def box(name,loc,size,mat,edge=.01,group='body',col=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    obj=bpy.context.object;obj.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    register(obj,name,mat,group,col)
    if edge:bevel(obj,edge,2)
    return obj

def mesh_obj(name,verts,faces,mat,group='body',edge=0):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);asset_col.objects.link(obj)
    if mat:mesh.materials.append(mat)
    groups[group].append(obj)
    if edge:bevel(obj,edge,2)
    return obj

def rr(w,h,r,n=7):
    pts=[]
    for cx,cy,a0 in [(w/2-r,h/2-r,0),(-w/2+r,h/2-r,90),(-w/2+r,-h/2+r,180),(w/2-r,-h/2+r,270)]:
        for j in range(n+1):
            a=math.radians(a0+j*90/n)
            pts.append((cx+r*math.cos(a),cy+r*math.sin(a)))
    return pts

def ring(name,outer,inner,z0,z1,mat,edge=.006):
    n=len(outer);verts=[(x,y,z) for pts,z in [(outer,z0),(outer,z1),(inner,z0),(inner,z1)] for x,y in pts]
    faces=[]
    for i in range(n):
        j=(i+1)%n
        faces += [(i,j,n+j,n+i),(2*n+j,2*n+i,3*n+i,3*n+j),
                  (n+i,n+j,3*n+j,3*n+i),(j,i,2*n+i,2*n+j)]
    return mesh_obj(name,verts,faces,mat,edge=edge)

def section(name,a,b,c,d,z0,z1,mat,edge=.008):
    pts=[a,b,c,d];v=[(x,y,z) for z in (z0,z1) for x,y in pts]
    f=[(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    return mesh_obj(name,v,f,mat,edge=edge)

# Raised basin: real hollow shell, not a closed box. Top opening remains traversable.
outer=rr(4.4,3.4,.27);inner=rr(3.66,2.66,.20)
ring('Structural basin walls',outer,inner,.13,1.0,grout)
box('Basin foundation',(0,0,.115),(4.15,3.15,.23),dark,.10)
box('Basin floor',(0,0,.21),(3.66,2.66,.19),grout,.045)
ring('Lower dark glazed plinth',rr(4.45,3.45,.28),rr(4.34,3.34,.24),.09,.24,dark)
ring('Thin ivory base bead',rr(4.44,3.44,.28),rr(4.35,3.35,.24),.255,.29,cream)

def wall_tiles(w,h,r,zmin,zmax,rows,mats,inward=False):
    # Straight courses and fitted curved corner tiles use the same pitch.
    for row in range(rows):
        z=zmin+(zmax-zmin)*(row+.5)/rows
        height=(zmax-zmin)/rows-.010
        for axis,length in [('x',w-2*r),('y',h-2*r)]:
            num=round(length/.255);pitch=length/num
            for side in (-1,1):
                for i in range(num):
                    along=-length/2+(i+.5)*pitch
                    offset=(-.009 if inward else .010)
                    if axis=='x':loc=(along,side*(h/2+offset),z);size=(pitch-.009,.032,height)
                    else:loc=(side*(w/2+offset),along,z);size=(.032,pitch-.009,height)
                    o=box('Glazed tile',loc,size,random.choice(mats),.010)
        for cx,cy,start in [(w/2-r,h/2-r,0),(-w/2+r,h/2-r,90),(-w/2+r,-h/2+r,180),(w/2-r,-h/2+r,270)]:
            for j in range(3):
                a=math.radians(start+j*30+1);b=math.radians(start+(j+1)*30-1)
                ra=r+(-.022 if inward else .026);rb=ra+.025
                points=[(cx+q*math.cos(t),cy+q*math.sin(t)) for q,t in [(rb,a),(rb,b),(ra,b),(ra,a)]]
                section('Corner glaze',*points,z-height/2,z+height/2,random.choice(mats),.004)

wall_tiles(4.4,3.4,.27,.30,.925,3,teals)
wall_tiles(3.66,2.66,.20,.32,.925,3,insets,True)
ring('Upper dark pinstripe',rr(4.445,3.445,.28),rr(4.37,3.37,.24),.921,.954,dark,.004)

# Broad ivory coping: individually jointed slabs and matching radial corner stones.
ring('Coping mortar',rr(4.56,3.56,.33),rr(3.53,2.53,.18),.96,1.065,grout)
for axis,length in [('x',3.44),('y',2.44)]:
    count=round(length/.43);pitch=length/count
    for side in (-1,1):
        for i in range(count):
            t=-length/2+(i+.5)*pitch
            loc=(t,side*1.526,1.055) if axis=='x' else (side*2.026,t,1.055)
            dims=(pitch-.012,.514,.15) if axis=='x' else (.514,pitch-.012,.15)
            box('Ivory coping slab',loc,dims,random.choice([cream,cream,cream_light]),.035)
for sx in (-1,1):
    for sy in (-1,1):
        box('Rounded corner coping',(sx*2.00,sy*1.50,1.055),(.55,.55,.15),cream_light,.105)

# Pale tiled basin floor and submerged seating ledges.
for ix in range(14):
    for iy in range(10):
        x=-1.70+ix*(3.40/13);y=-1.19+iy*(2.38/9)
        box('Floor tile',(x,y,.312),(.252,.254,.026),random.choice(insets),.007)
box('Back sitting bench',(0,.99,.44),(3.34,.57,.32),grout,.045)
for i in range(13):
    box('Bench seat tile',(-1.56+i*.26,.99,.61),(.248,.59,.07),random.choice(insets),.020)
box('Bench dark lip',(0,.699,.56),(3.33,.052,.064),dark,.012)

# Three clear exterior steps and three submerged steps at the front left.
for idx,(y,depth,z) in enumerate([(-2.19,.85,.27),(-1.99,.48,.54),(-1.81,.19,.81)]):
    box('Exterior step '+str(idx+1),(-.91,y,z/2+.08),(1.12,depth,z),grout,.025)
    box('Ivory step tread '+str(idx+1),(-.91,y,z+.10),(1.17,depth+.035,.09),cream,.025)
    for k in range(5):
        box('Step riser tile',(-1.354+k*.222,y-depth/2-.016,z/2+.08),(.211,.023,max(.05,z-.06)),random.choice(teals),.006)
for idx,(y,depth,z) in enumerate([(-1.14,.35,.85),(-.85,.33,.67),(-.55,.33,.49)]):
    box('Immersion step '+str(idx+1),(-.91,y,(z+.31)/2),(1.06,depth,z-.31),insets[0],.022)
    box('Immersion tread '+str(idx+1),(-.91,y,z),(1.08,depth+.015,.055),cream_light,.018)

def cylinder(name,a,b,r,mat,group='metal',vertices=16):
    a,b=Vector(a),Vector(b);delta=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=delta.length,location=(a+b)/2)
    obj=bpy.context.object;obj.rotation_euler=delta.to_track_quat('Z','Y').to_euler()
    register(obj,name,mat,group);bevel(obj,.004,2)
    for p in obj.data.polygons:p.use_smooth=True
    return obj

def tube(name,pts,r,mat,group='metal'):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.resolution_u=2 if group=='water' else 16
    curve.bevel_depth=r;curve.bevel_resolution=1 if group=='water' else 3
    spline=curve.splines.new('BEZIER');spline.bezier_points.add(len(pts)-1)
    for p,co in zip(spline.bezier_points,pts):p.co=co;p.handle_left_type='AUTO';p.handle_right_type='AUTO'
    obj=bpy.data.objects.new(name,curve);asset_col.objects.link(obj);curve.materials.append(mat);groups[group].append(obj)
    return obj

for x in (-1.48,-.34):
    tube('Continuous entrance handrail',[(x,-2.40,.36),(x,-2.39,1.05),(x,-2.25,1.20),(x,-1.48,1.73),(x,-1.20,1.77),(x,-.66,1.48),(x,-.60,.52)],.031,steel)
    for y,z in [(-2.40,.37),(-.60,.53)]:
        cylinder('Rail anchor flange',(x,y,z-.018),(x,y,z+.017),.075,brass)
        for dx,dy in [(.047,0),(-.047,0),(0,.047),(0,-.047)]:
            cylinder('Anchor screw',(x+dx,y+dy,z+.014),(x+dx,y+dy,z+.023),.009,steel,vertices=6)

# Rear hot-water supply with a compact, warm retro enamel temperature plate.
box('Fountain pedestal',(.98,1.52,1.28),(.67,.43,.51),dark,.06)
box('Fountain inset',(.98,1.291,1.29),(.55,.035,.37),cream,.035)
box('Fountain cap',(.98,1.51,1.562),(.75,.49,.10),cream_light,.035)
cylinder('Spout mounting boss',(.98,1.261,1.26),(.98,1.217,1.26),.088,brass)
tube('Hot water spout',[(.98,1.23,1.26),(.98,1.09,1.27),(.98,.97,1.20),(.98,.97,1.13)],.042,brass)
cylinder('Spout dark opening',(.98,.97,1.112),(.98,.97,1.12),.032,dark)
cylinder('Valve spindle',(1.195,1.253,1.40),(1.195,1.207,1.40),.026,brass)
cylinder('Oxblood valve bar',(1.11,1.197,1.40),(1.28,1.197,1.40),.018,red)
box('Front temperature badge',(.96,-1.741,.665),(.57,.045,.285),brass,.033,group='detail')
box('Badge enamel',(.96,-1.771,.665),(.514,.018,.239),cream_light,.025,group='detail')

def textmesh(name,text,loc,size,mat):
    curve=bpy.data.curves.new(name,'FONT');curve.body=text;curve.size=size;curve.extrude=.0006;curve.align_x='CENTER';curve.align_y='CENTER'
    obj=bpy.data.objects.new(name,curve);asset_col.objects.link(obj);obj.location=loc;obj.rotation_euler=(math.pi/2,0,0)
    curve.materials.append(mat);groups['detail'].append(obj)
textmesh('Temperature lettering','40 °C',(.96,-1.785,.701),.116,red)
textmesh('Bath number lettering','B A T H   0 1',(.96,-1.785,.606),.035,dark)

# Drain: visible through the water in the empty-basin view.
cylinder('Drain brass surround',(.88,-.62,.326),(.88,-.62,.339),.105,brass,'detail',32)
for i in range(6):
    box('Drain slot',(.88,-.686+i*.026,.342),(.14,.010,.004),dark,.003,'detail')

# Sparse irregular chips along the exterior glaze. Geometry carries these to FBX.
for i in range(45):
    x=random.uniform(-2.03,2.03);z=random.choice([.304,.506,.716,.918])+random.uniform(-.004,.004)
    if -.0<x<1.4 and .52<z<.82:continue
    width=random.uniform(.012,.043);h=random.uniform(.004,.013)
    mesh_obj('Small glaze edge chip',[(x,-1.729,z),(x+width,-1.729,z+.001),(x+width*.77,-1.730,z+h),(x+width*.22,-1.730,z+h*.7)],[(0,1,2,3)],wear)

# Independent water surface, fully removable for gameplay water replacement.
waterpts=rr(3.625,2.625,.19,12)
water=mesh_obj('Water surface',[(x,y,.865) for x,y in waterpts],[tuple(range(len(waterpts)))],water_mat,'water')
# Subtle geometric ripple arcs near the spout; separated from the structural body.
foam=material('M_Water_Ripple',(.46,.71,.60),.22)
for i in range(3):
    pts=[]
    radius=.15+i*.105
    for j in range(15):
        a=math.radians(195+j*135/14)
        pts.append((.98+radius*math.cos(a),.96+radius*.64*math.sin(a),.869))
    tube('Quiet ripple',pts,.0025,foam,'water')

def select_only(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.hide_set(False);o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]

def finish_group(key,name):
    objects=groups[key]
    select_only(objects)
    bpy.ops.object.convert(target='MESH')
    bpy.ops.object.join()
    obj=bpy.context.object;obj.name=name
    # Shared asset origin at floor centre, convenient placement and component replacement.
    scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.007)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.data.uv_layers.active.name='UV0'
    # Non-overlapping secondary UV channel for Unreal lightmaps.
    obj.data.uv_layers.new(name='UV1_Lightmap',do_init=True)
    obj.data.uv_layers.active_index=0
    return obj

body=finish_group('body','SM_Bath_01_Body')
hardware=finish_group('metal','SM_Bath_01_Hardware')
details=finish_group('detail','SM_Bath_01_Details')
water=finish_group('water','SM_Bath_01_Water')
asset_objects=[body,hardware,details,water]
for obj in asset_objects:
    obj['asset_authoring']='Original geometry; visual style reference: StylArts Stylized House Interior'
    obj['units']='metres'
    obj['fbx_export_axes']='forward -Y, up Z'

# Convex collision pieces preserve the opening and both staircases.
collisions=[]
def collision(loc,size):
    obj=box('UCX_SM_Bath_01_Body_'+str(len(collisions)).zfill(2),loc,size,None,0,None,collision_col)
    obj.hide_render=True;obj.display_type='WIRE';collisions.append(obj)
collision((0,0,.18),(4.12,3.12,.26))
collision((0,1.50,.665),(4.10,.36,.81));collision((0,-1.50,.665),(4.10,.36,.81))
collision((2.01,0,.665),(.36,2.70,.81));collision((-2.01,0,.665),(.36,2.70,.81))
collision((0,.99,.46),(3.34,.57,.32))
for y,d,z in [(-2.19,.85,.27),(-1.99,.48,.54),(-1.81,.19,.81)]:collision((-.91,y,(z+.18)/2),(1.17,d+.035,z+.09))
for y,d,z in [(-1.14,.35,.85),(-.85,.33,.67),(-.55,.33,.49)]:collision((-.91,y,(z+.31)/2),(1.08,d,z-.26))

# Export separate FBX components, keeping the water and collision independently usable.
def fbx(filename,objects):
    select_only(objects)
    bpy.ops.export_scene.fbx(filepath=str(ROOT/filename),use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
        bake_space_transform=False,use_mesh_modifiers=True,mesh_smooth_type='FACE',
        path_mode='COPY',embed_textures=True,add_leaf_bones=False)
fbx('SM_Bath_01_Body.fbx',[body]+collisions)
fbx('SM_Bath_01_Hardware.fbx',[hardware,details])
fbx('SM_Bath_01_Water.fbx',[water])
for obj in collisions:obj.hide_set(True)
select_only(asset_objects)
bpy.ops.export_scene.gltf(filepath=str(ROOT/'Bath_01.glb'),export_format='GLB',use_selection=True,
    export_apply=True,export_texcoords=True,export_normals=True,export_materials='EXPORT')

# Warm studio presentation, with a quiet tiled footing and no unrelated props.
groundmat=material('M_Stage_Stone',(.31,.34,.30),.87)
ground=box('Studio floor',(0,0,-.095),(200,200,.15),groundmat,0,None,stage_col)
def point_at(obj,p):obj.rotation_euler=(Vector(p)-obj.location).to_track_quat('-Z','Y').to_euler()
def area(name,loc,energy,color,size,target):
    data=bpy.data.lights.new(name,'AREA');data.energy=energy;data.color=color;data.shape='DISK';data.size=size
    obj=bpy.data.objects.new(name,data);stage_col.objects.link(obj);obj.location=loc;point_at(obj,target)
area('Key softbox',(-3,-4,7),1350,(1,.83,.64),5,(0,0,.4))
area('Cool fill',(4,-1,5),1000,(.70,.88,1),4,(0,0,.6))
area('Warm rim',(0,5,6),1800,(1,.89,.71),3,(0,0,.7))
scene.world.color=(.2,.2,.2)
scene.world.use_nodes=True
scene.world.node_tree.nodes.get('Background').inputs[0].default_value=(.28,.34,.30,1)
scene.world.node_tree.nodes.get('Background').inputs[1].default_value=.38
camdata=bpy.data.cameras.new('Bath asset camera');cam=bpy.data.objects.new('Bath asset camera',camdata);stage_col.objects.link(cam)
scene.camera=cam;cam.location=(6.4,-8.8,7.4);point_at(cam,(0,-.24,.62));camdata.type='ORTHO';camdata.ortho_scale=7.65
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=1400;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.view_settings.view_transform='AgX'
scene.render.film_transparent=False
scene.render.filepath=str(ROOT/'Bath_01_Hero.png')
select_only(asset_objects)
for screen in bpy.data.screens:
    for ar in screen.areas:
        if ar.type=='VIEW_3D':
            ar.spaces.active.region_3d.view_distance=8.0
            ar.spaces.active.region_3d.view_location=(0,0,.55)
            ar.spaces.active.shading.type='MATERIAL'
for img in bpy.data.images:
    if img.source=='FILE' or img.name.startswith('M_'):
        try:img.pack()
        except RuntimeError:pass
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Bath_01.blend'))
stats={}
for obj in asset_objects:
    obj.data.calc_loop_triangles()
    stats[obj.name]={'vertices':len(obj.data.vertices),'triangles':len(obj.data.loop_triangles),
                     'materials':len(obj.data.materials),'uv_channels':[u.name for u in obj.data.uv_layers]}
stats['collision_hulls']=len(collisions)
stats['nominal_basin_dimensions_m']=[4.56,3.56,1.13]
stats['overall_depth_with_steps_m']=4.415
(ROOT/'asset_manifest.json').write_text(json.dumps(stats,indent=2),encoding='utf-8')
bpy.ops.render.render(write_still=True)
water.hide_render=True
cam.location=(5.8,-7.8,10.6);point_at(cam,(0,-.15,.40));camdata.ortho_scale=7.2
scene.render.filepath=str(ROOT/'Bath_01_Empty.png')
bpy.ops.render.render(write_still=True)
water.hide_render=False
cam.location=(6.4,-8.8,7.4);point_at(cam,(0,-.24,.62));camdata.ortho_scale=7.65
scene.render.filepath=str(ROOT/'Bath_01_Hero.png')
result={'saved':str(ROOT/'Bath_01.blend'),'statistics':stats,'previews':['Bath_01_Hero.png','Bath_01_Empty.png']}
