"""Generate Arena Assault v4 sample assets.

Outputs:
- assets/meshes/enemy_body.aam2 : full rigid-weight skinned robot (8 bones)
- assets/meshes/enemy_body.aam  : AAM1 static fallback
- assets/textures/arena_atlas.tga

No third-party Python modules required.
"""

import math
import os
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
MESH2_OUT = os.path.join(ROOT, 'content','assets', 'meshes', 'enemy_body.aam2')
MESH1_OUT = os.path.join(ROOT, 'content','assets', 'meshes', 'enemy_body.aam')
ATLAS_OUT = os.path.join(ROOT, 'content','assets', 'textures', 'arena_atlas.tga')

BONE_ROOT = 0
BONE_TORSO = 1
BONE_HEAD = 2
BONE_ARM_L = 3
BONE_ARM_R = 4
BONE_LEG_L = 5
BONE_LEG_R = 6
BONE_WEAPON = 7
BONE_COUNT = 8


def write_aam1(path, vertices, indices):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(struct.pack('<4sIII', b'AAM1', 1, len(vertices), len(indices)))
        for v in vertices:
            f.write(struct.pack('<8f', *v))
        for i in indices:
            f.write(struct.pack('<I', i))


def write_aam2(path, vertices, indices):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(struct.pack('<4sIIII', b'AAM2', 2, len(vertices), len(indices), BONE_COUNT))
        for geom, bones, weights in vertices:
            f.write(struct.pack('<8f4I4f', *geom, *bones, *weights))
        for i in indices:
            f.write(struct.pack('<I', i))


def rigid_skin(bone):
    return (bone, 0, 0, 0), (1.0, 0.0, 0.0, 0.0)


def add_quad(v, ind, p0, p1, p2, p3, n, uv_rect=(0,0,1,1), bone=None):
    u0,v0,u1,v1 = uv_rect
    base = len(v)
    geoms = [(*p0,*n,u0,v0), (*p1,*n,u1,v0), (*p2,*n,u1,v1), (*p3,*n,u0,v1)]
    if bone is None:
        v.extend(geoms)
    else:
        bones, weights = rigid_skin(bone)
        v.extend((g,bones,weights) for g in geoms)
    ind.extend([base,base+1,base+2, base,base+2,base+3])


def add_box(v, ind, center, half, bone=None, uv_rect=(0,0,1,1)):
    cx,cy,cz=center; hx,hy,hz=half
    p=[
        (cx-hx,cy-hy,cz-hz),(cx+hx,cy-hy,cz-hz),(cx+hx,cy+hy,cz-hz),(cx-hx,cy+hy,cz-hz),
        (cx-hx,cy-hy,cz+hz),(cx+hx,cy-hy,cz+hz),(cx+hx,cy+hy,cz+hz),(cx-hx,cy+hy,cz+hz),
    ]
    faces=[(0,1,2,3,(0,0,-1)),(5,4,7,6,(0,0,1)),(4,0,3,7,(-1,0,0)),
           (1,5,6,2,(1,0,0)),(3,2,6,7,(0,1,0)),(4,5,1,0,(0,-1,0))]
    for a,b,c,d,n in faces:
        add_quad(v,ind,p[a],p[b],p[c],p[d],n,uv_rect,bone)


def add_oct_prism(v, ind, center_x, center_z, y0, y1, rx, rz, bone=None, sides=8):
    rings=[]
    for y in (y0,y1):
        ring=[]
        for i in range(sides):
            a=2*math.pi*i/sides + math.pi/8
            ring.append((center_x+math.cos(a)*rx, y, center_z+math.sin(a)*rz))
        rings.append(ring)
    for i in range(sides):
        j=(i+1)%sides
        p0,p1,p2,p3=rings[0][i],rings[0][j],rings[1][j],rings[1][i]
        mx=(p0[0]+p1[0])*0.5-center_x; mz=(p0[2]+p1[2])*0.5-center_z
        ln=math.hypot(mx,mz) or 1
        add_quad(v,ind,p0,p1,p2,p3,(mx/ln,0,mz/ln),(0,0,1,1),bone)
    for top,y,normal in ((False,y0,(0,-1,0)),(True,y1,(0,1,0))):
        center=(center_x,y,center_z)
        for i in range(sides):
            j=(i+1)%sides
            pts=(center,rings[1][i],rings[1][j]) if top else (center,rings[0][j],rings[0][i])
            base=len(v)
            raw=[]
            for p in pts:
                raw.append((*p,*normal,(p[0]-center_x)/(2*rx)+0.5,(p[2]-center_z)/(2*rz)+0.5))
            if bone is None:
                v.extend(raw)
            else:
                bones,weights=rigid_skin(bone)
                v.extend((g,bones,weights) for g in raw)
            ind.extend([base,base+1,base+2])


