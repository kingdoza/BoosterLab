"""Deterministic painted glaze textures and face-local mapping for the separate variant."""
import bpy
import numpy as np
import zlib
from pathlib import Path

ROOT=Path(r'C:\UnrealProjects\BathhouseSim\ArtSource\Bathhouse\Bath_01_Painterly')

def noise(x,y,nx,ny,seed):
    rng=np.random.default_rng(seed)
    grid=rng.uniform(-1,1,(ny+1,nx+1)).astype(np.float32)
    xx=np.clip(x*nx,0,nx-.0001);yy=np.clip(y*ny,0,ny-.0001)
    ix=xx.astype(int);iy=yy.astype(int)
    tx=xx-ix;ty=yy-iy
    tx=tx*tx*(3-2*tx);ty=ty*ty*(3-2*ty)
    return (grid[iy,ix]*(1-tx)+grid[iy,ix+1]*tx)*(1-ty)+(grid[iy+1,ix]*(1-tx)+grid[iy+1,ix+1]*tx)*ty

def image(name,values,noncolor=False):
    h,w=values.shape[:2]
    if values.ndim==2:values=np.repeat(values[:,:,None],3,axis=2)
    rgba=np.concatenate([np.clip(values,0,1),np.ones((h,w,1),np.float32)],axis=2).astype(np.float32)
    img=bpy.data.images.new(name,width=w,height=h)
    if noncolor:img.colorspace_settings.name='Non-Color'
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw=str(ROOT/'textures'/(name+'.png'));img.file_format='PNG';img.save()
    return img

def material(name,rgb,rough=.48,metal=0.,tex=False):
    name='M_PT_'+name.removeprefix('M_')
    mat=bpy.data.materials.new(name);mat.use_nodes=True
    mat.diffuse_color=(*rgb,1)
    p=mat.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*rgb,1)
    p.inputs['Roughness'].default_value=rough
    p.inputs['Metallic'].default_value=metal
    if not tex:
        if 'SatinNickel' in name:p.inputs['Roughness'].default_value=.34
        if 'AgedBrass' in name:p.inputs['Roughness'].default_value=.42
        return mat
    size=1024
    y,x=np.mgrid[0:size,0:size].astype(np.float32)/(size-1)
    seed=zlib.crc32(name.encode())
    rng=np.random.default_rng(seed)
    broad=noise(x,y,4,5,seed)
    brush=noise(x,y,54,5,seed+1)
    grain=noise(x,y,180,28,seed+2)
    broken=noise(x,y,23,19,seed+3)
    fine=noise(x,y,108,97,seed+4)
    is_coping='Coping' in name
    is_dark='DeepTeal' in name
    base=np.array(rgb,np.float32)*(.88 if not is_coping else .91)
    # Broad hue and value shifts, then elongated dry-brush strokes.
    field=.22*broad+.11*brush+.023*grain
    color=base[None,None,:]*(1+field[:,:,None])
    hue=np.stack([.025*broad,-.009*broad,-.028*broad],axis=2)
    color+=hue
    for i in range(19):
        cx,cy=rng.uniform(.03,.97,2);wx=rng.uniform(.018,.075);hy=rng.uniform(.09,.35)
        mask=np.clip(1-np.abs(x-cx)/wx,0,1)*np.clip(1-np.abs(y-cy)/hy,0,1)
        mask*=np.clip((grain+.5)*1.5,0,1)
        color+=mask[:,:,None]*rng.uniform(-.055,.058)
    # Irregular ageing at tile borders: recessed darker glaze and intermittent exposed clay.
    edge=np.minimum(np.minimum(x,1-x),np.minimum(y,1-y))
    edge_zone=np.clip(1-edge/(.055+.020*broken),0,1)
    color*=1-.19*edge_zone[:,:,None]
    width=.008+np.maximum(broken,0)*.029+np.maximum(fine,0)*.008
    chips=np.clip((width-edge)*170,0,1)*np.clip((broken+.14)*3.2,0,1)
    clay=np.array((.43,.32,.19) if is_coping else (.39,.33,.22),np.float32)
    color=color*(1-chips[:,:,None]*.80)+clay[None,None,:]*chips[:,:,None]*.80
    # Broken pale wear lines, following the perimeter but never a uniform drawn border.
    worn=np.clip(1-np.abs(edge-(.021+.012*broken))/.008,0,1)*np.clip(broken*1.7,0,1)
    color+=worn[:,:,None]*(.07 if is_coping else .042)
    if is_coping:
        # Wide, quiet rubbed patches and a few shallow irregular brush scuffs.
        color+=np.maximum(broad,0)[:,:,None]*np.array([.07,.06,.045])
        for i in range(7):
            cx,cy=rng.uniform(.1,.9,2);w=rng.uniform(.12,.32)
            stroke=np.clip(1-np.abs(x-cx)/w,0,1)*np.clip(1-np.abs(y-cy+.009*brush)/.0028,0,1)
            color-=stroke[:,:,None]*.032
    if is_dark:color=np.maximum(color,.013)
    albedo=image(name+'_BaseColor',color)
    roughness=np.clip((.55 if is_coping else .42)+.09*broad+.05*brush+.12*edge_zone+.16*chips,.25,.83)
    rough_img=image(name+'_Roughness',roughness,True)
    nodes=mat.node_tree.nodes;links=mat.node_tree.links
    uv=nodes.new('ShaderNodeUVMap');uv.uv_map='UV0'
    for image_data,input_name in [(albedo,'Base Color'),(rough_img,'Roughness')]:
        node=nodes.new('ShaderNodeTexImage');node.image=image_data
        links.new(uv.outputs['UV'],node.inputs['Vector'])
        links.new(node.outputs['Color'],p.inputs[input_name])
    return mat

def prepare_uv(objects):
    """Map each original primitive face to a full texture before bevel evaluation.
    The old atlas made every tile sample a tiny patch and hid the painted marks.
    """
    for obj in objects:
        if obj.type!='MESH':continue
        mesh=obj.data
        while mesh.uv_layers:mesh.uv_layers.remove(mesh.uv_layers[0])
        layer=mesh.uv_layers.new(name='UV0')
        for polygon in mesh.polygons:
            normal=polygon.normal
            axis=max(range(3),key=lambda k:abs(normal[k]))
            a,b={0:(1,2),1:(0,2),2:(0,1)}[axis]
            coords=[mesh.vertices[mesh.loops[li].vertex_index].co for li in polygon.loop_indices]
            lo=[min(c[k] for c in coords) for k in (a,b)]
            hi=[max(c[k] for c in coords) for k in (a,b)]
            for li,c in zip(polygon.loop_indices,coords):
                layer.data[li].uv=((c[a]-lo[0])/max(hi[0]-lo[0],1e-6),(c[b]-lo[1])/max(hi[1]-lo[1],1e-6))
