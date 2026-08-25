"""Offline structural validation for Arena Assault AAM1/AAM2 and TGA assets."""
import os
import struct

ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),'..'))


def validate_aam1(path):
    data=open(path,'rb').read()
    magic,version,vc,ic=struct.unpack_from('<4sIII',data,0)
    assert magic==b'AAM1' and version==1 and vc>0 and ic>=3 and ic%3==0
    expected=16+vc*32+ic*4
    assert len(data)==expected, (len(data),expected)
    off=16+vc*32
    inds=struct.unpack_from('<%dI'%ic,data,off)
    assert max(inds)<vc
    return vc,ic//3


def validate_aam2(path):
    data=open(path,'rb').read()
    magic,version,vc,ic,bc=struct.unpack_from('<4sIIII',data,0)
    assert magic==b'AAM2' and version==2 and 0<bc<=16 and vc>0 and ic>=3 and ic%3==0
    expected=20+vc*64+ic*4
    assert len(data)==expected, (len(data),expected)
    off=20
    for _ in range(vc):
        vals=struct.unpack_from('<8f4I4f',data,off)
        bones=vals[8:12]; weights=vals[12:16]
        assert max(bones)<bc
        assert all(w>=0 for w in weights)
        assert abs(sum(weights)-1.0)<1e-4
        off+=64
    inds=struct.unpack_from('<%dI'%ic,data,off)
    assert max(inds)<vc
    return vc,ic//3,bc


def validate_tga(path):
    data=open(path,'rb').read()
    assert len(data)>=18
    image_type=data[2]; w=data[12]|(data[13]<<8); h=data[14]|(data[15]<<8); bits=data[16]
    assert image_type==2 and bits==32 and w>0 and h>0
    assert len(data)>=18+w*h*4
    return w,h


if __name__=='__main__':
    a1=validate_aam1(os.path.join(ROOT,'content','assets','meshes','enemy_body.aam'))
    a2=validate_aam2(os.path.join(ROOT,'content','assets','meshes','enemy_body.aam2'))
    t=validate_tga(os.path.join(ROOT,'content','assets','textures','arena_atlas.tga'))
    print('AAM1:',a1[0],'vertices,',a1[1],'triangles')
    print('AAM2:',a2[0],'vertices,',a2[1],'triangles,',a2[2],'bones')
    print('TGA:',t[0],'x',t[1])
    print('OK')