def build_enemy_aam2():
    v=[]; i=[]
    # torso / root shell
    add_oct_prism(v,i,0,0,0.83,1.76,0.47,0.31,BONE_TORSO,8)
    add_box(v,i,(0,1.31,-0.31),(0.32,0.28,0.055),BONE_TORSO)
    add_box(v,i,(0,1.27,0.31),(0.28,0.38,0.08),BONE_TORSO)       # backpack
    add_box(v,i,(0,0.74,0),(0.34,0.16,0.24),BONE_ROOT)           # pelvis
    add_box(v,i,(-0.34,1.56,0),(0.13,0.11,0.25),BONE_TORSO)      # collar L
    add_box(v,i,(0.34,1.56,0),(0.13,0.11,0.25),BONE_TORSO)       # collar R

    # head + antenna
    add_oct_prism(v,i,0,-0.01,1.74,2.22,0.32,0.30,BONE_HEAD,8)
    add_box(v,i,(0,2.27,0.02),(0.055,0.09,0.055),BONE_HEAD)
    add_box(v,i,(0,2.39,0.02),(0.025,0.055,0.025),BONE_HEAD)

    # left arm, one rigid bone segment for robust CPU skinning
    add_box(v,i,(-0.56,1.48,0),(0.20,0.15,0.23),BONE_ARM_L)
    add_box(v,i,(-0.57,1.17,-0.02),(0.14,0.30,0.15),BONE_ARM_L)
    add_box(v,i,(-0.57,0.91,-0.10),(0.12,0.18,0.13),BONE_ARM_L)
    # right arm
    add_box(v,i,(0.56,1.48,0),(0.20,0.15,0.23),BONE_ARM_R)
    add_box(v,i,(0.57,1.17,-0.02),(0.14,0.30,0.15),BONE_ARM_R)
    add_box(v,i,(0.57,0.91,-0.10),(0.12,0.18,0.13),BONE_ARM_R)

    # legs and feet
    add_box(v,i,(-0.23,0.46,0),(0.17,0.30,0.19),BONE_LEG_L)
    add_box(v,i,(-0.23,0.17,-0.02),(0.15,0.20,0.16),BONE_LEG_L)
    add_box(v,i,(-0.23,0.05,-0.14),(0.19,0.08,0.31),BONE_LEG_L)
    add_box(v,i,(0.23,0.46,0),(0.17,0.30,0.19),BONE_LEG_R)
    add_box(v,i,(0.23,0.17,-0.02),(0.15,0.20,0.16),BONE_LEG_R)
    add_box(v,i,(0.23,0.05,-0.14),(0.19,0.08,0.31),BONE_LEG_R)

    # rifle mesh on dedicated weapon bone
    add_box(v,i,(0.49,1.18,-0.48),(0.13,0.10,0.38),BONE_WEAPON)
    add_box(v,i,(0.49,1.18,-0.86),(0.060,0.060,0.20),BONE_WEAPON)
    add_box(v,i,(0.49,1.06,-0.33),(0.07,0.16,0.10),BONE_WEAPON)
    add_box(v,i,(0.49,1.29,-0.40),(0.06,0.05,0.13),BONE_WEAPON)

    write_aam2(MESH2_OUT,v,i)
    print('wrote',MESH2_OUT,len(v),'vertices',len(i)//3,'triangles',BONE_COUNT,'bones')


def build_enemy_aam1():
    v=[]; i=[]
    add_oct_prism(v,i,0,0,0.82,1.78,0.46,0.30,None,8)
    add_oct_prism(v,i,0,-0.01,1.74,2.22,0.31,0.29,None,8)
    add_box(v,i,(0,1.31,-0.29),(0.31,0.27,0.055),None)
    add_box(v,i,(-0.53,1.56,0),(0.18,0.16,0.22),None)
    add_box(v,i,(0.53,1.56,0),(0.18,0.16,0.22),None)
    add_box(v,i,(0,0.74,0),(0.33,0.16,0.23),None)
    write_aam1(MESH1_OUT,v,i)
    print('wrote',MESH1_OUT,len(v),'vertices',len(i)//3,'triangles')


def pixel(x,y):
    qx = 0 if x < 128 else 1
    qy = 0 if y < 128 else 1
    lx=x%128; ly=y%128
    noise=((x*17+y*31+(x*y)%23)%17)-8
    if qx==0 and qy==0:
        base=64+noise; seam=(lx%32<2 or ly%32<2)
        c=(base-8,base,base+10) if not seam else (28,34,42)
    elif qx==1 and qy==0:
        base=116+noise; seam=(lx%40<2 or ly%40<2)
        c=(base+18,base-18,base-30) if not seam else (52,31,27)
    elif qx==0 and qy==1:
        base=48+noise//2; seam=(lx%24<2 or ly%24<2)
        c=(base,base+8,base+12) if not seam else (20,62,67)
    else:
        line=(lx%16<3 or ly%16<3)
        c=(12,180,220) if line else (18+noise//2,34+noise//2,42+noise//2)
    return tuple(max(0,min(255,int(v))) for v in c)+(255,)


def build_tga():
    w=h=256
    os.makedirs(os.path.dirname(ATLAS_OUT),exist_ok=True)
    header=struct.pack('<BBBHHBHHHHBB',0,0,2,0,0,0,0,0,w,h,32,0x28)
    with open(ATLAS_OUT,'wb') as f:
        f.write(header)
        for y in range(h):
            for x in range(w):
                r,g,b,a=pixel(x,y)
                f.write(bytes((b,g,r,a)))
    print('wrote',ATLAS_OUT,w,'x',h)


if __name__=='__main__':
    build_enemy_aam2()
    build_enemy_aam1()
    build_tga()
